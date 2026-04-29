#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct JoinResponse {
    bool        success = false;
    std::string sessionToken;
    std::string errorCode;
};

struct CoauthorsInfo {
    int         count = 0;
    std::string sessionToken;
};

struct CellResponse {
    bool success = false;
    // Each entry is a raw decoded data blob (OOXML bytes) from a GetChanges response.
    std::vector<std::vector<uint8_t>> dataBlobs;
    // Opaque base64 knowledge token from the server.
    // Pass back to the next GetChanges request so the server only returns new changes.
    std::string currentKnowledgeB64;
};

// Encodes MS-FSSHTTP SubRequests as SOAP/XML and decodes SubResponses.
// All methods are stateless. Reference: [MS-FSSHTTP] sections 2.3-2.9.
class FsshttpSerializer {
public:
    // Coauthoring SubRequests
    std::string encodeJoin(const std::string& fileUrl,
                           const std::string& clientId,
                           const std::string& sessionToken) const;

    std::string encodeRefresh(const std::string& fileUrl,
                              const std::string& clientId,
                              const std::string& sessionToken) const;

    std::string encodeExit(const std::string& fileUrl,
                           const std::string& clientId,
                           const std::string& sessionToken) const;

    JoinResponse decodeJoinResponse(const std::string& xml) const;
    bool         decodeRefreshResponse(const std::string& xml) const;

    // Cell SubRequests — carry FSSHTTPB binary payloads for document deltas.
    // base64Payload is the output of FsshttpCellSubRequest::buildPutChanges/buildGetChanges.
    std::string encodeCellPutChanges(const std::string& fileUrl,
                                     const std::string& clientId,
                                     const std::string& sessionToken,
                                     const std::string& base64Payload,
                                     uint32_t           payloadBytes) const;

    // knowledgeB64: from the previous CellResponse::currentKnowledgeB64; empty for first call.
    std::string encodeCellGetChanges(const std::string& fileUrl,
                                     const std::string& clientId,
                                     const std::string& sessionToken,
                                     const std::string& knowledgeB64 = {}) const;

    CellResponse decodeCellSubResponse(const std::string& xml) const;

private:
    std::string wrapEnvelope(const std::string& fileUrl,
                             const std::string& subrequest) const;
    std::string xmlAttrValue(const std::string& xml,
                             const std::string& attr) const;
};
