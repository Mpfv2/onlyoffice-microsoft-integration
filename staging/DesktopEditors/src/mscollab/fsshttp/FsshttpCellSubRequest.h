#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Builds MS-FSSHTTP CellSubRequest SOAP bodies (the binary FSSHTTPB payload
// embedded in a CoauthSubRequest's Storage element).
//
// Reference: MS-FSSHTTP section 2.3.2 (CellSubRequest), MS-FSSHTTPB spec.
struct FsshttpCellSubRequest {
    // Builds a minimal PutChanges request body.
    // docxBytes:    raw OOXML (.docx) bytes for the changed content
    // storageToken: opaque token from the JoinCoauthoring response
    // Returns base64-encoded FSSHTTPB payload ready to embed in SOAP.
    static std::string buildPutChanges(
        const std::vector<uint8_t>& docxBytes,
        const std::string& storageToken);

    // Builds a minimal GetChanges request body.
    // storageToken: opaque token from JoinCoauthoring (or last GetChanges response)
    // Returns base64-encoded FSSHTTPB payload.
    static std::string buildGetChanges(const std::string& storageToken);

    // Parses a GetChanges SOAP response, extracts data element blobs.
    // Returns empty vector on error or if no new changes.
    static std::vector<std::vector<uint8_t>> parseGetChangesResponse(
        const std::string& soapXml);

    // Walks a raw FSSHTTPB binary buffer and returns the body bytes of every
    // ObjectGroupObjectData (type 0x3C) element found.  Falls back to returning
    // the entire buffer as one element if no type-0x3C objects are present
    // (compatibility with servers that embed OOXML differently).
    static std::vector<std::vector<uint8_t>> extractOoxmlBlobs(
        const std::vector<uint8_t>& fsshttpbBinary);
};
