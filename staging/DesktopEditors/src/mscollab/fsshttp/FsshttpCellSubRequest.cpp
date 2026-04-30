#include "FsshttpCellSubRequest.h"
#include "FsshttpBinaryEncoder.h"
#include <cstring>

// ---------------------------------------------------------------------------
// Deterministic GUIDs used in PutChanges payloads
// ---------------------------------------------------------------------------
static const uint8_t GUID_Storage[16] = {
    0x6f,0x67,0x61,0x74,0x69,0x6f,0x6e,0x73,
    0x6f,0x63,0x74,0x6f,0x72,0x79,0x21,0x00
};
static const uint8_t GUID_A[16] = {
    0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10
};
static const uint8_t GUID_B[16] = {
    0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,
    0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,0x20
};
static const uint8_t GUID_C[16] = {
    0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,0x30
};

// ---------------------------------------------------------------------------
// Stream object type constants (MS-FSSHTTPB)
// ---------------------------------------------------------------------------
// These are the 6-bit (or 14-bit) type codes used in stream object headers.
static const uint8_t  SOT_REQUEST                   = 0x1A;
static const uint8_t  SOT_SUB_REQUEST               = 0x1B;
static const uint8_t  SOT_DATA_ELEMENT_PACKAGE      = 0x44;  // 0x44 > 0x3F → 32-bit
static const uint8_t  SOT_DATA_ELEMENT              = 0x01;
static const uint8_t  SOT_OBJ_GROUP_DECLARATIONS    = 0x45;  // > 0x3F → 32-bit
static const uint8_t  SOT_OBJ_GROUP_OBJECT_DECLARE  = 0x05;
static const uint8_t  SOT_OBJ_GROUP_DATA            = 0x46;  // > 0x3F → 32-bit
static const uint8_t  SOT_OBJ_GROUP_OBJECT_DATA     = 0x3C;

// Helper: choose 16-bit or 32-bit header automatically based on type and length.
// Types > 0x3F or lengths > 0x3F require the 32-bit form.
static void writeHeader(FsshttpBinaryWriter& w, uint16_t type, uint32_t length, bool compound) {
    if (type > 0x3F || length > 0x3F) {
        w.writeStreamHeader32(type, length, compound);
    } else {
        w.writeStreamHeader16(static_cast<uint8_t>(type),
                              static_cast<uint8_t>(length), compound);
    }
}

