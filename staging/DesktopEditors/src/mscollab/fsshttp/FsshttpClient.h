#pragma once
#include "FsshttpSession.h"
#include "FsshttpSerializer.h"
#include "OoxmlDocument.h"
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>

// Top-level MS-FSSHTTP client.
// Call joinSession() to enter co-authoring. Heartbeat runs on a background thread.
// Set onRemoteDelta to receive incoming changes.
class FsshttpClient {
public:
    FsshttpClient(const std::string& fileUrl,
                  const std::string& clientId,
                  std::function<std::string()> tokenProvider);
    ~FsshttpClient();

    bool joinSession();
    void exitSession();

    // Load the local .docx before or after joinSession to enable full OOXML sync.
    // If not called, sendDelta falls back to stub paragraph XML.
    void loadDocument(const std::string& localDocxPath);

    // Called by IntegrationBridge when the user makes a local edit.
    void sendDelta(const std::string& deltaJson);

    // Set before calling joinSession(). Called on the heartbeat thread.
    std::function<void(const std::string& deltaJson)> onRemoteDelta;
    std::function<void()> onSessionDropped;

    bool isJoined() const;

private:
    FsshttpSession    m_session;
    FsshttpSerializer m_serializer;
    OoxmlDocument     m_document;
    std::function<std::string()> m_tokenProvider;

    std::thread       m_heartbeatThread;
    std::atomic<bool> m_running{false};

    std::string post(const std::string& xml);
    std::string endpointUrl() const;
    void        heartbeatLoop();
    void        pollGetChanges();

    // Fallback: build stub paragraph XML when m_document is not loaded.
    static std::vector<uint8_t> deltaToOoxmlStub(const std::string& deltaJson);
    static std::string ooxmlToDeltaJsonStub(const std::vector<uint8_t>& ooxml);
};
