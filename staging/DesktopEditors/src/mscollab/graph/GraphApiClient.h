#pragma once
#include <string>
#include <vector>
#include <functional>

// Wraps Microsoft Graph API calls for OneDrive drive operations.
// All methods are synchronous (blocking libcurl calls).
class GraphApiClient {
public:
    struct DriveItem {
        std::string id;
        std::string name;
        std::string webUrl;
        std::string downloadUrl;  // @microsoft.graph.downloadUrl from Graph API
        bool        isFolder = false;
    };

    explicit GraphApiClient(std::function<std::string()> tokenProvider);

    // Maps a local OneDrive path to its SharePoint webUrl.
    // localPath:          absolute path, e.g. "/home/user/OneDrive - TECHCOLLEGE/Docs/file.docx"
    // onedriveFolderName: the OneDrive folder name, e.g. "OneDrive - TECHCOLLEGE"
    // Returns the item's webUrl on success, empty string on failure.
    std::string resolveWebUrl(const std::string& localPath,
                              const std::string& onedriveFolderName) const;

    // Lists the contents of a OneDrive folder.
    // folderPath: path relative to OneDrive root, e.g. "Documents/School" or "" for root
    // Returns empty vector on failure.
    std::vector<DriveItem> listFolder(const std::string& folderPath) const;

    // Uploads a local .docx file to OneDrive, overwriting the existing cloud copy.
    // Uses the same relative-path extraction as resolveWebUrl.
    // Returns true on HTTP 200/201, false on failure.
    bool uploadFile(const std::string& localPath,
                    const std::string& onedriveFolderName) const;

    // Parses a single DriveItem from a JSON object string excerpt.
    static DriveItem parseDriveItem(const std::string& json);

    // Extracts a JSON string field value. Returns empty string if not found.
    static std::string jsonString(const std::string& json, const std::string& key);

    // Extracts a JSON boolean field value. Returns false if not found.
    static bool jsonBool(const std::string& json, const std::string& key);

private:
    std::function<std::string()> m_tokenProvider;

    // Executes a GET request to the Graph API and returns the response body.
    std::string get(const std::string& url) const;
};