// ---------------------------------------------------------------------------
// FsshttpCellSubRequest::buildPutChanges
// ---------------------------------------------------------------------------
// Builds an FSSHTTPB binary payload encoding a PutChanges sub-request that
// wraps the supplied OOXML bytes as a single ObjectGroup data element.
//
// Stream object layout (MS-FSSHTTPB spec):
//
//   Request (compound, type=0x1A)
//     SubRequest (compound, type=0x1B)
//       RequestToken: uvarint(1)
//       Type:         uvarint(1)    // 1 = PutChanges
//       DataElementPackage (compound, type=0x44)
//         DataElement (compound, type=0x01)
//           ExGUID: (GUID_Storage, 1)
//           Type:   uvarint(1)     // 1 = ObjectGroup
//           ObjectGroupDeclarations (compound, type=0x45)
//             ObjectGroupObjectDeclare (non-compound, type=0x05)
//               CellID: (GUID_A,1 / GUID_B,1)
//               ObjectExtendedGUID: (GUID_C, 1)
//               PartitionID:   uvarint(1)
//               ObjectDataSize: uvarint(docxBytes.size())
//           end ObjectGroupDeclarations
//           ObjectGroupData (compound, type=0x46)
//             ObjectGroupObjectData (non-compound, type=0x3C)
//               Length: uvarint(docxBytes.size())
//               [raw docxBytes]
//           end ObjectGroupData
//         end DataElement
//       end DataElementPackage
//     end SubRequest
//   end Request
// ---------------------------------------------------------------------------
std::string FsshttpCellSubRequest::buildPutChanges(
        const std::vector<uint8_t>& docxBytes,
        const std::string& /*storageToken*/) {

    FsshttpBinaryWriter w;

    // --- Request (compound) ------------------------------------------------
    writeHeader(w, SOT_REQUEST, 0, /*compound=*/true);

    // --- SubRequest (compound) ---------------------------------------------
    writeHeader(w, SOT_SUB_REQUEST, 0, /*compound=*/true);

    // RequestToken = 1, Type = 1 (PutChanges)
    w.writeUvarint(1);
    w.writeUvarint(1);

    // --- DataElementPackage (compound) -------------------------------------
    writeHeader(w, SOT_DATA_ELEMENT_PACKAGE, 0, /*compound=*/true);

    // --- DataElement (compound) --------------------------------------------
    writeHeader(w, SOT_DATA_ELEMENT, 0, /*compound=*/true);

    // ExGUID for the storage element
    w.writeExGuid(GUID_Storage, 1);
    // Type = 1 (ObjectGroup)
    w.writeUvarint(1);

    // --- ObjectGroupDeclarations (compound) --------------------------------
    writeHeader(w, SOT_OBJ_GROUP_DECLARATIONS, 0, /*compound=*/true);

    // Build the ObjectGroupObjectDeclare body first so we know its size.
    {
        FsshttpBinaryWriter decl;
        // CellID: (GUID_A,1) / (GUID_B,1)
        decl.writeCellId(GUID_A, 1, GUID_B, 1);
        // ObjectExtendedGUID: (GUID_C, 1)
        decl.writeExGuid(GUID_C, 1);
        // PartitionID
        decl.writeUvarint(1);
        // ObjectDataSize
        decl.writeUvarint(static_cast<uint64_t>(docxBytes.size()));

        // ObjectGroupObjectDeclare header (non-compound); length = body size
        uint32_t declSize = static_cast<uint32_t>(decl.size());
        writeHeader(w, SOT_OBJ_GROUP_OBJECT_DECLARE, declSize, /*compound=*/false);
        w.writeBytes(decl.bytes().data(), decl.size());
    }

    // end ObjectGroupDeclarations
    w.writeEndObject(SOT_OBJ_GROUP_DECLARATIONS & 0x7F);

    // --- ObjectGroupData (compound) ----------------------------------------
    writeHeader(w, SOT_OBJ_GROUP_DATA, 0, /*compound=*/true);

    // Build ObjectGroupObjectData body
    {
        FsshttpBinaryWriter objData;
        objData.writeUvarint(static_cast<uint64_t>(docxBytes.size()));
        objData.writeBytes(docxBytes.data(), docxBytes.size());

        uint32_t objDataSize = static_cast<uint32_t>(objData.size());
        writeHeader(w, SOT_OBJ_GROUP_OBJECT_DATA, objDataSize, /*compound=*/false);
        w.writeBytes(objData.bytes().data(), objData.size());
    }

    // end ObjectGroupData
    w.writeEndObject(SOT_OBJ_GROUP_DATA & 0x7F);

    // end DataElement
    w.writeEndObject(SOT_DATA_ELEMENT & 0x7F);

    // end DataElementPackage
    w.writeEndObject(SOT_DATA_ELEMENT_PACKAGE & 0x7F);

    // end SubRequest
    w.writeEndObject(SOT_SUB_REQUEST & 0x7F);

    // end Request
    w.writeEndObject(SOT_REQUEST & 0x7F);

    return w.toBase64();
}

