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

// Read a JSON string value starting after the opening quote, handling escapes.
static std::string readJsonString(const std::string& j, size_t start) {
    std::string out;
    for (size_t i = start; i < j.size(); ++i) {
        char c = j[i];
        if (c == '\\' && i + 1 < j.size()) {
            char nc = j[++i];
            if      (nc == '"')  out += '"';
            else if (nc == '\\') out += '\\';
            else if (nc == 'n')  out += '\n';
            else if (nc == 'r')  out += '\r';
            else if (nc == 't')  out += '\t';
            else                 out += nc;
        } else if (c == '"') {
            break;
        } else {
            out += c;
        }
    }
    return out;
}

MergeEngine::Delta IntegrationBridge::parseDelta(const std::string& j) {
    MergeEngine::Delta d;
    auto pi = j.find("\"paragraphIndex\":");
    if (pi != std::string::npos) {
        try { d.paragraphIndex = std::stoi(j.substr(pi + 17)); } catch (...) {}
    }
    auto ci = j.find("\"content\":");
    if (ci != std::string::npos) {
        auto q1 = j.find('"', ci + 10);
        if (q1 != std::string::npos)
            d.content = readJsonString(j, q1 + 1);
    }
    return d;
}

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

std::string IntegrationBridge::serializeDelta(const MergeEngine::Delta& d) {
    return "{\"paragraphIndex\":" + std::to_string(d.paragraphIndex) +
           ",\"content\":\"" + jsonEscape(d.content) + "\"}";
}

void IntegrationBridge::startSession(const std::string& webUrl,
                                      const std::string& localPath) {
    m_localPath = localPath;
    std::string clientId = "onlyoffice-" + std::to_string(std::hash<std::string>{}(webUrl));
    m_client = std::make_unique<FsshttpClient>(
        webUrl, clientId, [this]() { return m_auth.accessToken(); });

    m_client->onRemoteDelta = [this](const std::string& d) {
        // Comment deltas carry their own structured JSON — bypass merge and
        // forward directly so the JS shim can call asc_AddComment.
        if (d.find("\"type\":\"comment\"") != std::string::npos) {
            if (sendToJs) sendToJs(d);
            return;
        }
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
    // Only track paragraph deltas for merge comparison — comment deltas have no
    // paragraphIndex and would poison m_lastLocalDelta with a default {0,""}.
    if (deltaJson.find("\"type\":\"comment\"") == std::string::npos)
        m_lastLocalDelta = parseDelta(deltaJson);
    if (!m_client) return;
    m_client->sendDelta(deltaJson);  // queued internally if not yet joined
}
