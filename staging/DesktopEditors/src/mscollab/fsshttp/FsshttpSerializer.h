#pragma once
#include <string>

struct JoinResponse {
    bool        success = false;
    std::string sessionToken;
    std::string errorCode;
};

struct CoauthorsInfo {
    int         count = 0;
    std::string sessionToken;
};

// Encodes MS-FSSHTTP SubRequests as SOAP/XML and decodes SubResponses.
// All methods are stateless. Reference: [MS-FSSHTTP] sections 2.3-2.9.
class FsshttpSerializer {
public:
    std::string encodeJoin(const std::string& fileUrl,
                           const std::string& clientId,
                           const std::string& sessionToken) const;

    std::string encodeRefresh(const std::string& fileUrl,
                              const std::string& clientId,
                              const std::string& sessionToken) const;

    std::string encodeExit(const std::string& fileUrl,
                           const std::string& clientId,
                           const std::string& sessionToken) const;

    JoinResponse   decodeJoinResponse(const std::string& xml) const;
    bool           decodeRefreshResponse(const std::string& xml) const;

private:
    std::string wrapEnvelope(const std::string& fileUrl,
                             const std::string& subrequest) const;
    std::string xmlAttrValue(const std::string& xml,
                             const std::string& attr) const;
};
