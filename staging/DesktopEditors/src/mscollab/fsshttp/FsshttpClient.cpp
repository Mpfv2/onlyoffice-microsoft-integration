#include "FsshttpClient.h"
#include "FsshttpCellSubRequest.h"
#include <curl/curl.h>
#include <chrono>
#include <thread>
#include <iostream>

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
    {
        std::lock_guard<std::mutex> lk(m_knowledgeMutex);
        m_knowledgeB64.clear();  // fresh session — start from root knowledge
    }
    m_running = true;
    m_heartbeatThread = std::thread(&FsshttpClient::heartbeatLoop, this);
    m_pollThread      = std::thread(&FsshttpClient::pollLoop, this);
    flushPendingDeltas();
    return true;
}

void FsshttpClient::exitSession() {
    m_running = false;
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    if (m_pollThread.joinable())      m_pollThread.join();
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
                // Exponential backoff rejoin: 5s, 10s, 20s before giving up.
                bool rejoined = false;
                for (int attempt = 0; attempt < 3 && m_running; ++attempt) {
                    std::this_thread::sleep_for(
                        std::chrono::seconds(5 * (1 << attempt)));
                    if (!m_running) break;
                    m_session.setJoining();
                    auto joinXml = m_serializer.encodeJoin(
                        m_session.fileUrl(), m_session.clientId(), "");
                    auto joinResp = post(joinXml);
                    auto jr = m_serializer.decodeJoinResponse(joinResp);
                    m_session.handleJoinResponse(jr.success, jr.sessionToken);
                    if (jr.success) {
                        {
                            std::lock_guard<std::mutex> lk(m_knowledgeMutex);
                            m_knowledgeB64.clear();
                        }
                        flushPendingDeltas();
                        rejoined = true;
                        break;
                    }
                }
                if (!rejoined) {
                    if (onSessionDropped) onSessionDropped();
                    break;
                }
            }
        }
    }
}

void FsshttpClient::pollLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (!m_running) break;
        if (isJoined()) pollGetChanges();
    }
}

void FsshttpClient::flushPendingDeltas() {
    std::lock_guard<std::mutex> lock(m_deltaMutex);
    while (!m_pendingDeltas.empty()) {
        sendDeltaImmediate(m_pendingDeltas.front());
        m_pendingDeltas.pop();
    }
}

void FsshttpClient::loadDocument(const std::string& localDocxPath) {
    if (!m_document.loadFromDocx(localDocxPath))
        std::cerr << "[MsCollab] OoxmlDocument: could not load " << localDocxPath << "\n";
}

void FsshttpClient::pollGetChanges() {
    std::string knowledge;
    {
        std::lock_guard<std::mutex> lk(m_knowledgeMutex);
        knowledge = m_knowledgeB64;
    }

    auto xml = m_serializer.encodeCellGetChanges(
        m_session.fileUrl(), m_session.clientId(), m_session.sessionToken(), knowledge);
    auto resp = post(xml);
    if (resp.empty()) return;
    auto cellResp = m_serializer.decodeCellSubResponse(resp);

    // Update the stored knowledge token so the next poll fetches only new changes.
    if (!cellResp.currentKnowledgeB64.empty()) {
        std::lock_guard<std::mutex> lk(m_knowledgeMutex);
        m_knowledgeB64 = cellResp.currentKnowledgeB64;
    }

    for (const auto& blob : cellResp.dataBlobs) {
        // Try comments part first (has <w:comments> root).
        auto comments = OoxmlDocument::parseCommentDeltas(blob);
        if (!comments.empty()) {
            for (const auto& c : comments) {
                std::string j = "{\"type\":\"comment\""
                                ",\"commentId\":"  + std::to_string(c.id) +
                                ",\"author\":\"" + c.author + "\""
                                ",\"date\":\"" + c.date + "\""
                                ",\"text\":\"" + c.text + "\"}";
                if (onRemoteDelta) onRemoteDelta(j);
            }
            continue;
        }

        // Fall through to paragraph parsing (document.xml blobs).
        if (m_document.isLoaded()) {
            auto deltas = OoxmlDocument::parseParagraphDeltas(blob);
            for (const auto& d : deltas) {
                std::string j = "{\"paragraphIndex\":" + std::to_string(d.index) +
                                ",\"content\":\"" + d.text + "\"}";
                if (onRemoteDelta) onRemoteDelta(j);
            }
        } else {
            std::string delta = ooxmlToDeltaJsonStub(blob);
            if (!delta.empty() && onRemoteDelta) onRemoteDelta(delta);
        }
    }
}

void FsshttpClient::sendDelta(const std::string& deltaJson) {
    if (!isJoined()) {
        std::lock_guard<std::mutex> lock(m_deltaMutex);
        m_pendingDeltas.push(deltaJson);
        return;
    }
    sendDeltaImmediate(deltaJson);
}

