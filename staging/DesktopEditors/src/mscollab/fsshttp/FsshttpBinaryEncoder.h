#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Low-level binary writer for the MS-FSSHTTPB format (MS-FSSHTTPB spec).
// All integers are little-endian. Used by FsshttpSerializer to build
// CellSubRequest bodies.
class FsshttpBinaryWriter {
public:
    // CompactUnsignedInt64 (MS-FSSHTTPB 2.2.1.1):
    // Variable-length encoding; each byte stores 7 value bits in bits[7:1].
    // Bit 0 = 1 in the last byte, 0 in all preceding bytes.
    void writeUvarint(uint64_t value);

    // Write raw bytes
    void writeBytes(const uint8_t* data, size_t len);
    void writeByte(uint8_t b);
    void writeUint16LE(uint16_t v);
    void writeUint32LE(uint32_t v);
    void writeUint64LE(uint64_t v);

    // Write a 16-byte GUID as raw bytes
    void writeGuid(const uint8_t guid[16]);

    // ExGUID (MS-FSSHTTPB 2.2.1.5): GUID + CompactUnsignedInt64 value
    void writeExGuid(const uint8_t guid[16], uint64_t n);

    // CellID (MS-FSSHTTPB 2.2.1.7): two ExGUIDs (object space ID, object ID)
    void writeCellId(const uint8_t spaceGuid[16], uint64_t spaceN,
                     const uint8_t objGuid[16],   uint64_t objN);

    // StreamObjectHeader (16-bit, MS-FSSHTTPB 2.2.1.5.1):
    //   bit 0   = 0  (marks as 16-bit header)
    //   bit 1   = compound
    //   bits 7:2 = type (6-bit)
    //   bits 13:8 = length (6-bit, bytes of object body after header)
    //   bits 15:14 = 0b11 (constant)
    // Use for: type <= 0x3F and length <= 0x3F
    void writeStreamHeader16(uint8_t type, uint8_t length, bool compound);

    // StreamObjectHeader (32-bit, MS-FSSHTTPB 2.2.1.5.2):
    //   bit 0   = 1  (marks as 32-bit header)
    //   bit 1   = compound
    //   bits 15:2 = type (14-bit)
    //   bits 30:16 = length (15-bit)
    //   bit 31 = 0
    // Use when type > 0x3F or length > 0x3F
    void writeStreamHeader32(uint16_t type, uint32_t length, bool compound);

    // End of compound stream object (8-bit, MS-FSSHTTPB 2.2.1.5.3):
    //   bits 7:1 = type (must match the opening compound header type)
    //   bit 0 = 1
    void writeEndObject(uint8_t type);

    const std::vector<uint8_t>& bytes() const { return m_buf; }
    size_t size() const { return m_buf.size(); }

    // Returns the accumulated buffer as a base64-encoded string (no line breaks).
    std::string toBase64() const;

private:
    std::vector<uint8_t> m_buf;
};

// Low-level binary reader for the MS-FSSHTTPB format.
class FsshttpBinaryReader {
public:
    FsshttpBinaryReader(const uint8_t* data, size_t size);
    explicit FsshttpBinaryReader(const std::vector<uint8_t>& data);

    uint64_t readUvarint();
    uint8_t  readByte();
    uint16_t readUint16LE();
    uint32_t readUint32LE();
    void     readBytes(uint8_t* out, size_t n);
    void     readGuid(uint8_t out[16]);
    uint64_t readExGuidN(uint8_t outGuid[16]);   // reads GUID + uvarint, returns N

    struct StreamHeader {
        uint16_t type;
        uint32_t length;   // body bytes after header
        bool     compound;
        bool     is32bit;
    };
    // Returns false on end-of-stream or parse error.
    bool readStreamHeader(StreamHeader& out);

    bool eof() const { return m_pos >= m_size; }
    size_t remaining() const { return m_size - m_pos; }
    size_t pos() const { return m_pos; }
    void skip(size_t n);

    // Decode base64 → bytes, then return a reader over the result.
    // The returned vector owns the decoded bytes; pass it to the constructor.
    static std::vector<uint8_t> fromBase64(const std::string& b64);

private:
    const uint8_t* m_data;
    size_t         m_size;
    size_t         m_pos = 0;
};
