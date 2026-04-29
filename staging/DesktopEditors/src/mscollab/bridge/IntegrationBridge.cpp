#include "IntegrationBridge.h"
#include "../config.h"
#include <iostream>

IntegrationBridge::IntegrationBridge()
    : m_auth(MsCollabConfig::TENANT, MsCollabConfig::CLIENT_ID)
    , m_graph([this]() { return m_auth.accessToken(); }) {}

bool IntegrationBridge::isOneDrivePath(const std::string& path) const {
    return path.find("OneDrive") != std::string::npos ||
           path.find("sharepoint.com") != std::string::npos;
}

std::string IntegrationBridge::resolveOneDriveUrl(const std::string& localPath) const {
    std::string webUrl = m_graph.resolveWebUrl(localPath, MsCollabConfig::ONEDRIVE_FOLDER);
    if (!webUrl.empty()) return webUrl;
    // If Graph API lookup fails, fall back to local path (session join will fail but
    // the auth/merge layers remain intact for when connectivity is restored).
    std::cerr << "[MsCollab] Graph resolveWebUrl failed, using local path as fallback\n";
    return localPath;
}

MergeEngine::Delta IntegrationBridge::parseDelta(const std::string& j) {
    MergeEngine::Delta d;
    auto pi = j.find("\"paragraphIndex\":");
    if (pi != std::string::npos) d.paragraphIndex = std::stoi(j.substr(pi + 17));
    auto ci = j.find("\"content\":");
    if (ci != std::string::npos) {
        auto q1 = j.find('"', ci + 10);
        if (q1 != std::string::npos) {
            auto q2 = j.find('"', q1 + 1);
            if (q2 != std::string::npos) d.content = j.substr(q1 + 1, q2 - q1 - 1);
        }
    }
    return d;
}

std::string IntegrationBridge::serializeDelta(const MergeEngine::Delta& d) {
    return "{\"paragraphIndex\":" + std::to_string(d.paragraphIndex) +
           ",\"content\":\"" + d.content + "\"}";
}

void IntegrationBridge::startSession(const std::string& webUrl,
                                      const std::string& localPath) {
    m_localPath = localPath;
    std::string clientId = "onlyoffice-" + std::to_string(std::hash<std::string>{}(webUrl));
    m_client = std::make_unique<FsshttpClient>(
        webUrl, clientId, [this]() { return m_auth.accessToken(); });

    m_client->onRemoteDelta = [this](const std::string& d) {
        auto remote = parseDelta(d);
        auto result = m_merge.merge(m_lastLocalDelta, remote);
        if (result.action == MergeEngine::Action::Discard) return;
        if (result.action == MergeEngine::Action::TrackedChange)
            result.content = "TRACKED:" + result.content;
        if (sendToJs) sendToJs(serializeDelta({result.paragraphIndex, result.content}));
    };

    m_client->onSessionDropped = []() { std::cerr << "[MsCollab] Session dropped\n"; };
    if (!localPath.empty()) m_client->loadDocument(localPath);
    if (!m_client->joinSession()) {
        std::cerr << "[MsCollab] Failed to join coauthoring session\n";
        m_client.reset();
    }
}

void IntegrationBridge::onDocumentOpened(const std::string& filePath) {
    if (!isOneDrivePath(filePath)) return;
    if (!m_auth.isAuthenticated() && !m_auth.authenticate()) {
        std::cerr << "[MsCollab] Authentication failed — local only mode\n";
        return;
    }
    startSession(resolveOneDriveUrl(filePath), filePath);
}

void IntegrationBridge::openFromOneDrive(const std::string& webUrl,
                                          const std::string& localPath) {
    if (!m_auth.isAuthenticated() && !m_auth.authenticate()) {
        std::cerr << "[MsCollab] Authentication failed — local only mode\n";
        return;
    }
    startSession(webUrl, localPath);
}

void IntegrationBridge::onDocumentClosed(const std::string& /*filePath*/) {
    if (m_client) {
        m_client->exitSession();
        m_client.reset();
        // Best-effort upload: ensures OneDrive has the latest copy even if FSSHTTP
        // missed final deltas (e.g. PutChanges inflight at session exit).
        if (!m_localPath.empty()) {
            if (!m_graph.uploadFile(m_localPath, MsCollabConfig::ONEDRIVE_FOLDER))
                std::cerr << "[MsCollab] Final OneDrive upload failed (non-fatal)\n";
            m_localPath.clear();
        }
    }
}

void IntegrationBridge::onOutgoingChange(const std::string& deltaJson) {
    m_lastLocalDelta = parseDelta(deltaJson);
    if (!m_client) return;
    m_client->sendDelta(deltaJson);  // queued internally if not yet joined
}
