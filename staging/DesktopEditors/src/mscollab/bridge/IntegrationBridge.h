#pragma once
#include "../auth/AuthModule.h"
#include "../fsshttp/FsshttpClient.h"
#include "../graph/GraphApiClient.h"
#include "../merge/MergeEngine.h"
#include <string>
#include <memory>
#include <functional>

// Owns the FsshttpClient, AuthModule, and GraphApiClient.
// Connects OnlyOffice document events to the MS-FSSHTTP protocol layer.
class IntegrationBridge {
public:
    IntegrationBridge();

    // Called when user opens a file detected as a local OneDrive path.
    // Resolves the SharePoint URL via Graph API, then joins a co-auth session.
    void onDocumentOpened(const std::string& localFilePath);

    // Called by the OneDrive toolbar dialog after downloading a temp file.
    // webUrl is the SharePoint URL (for MS-FSSHTTP); localPath is the temp .docx file.
    void openFromOneDrive(const std::string& webUrl, const std::string& localPath);

    // Called when user closes a document.
    void onDocumentClosed(const std::string& filePath);

    // Called by the Qt <-> JS event bus when the JS shim fires onOutgoingChange.
    void onOutgoingChange(const std::string& deltaJson);

    // Set by CAscApplicationManager to push incoming changes into sdkjs.
    std::function<void(const std::string& deltaJson)> sendToJs;

    // Exposed for toolbar action to construct OneDriveDialog with a token provider.
    AuthModule& authModule() { return m_auth; }

private:
    AuthModule                     m_auth;
    GraphApiClient                 m_graph;
    std::unique_ptr<FsshttpClient> m_client;
    MergeEngine                    m_merge;
    MergeEngine::Delta             m_lastLocalDelta;  // for conflict detection
    std::string                    m_localPath;       // set by startSession for uploadFile
    std::string                    m_onedriveFolderName;

    bool        isOneDrivePath(const std::string& path) const;
    std::string resolveOneDriveUrl(const std::string& localPath) const;
    void        startSession(const std::string& webUrl, const std::string& localPath);

    // Parse paragraphIndex+content from a deltaJson string.
    static MergeEngine::Delta parseDelta(const std::string& deltaJson);
    // Serialize a MergeEngine::Delta to deltaJson.
    static std::string serializeDelta(const MergeEngine::Delta& d);
};