// ---------------------------------------------------------------------------
// FsshttpCellSubRequest::buildGetChanges
// ---------------------------------------------------------------------------
// Minimal GetChanges request — server returns all changes from root knowledge.
//
//   Request (compound, type=0x1A)
//     SubRequest (compound, type=0x1B)
//       RequestToken: uvarint(2)
//       Type:         uvarint(2)   // 2 = GetChanges
//     end SubRequest
//   end Request
// ---------------------------------------------------------------------------
std::string FsshttpCellSubRequest::buildGetChanges(const std::string& /*storageToken*/) {
    FsshttpBinaryWriter w;

    // Request (compound)
    writeHeader(w, SOT_REQUEST, 0, /*compound=*/true);

    // SubRequest (compound)
    writeHeader(w, SOT_SUB_REQUEST, 0, /*compound=*/true);

    // RequestToken = 2, Type = 2 (GetChanges)
    w.writeUvarint(2);
    w.writeUvarint(2);

    // end SubRequest
    w.writeEndObject(SOT_SUB_REQUEST & 0x7F);

    // end Request
    w.writeEndObject(SOT_REQUEST & 0x7F);

    return w.toBase64();
}

// ---------------------------------------------------------------------------
// FsshttpCellSubRequest::parseGetChangesResponse
// ---------------------------------------------------------------------------
// Parses a GetChanges SOAP response and extracts data element blobs.
//
// Strategy:
//   1. Look for base64 content between <StorageResponseData>...</StorageResponseData>.
//   2. Fall back to SubResponseData="..." attribute pattern.
//   3. Base64-decode the found blob.
//   4. Walk the stream objects looking for type 0x3C (ObjectGroupObjectData);
//      collect each body as a blob. If none found, return the whole decoded
//      buffer as a single element (caller treats it as one delta).
// ---------------------------------------------------------------------------
static std::string extractBase64FromXml(const std::string& xml) {
    // Try <StorageResponseData>...</StorageResponseData>
    const char* openTag  = "<StorageResponseData>";
    const char* closeTag = "</StorageResponseData>";
    size_t start = xml.find(openTag);
    if (start != std::string::npos) {
        start += std::strlen(openTag);
        size_t end = xml.find(closeTag, start);
        if (end != std::string::npos) {
            return xml.substr(start, end - start);
        }
    }

    // Try <wopi:StorageResponse>...</wopi:StorageResponse>
    const char* wopiOpen  = "<wopi:StorageResponse>";
    const char* wopiClose = "</wopi:StorageResponse>";
    start = xml.find(wopiOpen);
    if (start != std::string::npos) {
        start += std::strlen(wopiOpen);
        size_t end = xml.find(wopiClose, start);
        if (end != std::string::npos) {
            return xml.substr(start, end - start);
        }
    }

    // Try SubResponseData="..." attribute
    const char* attr = "SubResponseData=\"";
    start = xml.find(attr);
    if (start != std::string::npos) {
        start += std::strlen(attr);
        size_t end = xml.find('"', start);
        if (end != std::string::npos) {
            return xml.substr(start, end - start);
        }
    }

    return {};
}

// Reads a CompactUnsignedInt64 starting at offset `pos` in `buf`.
// On success, returns true and writes value/byteCount out-params.
// On overflow / overrun, returns false.
static bool tryReadUvarintAt(const std::vector<uint8_t>& buf, size_t pos,
                             uint64_t& outValue, size_t& outBytes) {
    uint64_t value = 0;
    int shift = 0;
    size_t i = pos;
    while (i < buf.size()) {
        uint8_t b = buf[i++];
        value |= static_cast<uint64_t>((b >> 1) & 0x7F) << shift;
        shift += 7;
        if (b & 0x01) {
            outValue = value;
            outBytes = i - pos;
            return true;
        }
        if (shift >= 64) return false;
    }
    return false;
}

