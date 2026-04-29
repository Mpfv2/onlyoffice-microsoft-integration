#pragma once
#include <string>

// Tracks coauthoring session state.
// All state transitions happen via handle*() methods.
class FsshttpSession {
public:
    enum class State { Disconnected, Joining, Joined, Exiting };

    FsshttpSession(const std::string& fileUrl, const std::string& clientId);

    State       state() const;
    std::string sessionToken() const;
    std::string storageToken() const;
    std::string fileUrl() const;
    std::string clientId() const;
    int         retryCount() const;

    void handleJoinResponse(bool success, const std::string& token);
    void handleRefreshSuccess();
    void handleRefreshFailure();
    void handleExit();
    void setJoining();
    void setStorageToken(const std::string& token);

    static constexpr int MAX_RETRIES = 3;

private:
    std::string m_fileUrl;
    std::string m_clientId;
    std::string m_sessionToken;
    std::string m_storageToken;  // opaque token from JoinResponse, needed for CellSubRequests
    State       m_state = State::Disconnected;
    int         m_retryCount = 0;
};
