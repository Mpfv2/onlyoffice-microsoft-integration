#include "FsshttpSession.h"

FsshttpSession::FsshttpSession(const std::string& fileUrl, const std::string& clientId)
    : m_fileUrl(fileUrl), m_clientId(clientId) {}

FsshttpSession::State FsshttpSession::state() const        { return m_state; }
std::string FsshttpSession::sessionToken() const           { return m_sessionToken; }
std::string FsshttpSession::storageToken() const           { return m_storageToken; }
std::string FsshttpSession::fileUrl() const                { return m_fileUrl; }
std::string FsshttpSession::clientId() const               { return m_clientId; }
int         FsshttpSession::retryCount() const             { return m_retryCount; }
void        FsshttpSession::setStorageToken(const std::string& t) { m_storageToken = t; }

void FsshttpSession::setJoining() {
    m_state = State::Joining;
}

void FsshttpSession::handleJoinResponse(bool success, const std::string& token) {
    if (success) {
        m_sessionToken = token;
        m_state = State::Joined;
        m_retryCount = 0;
    } else {
        m_sessionToken.clear();
        m_state = State::Disconnected;
    }
}

void FsshttpSession::handleRefreshSuccess() {
    m_retryCount = 0;
}

void FsshttpSession::handleRefreshFailure() {
    ++m_retryCount;
    if (m_retryCount >= MAX_RETRIES) {
        m_state = State::Disconnected;
        m_sessionToken.clear();
    }
}

void FsshttpSession::handleExit() {
    m_state = State::Disconnected;
    m_sessionToken.clear();
    m_retryCount = 0;
}
