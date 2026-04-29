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

void IntegrationBridge::onDocumentOpened(const std::string& filePath) {
    if (!isOneDrivePath(filePath)) return;

    if (!m_auth.isAuthenticated()) {
        if (!m_auth.authenticate()) {
            std::cerr << "[MsCollab] Authentication failed — local only mode\n";
            return;
        }
    }

    std::string fileUrl  = resolveOneDriveUrl(filePath);
    std::string clientId = "onlyoffice-" +
        std::to_string(std::hash<std::string>{}(filePath));

    m_client = std::make_unique<FsshttpClient>(
        fileUrl, clientId,
        [this]() { return m_auth.accessToken(); });

    m_client->onRemoteDelta = [this](const std::string& delta) {
        if (sendToJs) sendToJs(delta);
    };

    m_client->onSessionDropped = []() {
        std::cerr << "[MsCollab] Session dropped\n";
    };

    if (!m_client->joinSession()) {
        std::cerr << "[MsCollab] Failed to join coauthoring session\n";
        m_client.reset();
    }
}

void IntegrationBridge::onDocumentClosed(const std::string& /*filePath*/) {
    if (m_client) {
        m_client->exitSession();
        m_client.reset();
    }
}

void IntegrationBridge::onOutgoingChange(const std::string& deltaJson) {
    if (!m_client || !m_client->isJoined()) return;
    m_client->sendDelta(deltaJson);
}
