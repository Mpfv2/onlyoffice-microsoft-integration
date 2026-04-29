#pragma once
#include "../auth/AuthModule.h"
#include "../fsshttp/FsshttpClient.h"
#include "../merge/MergeEngine.h"
#include <string>
#include <memory>
#include <functional>

// Owns the FsshttpClient and AuthModule.
// Connects OnlyOffice document events to the MS-FSSHTTP protocol layer.
class IntegrationBridge {
public:
    IntegrationBridge();

    // Called when user opens a file. If it's a OneDrive path, starts co-auth session.
    void onDocumentOpened(const std::string& filePath);

    // Called when user closes a document.
    void onDocumentClosed(const std::string& filePath);

    // Called by the Qt <-> JS event bus when the JS shim fires onOutgoingChange.
    void onOutgoingChange(const std::string& deltaJson);

    // Set by CAscApplicationManager to push incoming changes into sdkjs.
    std::function<void(const std::string& deltaJson)> sendToJs;

private:
    AuthModule                     m_auth;
    std::unique_ptr<FsshttpClient> m_client;
    MergeEngine                    m_merge;

    bool        isOneDrivePath(const std::string& path) const;
    std::string resolveOneDriveUrl(const std::string& localPath) const;
};
