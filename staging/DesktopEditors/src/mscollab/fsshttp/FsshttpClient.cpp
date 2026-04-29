#include "FsshttpClient.h"
#include <curl/curl.h>
#include <chrono>
#include <thread>

static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

FsshttpClient::FsshttpClient(const std::string& fileUrl,
                             const std::string& clientId,
                             std::function<std::string()> tokenProvider)
    : m_session(fileUrl, clientId), m_tokenProvider(std::move(tokenProvider)) {}

FsshttpClient::~FsshttpClient() {
    exitSession();
}

// OneDrive for Business URLs: https://{tenant}-my.sharepoint.com/personal/{upn}/...
// MS-FSSHTTP endpoint is at the host root + /_vti_bin/vti_aut/author.dll
std::string FsshttpClient::endpointUrl() const {
    const auto& url = m_session.fileUrl();
    auto slash = url.find('/', 8);  // skip https://
    auto host = url.substr(0, slash);
    return host + "/_vti_bin/vti_aut/author.dll";
}

std::string FsshttpClient::post(const std::string& xml) {
    std::string token = m_tokenProvider();
    if (token.empty()) return {};
    std::string response;
    CURL* curl = curl_easy_init();
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
    headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
    curl_easy_setopt(curl, CURLOPT_URL, endpointUrl().c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

bool FsshttpClient::joinSession() {
    m_session.setJoining();
    auto xml = m_serializer.encodeJoin(
        m_session.fileUrl(), m_session.clientId(), "");
    auto resp = post(xml);
    auto result = m_serializer.decodeJoinResponse(resp);
    m_session.handleJoinResponse(result.success, result.sessionToken);
    if (!result.success) return false;
    m_running = true;
    m_heartbeatThread = std::thread(&FsshttpClient::heartbeatLoop, this);
    return true;
}

void FsshttpClient::exitSession() {
    m_running = false;
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    if (m_session.state() == FsshttpSession::State::Joined) {
        auto xml = m_serializer.encodeExit(
            m_session.fileUrl(), m_session.clientId(), m_session.sessionToken());
        post(xml);
        m_session.handleExit();
    }
}

void FsshttpClient::heartbeatLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        if (!m_running) break;
        auto xml = m_serializer.encodeRefresh(
            m_session.fileUrl(), m_session.clientId(), m_session.sessionToken());
        auto resp = post(xml);
        if (m_serializer.decodeRefreshResponse(resp)) {
            m_session.handleRefreshSuccess();
        } else {
            m_session.handleRefreshFailure();
            if (m_session.state() == FsshttpSession::State::Disconnected) {
                if (onSessionDropped) onSessionDropped();
                break;
            }
        }
    }
}

void FsshttpClient::sendDelta(const std::string& /*deltaJson*/) {
    // CellSubRequest (FsshttpCellStorageData) binary encoding — Phase 2 (see Known Gaps)
}

bool FsshttpClient::isJoined() const {
    return m_session.state() == FsshttpSession::State::Joined;
}