// Returns the value of a JSON string field, handling \" and \\ escapes.
static std::string jsonFieldStr(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos) return {};
    // Skip to the opening quote of the value
    auto q1 = json.find('"', pos + key.size() + 3);
    if (q1 == std::string::npos) return {};
    ++q1;  // skip the opening '"'
    std::string out;
    for (size_t i = q1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '\\' && i + 1 < json.size()) {
            char nc = json[++i];
            if      (nc == '"')  out += '"';
            else if (nc == '\\') out += '\\';
            else if (nc == 'n')  out += '\n';
            else if (nc == 'r')  out += '\r';
            else if (nc == 't')  out += '\t';
            else                 out += nc;
        } else if (c == '"') {
            break;  // end of string
        } else {
            out += c;
        }
    }
    return out;
}

static int jsonFieldInt(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos) return 0;
    pos += key.size() + 3;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
    try { return std::stoi(json.substr(pos)); } catch (...) { return 0; }
}

void FsshttpClient::sendDeltaImmediate(const std::string& deltaJson) {
    // Comment delta: {type:"comment", commentId:N, author:"...", date:"...", text:"..."}
    if (deltaJson.find("\"type\":\"comment\"") != std::string::npos) {
        int id        = jsonFieldInt(deltaJson, "commentId");
        std::string author = jsonFieldStr(deltaJson, "author");
        std::string date   = jsonFieldStr(deltaJson, "date");
        std::string text   = jsonFieldStr(deltaJson, "text");
        m_document.applyCommentDelta(id, author, date, text);
        auto ooxml = m_document.serializeComments();
        if (ooxml.empty()) return;
        auto b64 = FsshttpCellSubRequest::buildPutChanges(ooxml, m_session.sessionToken());
        auto soapXml = m_serializer.encodeCellPutChanges(
            m_session.fileUrl(), m_session.clientId(), m_session.sessionToken(),
            b64, static_cast<uint32_t>(ooxml.size()));
        post(soapXml);
        return;
    }

    // Paragraph delta: {paragraphIndex:N, content:"..."}
    std::vector<uint8_t> ooxml;
    if (m_document.isLoaded()) {
        int paraIdx = jsonFieldInt(deltaJson, "paragraphIndex");
        std::string content = jsonFieldStr(deltaJson, "content");
        m_document.applyParagraphDelta(paraIdx, content);
        ooxml = m_document.serialize();
    } else {
        ooxml = deltaToOoxmlStub(deltaJson);
    }
    auto b64 = FsshttpCellSubRequest::buildPutChanges(ooxml, m_session.sessionToken());
    auto soapXml = m_serializer.encodeCellPutChanges(
        m_session.fileUrl(), m_session.clientId(), m_session.sessionToken(),
        b64, static_cast<uint32_t>(ooxml.size()));
    post(soapXml);
}

static std::string jsonEscapeStr(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '"')  out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else                out += c;
    }
    return out;
}

static std::string xmlEscapeStr(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '<')  out += "&lt;";
        else if (c == '>')  out += "&gt;";
        else if (c == '&')  out += "&amp;";
        else if (c == '"')  out += "&quot;";
        else                out += c;
    }
    return out;
}

std::vector<uint8_t> FsshttpClient::deltaToOoxmlStub(const std::string& deltaJson) {
    std::string content = jsonFieldStr(deltaJson, "content");
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body><w:p><w:r><w:t xml:space=\"preserve\">" +
        xmlEscapeStr(content) +
        "</w:t></w:r></w:p></w:body>"
        "</w:document>";
    return std::vector<uint8_t>(xml.begin(), xml.end());
}

std::string FsshttpClient::ooxmlToDeltaJsonStub(const std::vector<uint8_t>& ooxml) {
    std::string xml(ooxml.begin(), ooxml.end());
    std::string text;
    size_t pos = 0;
    while ((pos = xml.find("<w:t", pos)) != std::string::npos) {
        // Make sure it's exactly <w:t not <w:tbl etc.
        char next = (pos + 4 < xml.size()) ? xml[pos + 4] : 0;
        if (next != '>' && next != ' ' && next != '\t' && next != '\n') {
            pos += 4;
            continue;
        }
        auto gt = xml.find('>', pos);
        if (gt == std::string::npos) break;
        auto lt = xml.find("</w:t>", gt);
        if (lt == std::string::npos) break;
        text += xml.substr(gt + 1, lt - gt - 1);
        pos = lt + 6;
    }
    if (text.empty()) return {};
    return "{\"paragraphIndex\":0,\"content\":\"" + jsonEscapeStr(text) + "\"}";
}

bool FsshttpClient::isJoined() const {
    return m_session.state() == FsshttpSession::State::Joined;
}
