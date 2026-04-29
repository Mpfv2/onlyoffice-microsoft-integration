#pragma once
#include "FsshttpSession.h"
#include "FsshttpSerializer.h"
#include <string>
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
};
