#pragma once
#include "FsshttpSession.h"
#include "FsshttpSerializer.h"
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

    // Called by IntegrationBridge when the user makes a local edit.
    void sendDelta(const std::string& deltaJson);

    // Set before calling joinSession(). Called on the heartbeat thread.
    std::function<void(const std::string& deltaJson)> onRemoteDelta;
    std::function<void()> onSessionDropped;

    bool isJoined() const;

private:
    FsshttpSession    m_session;
    FsshttpSerializer m_serializer;
    std::function<std::string()> m_tokenProvider;

    std::thread       m_heartbeatThread;
    std::atomic<bool> m_running{false};

    std::string post(const std::string& xml);
    std::string endpointUrl() const;
    void        heartbeatLoop();
    void        pollGetChanges();

    // Minimal OOXML paragraph for a deltaJson {paragraphIndex,content}.
    static std::vector<uint8_t> deltaToOoxml(const std::string& deltaJson);
    // Extract text from an OOXML blob and return it as a deltaJson.
    static std::string ooxmlToDeltaJson(const std::vector<uint8_t>& ooxml);
};
