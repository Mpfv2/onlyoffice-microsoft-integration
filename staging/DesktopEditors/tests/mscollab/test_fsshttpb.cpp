#include "../../src/mscollab/fsshttp/FsshttpBinaryEncoder.h"
#include "../../src/mscollab/fsshttp/FsshttpCellSubRequest.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
#include <cstdint>

// ---------------------------------------------------------------------------
// Test 1: FsshttpBinaryWriter_Uvarint
//
// Write several CompactUnsignedInt64 values via FsshttpBinaryWriter, then read
// them back with FsshttpBinaryReader and assert the round-trip is exact.
// ---------------------------------------------------------------------------
static void test_uvarint() {
    const uint64_t testValues[] = { 0, 1, 127, 128, 16383, 16384 };
    const size_t N = sizeof(testValues) / sizeof(testValues[0]);

    FsshttpBinaryWriter w;
    for (size_t i = 0; i < N; ++i)
        w.writeUvarint(testValues[i]);

    FsshttpBinaryReader r(w.bytes());
    for (size_t i = 0; i < N; ++i) {
        uint64_t got = r.readUvarint();
        assert(got == testValues[i] && "Uvarint round-trip mismatch");
    }
    assert(r.eof() && "Reader should be at EOF after consuming all uvarints");
    std::printf("PASS: FsshttpBinaryWriter_Uvarint\n");
}

// ---------------------------------------------------------------------------
// Test 2: FsshttpBinaryWriter_StreamHeader16
//
// Write a non-compound 16-bit stream object header with type=0x10, length=0x08,
// read it back and assert all fields are correct.
// ---------------------------------------------------------------------------
static void test_stream_header16() {
    FsshttpBinaryWriter w;
    w.writeStreamHeader16(/*type=*/0x10, /*length=*/0x08, /*compound=*/false);

    FsshttpBinaryReader r(w.bytes());
    FsshttpBinaryReader::StreamHeader hdr;
    bool ok = r.readStreamHeader(hdr);

    assert(ok               && "readStreamHeader should succeed");
    assert(!hdr.is32bit     && "Should be parsed as a 16-bit header");
    assert(hdr.type   == 0x10 && "type should be 0x10");
    assert(hdr.length == 0x08 && "length should be 8");
    assert(!hdr.compound    && "compound should be false");
    assert(r.eof()          && "Reader should be at EOF after header");

    std::printf("PASS: FsshttpBinaryWriter_StreamHeader16\n");
}

// ---------------------------------------------------------------------------
// Test 3: FsshttpCellSubRequest_PutChanges_IsNonEmpty
//
// Call buildPutChanges with a small payload and verify:
//   - The result is non-empty.
//   - Its length is divisible by 4 (valid base64 with padding).
// ---------------------------------------------------------------------------
static void test_put_changes_non_empty() {
    const std::vector<uint8_t> docxBytes = { 0x41, 0x42, 0x43 };  // "ABC"
    const std::string storageToken = "tok";

    std::string b64 = FsshttpCellSubRequest::buildPutChanges(docxBytes, storageToken);

    assert(!b64.empty()          && "buildPutChanges result should be non-empty");
    assert(b64.size() % 4 == 0   && "base64 result length should be divisible by 4");

    // Additionally verify it is valid base64: all characters in alphabet or '='
    for (char c : b64) {
        bool valid = (c >= 'A' && c <= 'Z')
                  || (c >= 'a' && c <= 'z')
                  || (c >= '0' && c <= '9')
                  || c == '+'
                  || c == '/'
                  || c == '=';
        assert(valid && "base64 result contains invalid character");
    }

    std::printf("PASS: FsshttpCellSubRequest_PutChanges_IsNonEmpty (len=%zu)\n",
                b64.size());
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    test_uvarint();
    test_stream_header16();
    test_put_changes_non_empty();
    std::printf("All tests passed.\n");
    return 0;
}
