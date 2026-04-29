#include "FsshttpBinaryEncoder.h"
#include <stdexcept>
#include <cstring>

// ---------------------------------------------------------------------------
// FsshttpBinaryWriter
// ---------------------------------------------------------------------------

void FsshttpBinaryWriter::writeByte(uint8_t b) {
    m_buf.push_back(b);
}

void FsshttpBinaryWriter::writeBytes(const uint8_t* data, size_t len) {
    m_buf.insert(m_buf.end(), data, data + len);
}

void FsshttpBinaryWriter::writeUint16LE(uint16_t v) {
    m_buf.push_back(static_cast<uint8_t>(v & 0xFF));
    m_buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void FsshttpBinaryWriter::writeUint32LE(uint32_t v) {
    m_buf.push_back(static_cast<uint8_t>(v & 0xFF));
    m_buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    m_buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    m_buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

void FsshttpBinaryWriter::writeUint64LE(uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        m_buf.push_back(static_cast<uint8_t>(v & 0xFF));
        v >>= 8;
    }
}

void FsshttpBinaryWriter::writeGuid(const uint8_t guid[16]) {
    writeBytes(guid, 16);
}

// CompactUnsignedInt64 (MS-FSSHTTPB 2.2.1.1):
// Each byte encodes 7 value bits in bits[7:1]. Bit 0 = 0 for continuation,
// bit 0 = 1 for the final byte.
void FsshttpBinaryWriter::writeUvarint(uint64_t value) {
    do {
        uint8_t b = static_cast<uint8_t>((value & 0x7F) << 1);
        value >>= 7;
        if (value == 0) {
            b |= 0x01;  // last byte: set bit 0
        }
        m_buf.push_back(b);
    } while (value != 0);
}

// ExGUID (MS-FSSHTTPB 2.2.1.5): GUID (16 bytes) + CompactUnsignedInt64
// If N == 0, write the zero GUID + uvarint(0).
void FsshttpBinaryWriter::writeExGuid(const uint8_t guid[16], uint64_t n) {
    if (n == 0) {
        static const uint8_t zeroGuid[16] = {};
        writeGuid(zeroGuid);
        writeUvarint(0);
    } else {
        writeGuid(guid);
        writeUvarint(n);
    }
}

// CellID (MS-FSSHTTPB 2.2.1.7): two ExGUIDs
void FsshttpBinaryWriter::writeCellId(const uint8_t spaceGuid[16], uint64_t spaceN,
                                       const uint8_t objGuid[16],   uint64_t objN) {
    writeExGuid(spaceGuid, spaceN);
    writeExGuid(objGuid, objN);
}

// StreamObjectHeader 16-bit (MS-FSSHTTPB 2.2.1.5.1):
//   bit 0    = 0           (16-bit marker)
//   bit 1    = compound
//   bits 7:2 = type (6-bit)
//   bits 13:8 = length (6-bit)
//   bits 15:14 = 0b11      (constant)
void FsshttpBinaryWriter::writeStreamHeader16(uint8_t type, uint8_t length, bool compound) {
    uint16_t v = 0;
    // bit 0 = 0 (16-bit header)
    if (compound) v |= 0x0002;          // bit 1
    v |= (static_cast<uint16_t>(type & 0x3F) << 2);   // bits 7:2
    v |= (static_cast<uint16_t>(length & 0x3F) << 8);  // bits 13:8
    v |= 0xC000;                        // bits 15:14 = 0b11
    writeUint16LE(v);
}

// StreamObjectHeader 32-bit (MS-FSSHTTPB 2.2.1.5.2):
//   bit 0    = 1           (32-bit marker)
//   bit 1    = compound
//   bits 15:2 = type (14-bit)
//   bits 30:16 = length (15-bit)
//   bit 31 = 0
void FsshttpBinaryWriter::writeStreamHeader32(uint16_t type, uint32_t length, bool compound) {
    uint32_t v = 0x00000001;            // bit 0 = 1 (32-bit header)
    if (compound) v |= 0x00000002;      // bit 1
    v |= (static_cast<uint32_t>(type & 0x3FFF) << 2);     // bits 15:2
    v |= (static_cast<uint32_t>(length & 0x7FFF) << 16);  // bits 30:16
    // bit 31 = 0 (already clear)
    writeUint32LE(v);
}

// End of compound stream object (8-bit, MS-FSSHTTPB 2.2.1.5.3):
//   bits 7:1 = type
//   bit 0    = 1
void FsshttpBinaryWriter::writeEndObject(uint8_t type) {
    uint8_t b = static_cast<uint8_t>((type << 1) | 0x01);
    writeByte(b);
}

// ---------------------------------------------------------------------------
// Base64 encoding (RFC 4648, with '=' padding, no line breaks)
// ---------------------------------------------------------------------------
static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string FsshttpBinaryWriter::toBase64() const {
    const uint8_t* data = m_buf.data();
    size_t len = m_buf.size();
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    size_t i = 0;
    while (i + 2 < len) {
        uint32_t triple = (static_cast<uint32_t>(data[i]) << 16)
                        | (static_cast<uint32_t>(data[i+1]) << 8)
                        |  static_cast<uint32_t>(data[i+2]);
        out += kBase64Chars[(triple >> 18) & 0x3F];
        out += kBase64Chars[(triple >> 12) & 0x3F];
        out += kBase64Chars[(triple >>  6) & 0x3F];
        out += kBase64Chars[ triple        & 0x3F];
        i += 3;
    }
    if (i < len) {
        uint32_t triple = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) triple |= static_cast<uint32_t>(data[i+1]) << 8;
        out += kBase64Chars[(triple >> 18) & 0x3F];
        out += kBase64Chars[(triple >> 12) & 0x3F];
        if (i + 1 < len) {
            out += kBase64Chars[(triple >> 6) & 0x3F];
        } else {
            out += '=';
        }
        out += '=';
    }
    return out;
}

