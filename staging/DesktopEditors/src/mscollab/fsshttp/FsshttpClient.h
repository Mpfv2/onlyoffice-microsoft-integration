#pragma once
#include "FsshttpSession.h"
#include "FsshttpSerializer.h"
#include "OoxmlDocument.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <cstdint>
#include <functional>
#include <thread>
#include <atomic>

// Top-level MS-FSSHTTP client.
// Call joinSession() to enter co-authoring. A 30s heartbeat and a 5s GetChanges
// poll run on background threads. Set onRemoteDelta before calling joinSession().
class FsshttpClient {
public:
    FsshttpClient(const std::string& fileUrl,
                  const std::string& clientId,
                  std::function<std::string()> tokenProvider);
    ~FsshttpClient();

    bool joinSession();
    void exitSession();

    // Load the local .docx before joinSession to enable full OOXML sync.
    // Falls back to stub paragraph XML if not called.
    void loadDocument(const std::string& localDocxPath);

    // Queues the delta if not joined; sends immediately when joined.
    // Queued deltas are flushed automatically on successful re-join.
    void sendDelta(const std::string& deltaJson);

    // Set before calling joinSession(). Called on background threads.
    std::function<void(const std::string& deltaJson)> onRemoteDelta;
    std::function<void()> onSessionDropped;

    bool isJoined() const;

private:
    FsshttpSession    m_session;
    FsshttpSerializer m_serializer;
    OoxmlDocument     m_document;
    std::function<std::string()> m_tokenProvider;

    std::thread       m_heartbeatThread;
    std::thread       m_pollThread;
    std::atomic<bool> m_running{false};

    // Deltas queued while session is not joined (network loss, pre-join).
    std::queue<std::string> m_pendingDeltas;
    std::mutex              m_deltaMutex;

    // Last knowledge token from the server; used in subsequent GetChanges calls
    // so the server only returns changes made after the last poll.
    std::string m_knowledgeB64;
    std::mutex  m_knowledgeMutex;

    // Protects m_document: read in pollLoop thread, written from caller thread.
    std::mutex  m_documentMutex;

    std::string post(const std::string& xml);
    std::string endpointUrl() const;
    void        heartbeatLoop();  // 30s refresh cycle
    void        pollLoop();       // 5s GetChanges cycle
    void        pollGetChanges();
    void        flushPendingDeltas();
    void        sendDeltaImmediate(const std::string& deltaJson);

    static std::vector<uint8_t> deltaToOoxmlStub(const std::string& deltaJson);
    static std::string ooxmlToDeltaJsonStub(const std::vector<uint8_t>& ooxml);
};
