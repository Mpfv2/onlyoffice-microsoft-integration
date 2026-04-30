#include "AuthModule.h"
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <cstdlib>   // rand, system
#include <algorithm>

static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* s) {
    s->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

AuthModule::AuthModule(const std::string& tenantDomain, const std::string& clientId)
    : m_tenantDomain(tenantDomain), m_clientId(clientId),
      m_store("mscollab." + tenantDomain) {}

std::string AuthModule::base64UrlEncode(const unsigned char* data, size_t len) {
    static const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (size_t i = 0; i < len; i += 3) {
        unsigned int v = data[i] << 16;
        if (i+1 < len) v |= data[i+1] << 8;
        if (i+2 < len) v |= data[i+2];
        out += table[(v >> 18) & 0x3F];
        out += table[(v >> 12) & 0x3F];
        out += (i+1 < len) ? table[(v >> 6) & 0x3F] : '=';
        out += (i+2 < len) ? table[v & 0x3F] : '=';
    }
    for (auto& c : out) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    out.erase(std::remove(out.begin(), out.end(), '='), out.end());
    return out;
}

std::string AuthModule::generateCodeVerifier() {
    unsigned char buf[32];
    RAND_bytes(buf, sizeof(buf));
    return base64UrlEncode(buf, sizeof(buf));
}

std::string AuthModule::codeChallenge(const std::string& verifier) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(verifier.data()),
           verifier.size(), hash);
    return base64UrlEncode(hash, SHA256_DIGEST_LENGTH);
}

std::string AuthModule::buildAuthUrl(const std::string& verifier, int port) const {
    std::string redirect = "http%3A%2F%2Flocalhost%3A" + std::to_string(port) + "%2Fcallback";
    return "https://login.microsoftonline.com/" + m_tenantDomain +
           "/oauth2/v2.0/authorize"
           "?client_id=" + m_clientId +
           "&response_type=code"
           "&redirect_uri=" + redirect +
           "&scope=Files.ReadWrite%20Sites.ReadWrite.All%20offline_access"
           "&code_challenge=" + codeChallenge(verifier) +
           "&code_challenge_method=S256";
}

// Opens a loopback listen socket on a kernel-chosen ephemeral port.
// Returns the bound socket fd and writes the chosen port to `outPort`.
// Returns -1 on failure.
static int openLoopbackListener(int& outPort) {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) return -1;
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(0);   // 0 = let the kernel pick a free port
    if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server);
        return -1;
    }
    if (listen(server, 1) < 0) {
        close(server);
        return -1;
    }

    sockaddr_in bound{};
    socklen_t boundLen = sizeof(bound);
    if (getsockname(server, (sockaddr*)&bound, &boundLen) < 0) {
        close(server);
        return -1;
    }
    outPort = ntohs(bound.sin_port);
    return server;
}

static std::string waitForCode(int server) {
    int client = accept(server, nullptr, nullptr);
    if (client < 0) { close(server); return {}; }
    char buf[4096]{};
    recv(client, buf, sizeof(buf)-1, 0);
    const char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                       "<h2>Login complete. You can close this tab.</h2>";
    send(client, resp, strlen(resp), 0);
    close(client);
    close(server);
    std::string req(buf);
    auto codePos = req.find("code=");
    if (codePos == std::string::npos) return {};
    codePos += 5;
    auto end = req.find_first_of(" &", codePos);
    return req.substr(codePos, end - codePos);
}

std::string AuthModule::exchangeCode(const std::string& code,
                                     const std::string& verifier,
                                     int port) {
    std::string redirect = "http://localhost:" + std::to_string(port) + "/callback";
    std::string body =
        "client_id=" + m_clientId +
        "&grant_type=authorization_code"
        "&code=" + code +
        "&redirect_uri=" + redirect +
        "&code_verifier=" + verifier;
    std::string response;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,
        ("https://login.microsoftonline.com/" + m_tenantDomain +
         "/oauth2/v2.0/token").c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return response;
}

bool AuthModule::authenticate() {
    // Open the listen socket BEFORE launching the browser so the OS-chosen
    // port is captured up front and the redirect URL embeds the right port.
    // rand()-based port picking was deterministic (no srand) and would clash
    // across app launches.
    int port = 0;
    int server = openLoopbackListener(port);
    if (server < 0) return false;

    std::string verifier = generateCodeVerifier();
    std::string url = buildAuthUrl(verifier, port);
    system(("xdg-open '" + url + "' &").c_str());
    std::string code = waitForCode(server);   // also closes `server`
    if (code.empty()) return false;
    std::string tokenJson = exchangeCode(code, verifier, port);
    auto extract = [&](const std::string& key) -> std::string {
        auto pos = tokenJson.find("\"" + key + "\":");
        if (pos == std::string::npos) return {};
        pos = tokenJson.find('"', pos + key.size() + 3) + 1;
        auto end = tokenJson.find('"', pos);
        return tokenJson.substr(pos, end - pos);
    };
    auto extractNum = [&](const std::string& key) -> int64_t {
        auto pos = tokenJson.find("\"" + key + "\":");
        if (pos == std::string::npos) return 0;
        pos += key.size() + 3;
        auto end = tokenJson.find_first_of(",}", pos);
        return std::stoll(tokenJson.substr(pos, end - pos));
    };
    std::string access  = extract("access_token");
    std::string refresh = extract("refresh_token");
    int64_t expiresIn   = extractNum("expires_in");
    if (access.empty()) return false;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_store.setAccessToken(access);
    m_store.setRefreshToken(refresh);
    m_store.setExpiresAt(now + expiresIn);
    return true;
}

bool AuthModule::refreshTokens() {
    std::string rt = m_store.refreshToken();
    if (rt.empty()) return false;
    std::string body =
        "client_id=" + m_clientId +
        "&grant_type=refresh_token"
        "&refresh_token=" + rt +
        "&scope=Files.ReadWrite%20Sites.ReadWrite.All%20offline_access";
    std::string response;
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL,
        ("https://login.microsoftonline.com/" + m_tenantDomain +
         "/oauth2/v2.0/token").c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    auto extract = [&](const std::string& key) -> std::string {
        auto pos = response.find("\"" + key + "\":");
        if (pos == std::string::npos) return {};
        pos = response.find('"', pos + key.size() + 3) + 1;
        auto end = response.find('"', pos);
        return response.substr(pos, end - pos);
    };
    auto extractNum = [&](const std::string& key) -> int64_t {
        auto pos = response.find("\"" + key + "\":");
        if (pos == std::string::npos) return 0;
        pos += key.size() + 3;
        auto end = response.find_first_of(",}", pos);
        return std::stoll(response.substr(pos, end - pos));
    };
    std::string access = extract("access_token");
    if (access.empty()) return false;
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_store.setAccessToken(access);
    m_store.setExpiresAt(now + extractNum("expires_in"));
    auto newRt = extract("refresh_token");
    if (!newRt.empty()) m_store.setRefreshToken(newRt);
    return true;
}

std::string AuthModule::accessToken() {
    if (m_store.isExpired()) {
        if (!refreshTokens()) return {};
    }
    return m_store.accessToken();
}

bool AuthModule::isAuthenticated() const {
    return !m_store.accessToken().empty();
}

void AuthModule::logout() {
    m_store.clear();
}
