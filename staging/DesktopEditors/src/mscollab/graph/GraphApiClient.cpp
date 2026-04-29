#include "GraphApiClient.h"
#include <curl/curl.h>
#include <algorithm>
#include <sstream>

static const std::string GRAPH_BASE = "https://graph.microsoft.com/v1.0/me/drive";

static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* s) {
    s->append(static_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

GraphApiClient::GraphApiClient(std::function<std::string()> tokenProvider)
    : m_tokenProvider(std::move(tokenProvider)) {}

std::string GraphApiClient::get(const std::string& url) const {
    std::string token = m_tokenProvider();
    if (token.empty()) return {};

    std::string response;
    CURL* curl = curl_easy_init();
    if (!curl) return {};

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

std::string GraphApiClient::jsonString(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos) return {};
    pos = json.find('"', pos + key.size() + 3);
    if (pos == std::string::npos) return {};
    ++pos;
    auto end = json.find('"', pos);
    if (end == std::string::npos) return {};
    return json.substr(pos, end - pos);
}

bool GraphApiClient::jsonBool(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos) return false;
    pos += key.size() + 3;
    while (pos < json.size() && json[pos] == ' ') ++pos;
    return json.substr(pos, 4) == "true";
}

GraphApiClient::DriveItem GraphApiClient::parseDriveItem(const std::string& json) {
    DriveItem item;
    item.id          = jsonString(json, "id");
    item.name        = jsonString(json, "name");
    item.webUrl      = jsonString(json, "webUrl");
    item.downloadUrl = jsonString(json, "@microsoft.graph.downloadUrl");
    // A folder item has a "folder" object; a file has "file"
    item.isFolder    = (json.find("\"folder\":{") != std::string::npos ||
                        json.find("\"folder\": {") != std::string::npos);
    return item;
}

std::string GraphApiClient::resolveWebUrl(const std::string& localPath,
                                          const std::string& onedriveFolderName) const {
    // Find the OneDrive root in the local path and extract the relative portion.
    // e.g. "/home/user/OneDrive - TECHCOLLEGE/Docs/file.docx"
    //   -> relative = "Docs/file.docx"
    auto pos = localPath.find(onedriveFolderName);
    if (pos == std::string::npos) return {};
    pos += onedriveFolderName.size();
    if (pos < localPath.size() && (localPath[pos] == '/' || localPath[pos] == '\\'))
        ++pos;
    std::string relativePath = localPath.substr(pos);
    // Normalise backslashes to forward slashes
    std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
    if (relativePath.empty()) return {};

    // Graph API: GET /me/drive/root:/{relative-path}
    // URL-encode minimal characters (' ' -> %20, but Graph accepts unencoded slashes)
    std::string encoded;
    for (char c : relativePath) {
        if (c == ' ') encoded += "%20";
        else          encoded += c;
    }

    std::string url = GRAPH_BASE + "/root:/" + encoded;
    std::string resp = get(url);
    if (resp.empty()) return {};
    return jsonString(resp, "webUrl");
}

std::vector<GraphApiClient::DriveItem> GraphApiClient::listFolder(
        const std::string& folderPath) const {
    std::string url;
    if (folderPath.empty()) {
        url = GRAPH_BASE + "/root/children?$select=id,name,webUrl,folder,file,@microsoft.graph.downloadUrl";
    } else {
        std::string encoded;
        for (char c : folderPath) {
            if (c == ' ') encoded += "%20";
            else          encoded += c;
        }
        url = GRAPH_BASE + "/root:/" + encoded + ":/children"
              "?$select=id,name,webUrl,folder,file,@microsoft.graph.downloadUrl";
    }

    std::string resp = get(url);
    if (resp.empty()) return {};

    // Parse the "value": [...] array from the response.
    // Each element is a JSON object for one item.
    std::vector<DriveItem> items;
    auto valuePos = resp.find("\"value\":[");
    if (valuePos == std::string::npos) return {};
    valuePos += 9; // skip "value":[

    int depth = 0;
    std::string current;
    bool inObject = false;
    for (size_t i = valuePos; i < resp.size(); ++i) {
        char c = resp[i];
        if (c == '{') {
            ++depth;
            inObject = true;
            current += c;
        } else if (c == '}' && inObject) {
            current += c;
            --depth;
            if (depth == 0) {
                items.push_back(parseDriveItem(current));
                current.clear();
                inObject = false;
            }
        } else if (c == ']' && depth == 0) {
            break;
        } else if (inObject) {
            current += c;
        }
    }
    return items;
}
