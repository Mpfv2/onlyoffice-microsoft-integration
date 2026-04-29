#include "IntegrationBridge.h"
#include <iostream>

// Placeholder — replaced in Task 10 when config.h is created
static const std::string PLACEHOLDER_CLIENT_ID = "REPLACE_WITH_AZURE_CLIENT_ID";
static const std::string TENANT = "techcollege.dk";

IntegrationBridge::IntegrationBridge()
    : m_auth(TENANT, PLACEHOLDER_CLIENT_ID) {}

bool IntegrationBridge::isOneDrivePath(const std::string& path) const {
    return path.find("OneDrive") != std::string::npos ||
           path.find("sharepoint.com") != std::string::npos;
}

// Returns the path directly for now.
// TODO (Phase 1 completion): resolve via Graph API GET /me/drive/root:/path
// to get the item's webUrl for the MS-FSSHTTP endpoint.
std::string IntegrationBridge::resolveOneDriveUrl(const std::string& localPath) const {
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