// ---------------------------------------------------------------------------
// FsshttpBinaryReader
// ---------------------------------------------------------------------------

FsshttpBinaryReader::FsshttpBinaryReader(const uint8_t* data, size_t size)
    : m_data(data), m_size(size), m_pos(0) {}

FsshttpBinaryReader::FsshttpBinaryReader(const std::vector<uint8_t>& data)
    : m_data(data.data()), m_size(data.size()), m_pos(0) {}

uint8_t FsshttpBinaryReader::readByte() {
    if (m_pos >= m_size)
        throw std::runtime_error("FsshttpBinaryReader: unexpected end of stream");
    return m_data[m_pos++];
}

void FsshttpBinaryReader::readBytes(uint8_t* out, size_t n) {
    if (m_pos + n > m_size)
        throw std::runtime_error("FsshttpBinaryReader: unexpected end of stream");
    std::memcpy(out, m_data + m_pos, n);
    m_pos += n;
}

uint16_t FsshttpBinaryReader::readUint16LE() {
    uint8_t lo = readByte();
    uint8_t hi = readByte();
    return static_cast<uint16_t>(lo) | (static_cast<uint16_t>(hi) << 8);
}

uint32_t FsshttpBinaryReader::readUint32LE() {
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i)
        v |= static_cast<uint32_t>(readByte()) << (8 * i);
    return v;
}

void FsshttpBinaryReader::readGuid(uint8_t out[16]) {
    readBytes(out, 16);
}

// CompactUnsignedInt64 (MS-FSSHTTPB 2.2.1.1):
// Each byte has 7 value bits in bits[7:1]. Bit 0 = 1 signals the last byte.
uint64_t FsshttpBinaryReader::readUvarint() {
    uint64_t result = 0;
    int shift = 0;
    while (true) {
        uint8_t b = readByte();
        result |= static_cast<uint64_t>((b >> 1) & 0x7F) << shift;
        shift += 7;
        if (b & 0x01) break;  // last byte
        if (shift >= 64)
            throw std::runtime_error("FsshttpBinaryReader: uvarint overflow");
    }
    return result;
}

uint64_t FsshttpBinaryReader::readExGuidN(uint8_t outGuid[16]) {
    readGuid(outGuid);
    return readUvarint();
}

void FsshttpBinaryReader::skip(size_t n) {
    if (m_pos + n > m_size)
        throw std::runtime_error("FsshttpBinaryReader: skip past end of stream");
    m_pos += n;
}

// Reads a stream object header (16-bit or 32-bit).
// Determines type from bit 0 of the first byte:
//   bit 0 == 0 → 16-bit header
//   bit 0 == 1 → 32-bit header
// Returns false on EOF or parse error.
bool FsshttpBinaryReader::readStreamHeader(StreamHeader& out) {
    if (eof()) return false;

    uint8_t firstByte = m_data[m_pos];

    if ((firstByte & 0x01) == 0) {
        // 16-bit header
        if (m_pos + 2 > m_size) return false;
        uint16_t v = readUint16LE();
        out.is32bit   = false;
        out.compound  = (v & 0x0002) != 0;
        out.type      = static_cast<uint16_t>((v >> 2) & 0x3F);
        out.length    = static_cast<uint32_t>((v >> 8) & 0x3F);
    } else {
        // 32-bit header
        if (m_pos + 4 > m_size) return false;
        uint32_t v = readUint32LE();
        out.is32bit   = true;
        out.compound  = (v & 0x00000002) != 0;
        out.type      = static_cast<uint16_t>((v >> 2) & 0x3FFF);
        out.length    = static_cast<uint32_t>((v >> 16) & 0x7FFF);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Base64 decoding (RFC 4648, ignores whitespace, handles '=' padding)
// ---------------------------------------------------------------------------
static int base64CharValue(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;  // invalid / padding / whitespace
}

std::vector<uint8_t> FsshttpBinaryReader::fromBase64(const std::string& b64) {
    std::vector<uint8_t> out;
    out.reserve((b64.size() / 4) * 3);

    int buf = 0;
    int bits = 0;

    for (char c : b64) {
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        // Stop at padding
        if (c == '=') break;

        int val = base64CharValue(c);
        if (val < 0) continue;  // skip unrecognised chars

        buf = (buf << 6) | val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}