// Schema-aware extractor.
//
// A naive stream-header walker cannot navigate FSSHTTPB because compound
// stream objects contain inline content (uvarints, ExGUIDs) before nested
// objects — e.g. SubRequest carries 2 uvarints, DataElement carries an
// ExGUID + uvarint. Reading those bytes as headers walks the parser into
// garbage.
//
// We instead scan the buffer for ObjectGroupObjectData stream headers (type
// 0x3C, non-compound) by their byte signature, validate each candidate by
// reading its body (uvarint(dataLen) + dataLen bytes) and confirming the
// math is consistent with the header's declared length, then extract the
// inner OOXML payload.
//
// 32-bit header for type=0x3C non-compound:
//   byte 0 = 0xF1   (bit 0 = 1 [32-bit], bit 1 = 0 [non-compound],
//                    bits 7:2 of value = 0x3C low 6 bits = 0x3C, so:
//                    0x01 | (0x3C << 2) = 0x01 | 0xF0 = 0xF1)
//   byte 1 = 0x00   (high 8 bits of (type << 2) = 0)
//   bytes 2-3 = 15-bit length LE (high bit of byte 3 must be 0)
//
// 16-bit header for type=0x3C non-compound:
//   byte 0 = 0xF0   (bit 0 = 0, bit 1 = 0, bits 7:2 = 0x3C → 0x3C << 2 = 0xF0)
//   byte 1 = 0xC0 | (length & 0x3F)   (bits 15:14 = 0b11, length 6 bits)
std::vector<std::vector<uint8_t>>
FsshttpCellSubRequest::extractOoxmlBlobs(const std::vector<uint8_t>& raw) {
    std::vector<std::vector<uint8_t>> blobs;
    if (raw.size() < 2) {
        if (!raw.empty()) blobs.push_back(raw);
        return blobs;
    }

    size_t i = 0;
    while (i + 1 < raw.size()) {
        // 32-bit header: 0xF1 0x00 [len_lo] [len_hi & 0x7F]
        if (raw[i] == 0xF1 && raw[i + 1] == 0x00 && i + 4 <= raw.size()) {
            uint8_t lh = raw[i + 3];
            if ((lh & 0x80) == 0) {
                uint32_t length = static_cast<uint32_t>(raw[i + 2])
                                | (static_cast<uint32_t>(lh) << 8);
                size_t bodyStart = i + 4;
                if (length > 0 && bodyStart + length <= raw.size()) {
                    uint64_t dataLen = 0;
                    size_t   uvBytes = 0;
                    if (tryReadUvarintAt(raw, bodyStart, dataLen, uvBytes)
                            && dataLen > 0
                            && uvBytes + dataLen == length) {
                        size_t blobStart = bodyStart + uvBytes;
                        blobs.emplace_back(raw.begin() + blobStart,
                                           raw.begin() + blobStart + dataLen);
                        i = bodyStart + length;
                        continue;
                    }
                }
            }
        }

        // 16-bit header: 0xF0 [0xC0..0xFF]
        if (raw[i] == 0xF0 && (raw[i + 1] & 0xC0) == 0xC0 && i + 2 <= raw.size()) {
            uint32_t length = static_cast<uint32_t>(raw[i + 1] & 0x3F);
            size_t bodyStart = i + 2;
            if (length > 0 && bodyStart + length <= raw.size()) {
                uint64_t dataLen = 0;
                size_t   uvBytes = 0;
                if (tryReadUvarintAt(raw, bodyStart, dataLen, uvBytes)
                        && dataLen > 0
                        && uvBytes + dataLen == length) {
                    size_t blobStart = bodyStart + uvBytes;
                    blobs.emplace_back(raw.begin() + blobStart,
                                       raw.begin() + blobStart + dataLen);
                    i = bodyStart + length;
                    continue;
                }
            }
        }

        ++i;
    }

    if (blobs.empty()) {
        // Fallback: caller may receive a server response that wraps OOXML
        // differently (or the buffer is the OOXML directly). Return the
        // whole buffer so higher layers can attempt XML parsing.
        blobs.push_back(raw);
    }
    return blobs;
}

std::vector<std::vector<uint8_t>>
FsshttpCellSubRequest::parseGetChangesResponse(const std::string& soapXml) {
    std::string b64 = extractBase64FromXml(soapXml);
    if (b64.empty()) return {};

    std::string cleanB64;
    cleanB64.reserve(b64.size());
    for (char c : b64) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            cleanB64 += c;
    }
    if (cleanB64.empty()) return {};

    std::vector<uint8_t> raw = FsshttpBinaryReader::fromBase64(cleanB64);
    if (raw.empty()) return {};

    return extractOoxmlBlobs(raw);
}
