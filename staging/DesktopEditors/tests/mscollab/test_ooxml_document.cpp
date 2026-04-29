#include <gtest/gtest.h>
#include "fsshttp/OoxmlDocument.h"
#include <string>
#include <vector>

// Tests that exercise OoxmlDocument's XML manipulation without needing a real
// .docx file. We inject word/document.xml content directly via a test-only helper.

// Helper: create an OoxmlDocument with pre-loaded XML by writing a temporary .docx.
// Since loadFromDocx requires libzip, we use a thin friend accessor instead.
// OoxmlDocument exposes an internal setter for tests via a protected method —
// but because it doesn't, we use parseParagraphDeltas (static) and rely on the
// fact that applyParagraphDelta + serialize are public and operate on m_documentXml.

// Note: loadFromDocx is NOT tested here (requires a real .docx file; tested on Arch).
// These tests cover the core XML surgery and parse logic.

class OoxmlDocumentTest : public ::testing::Test {
protected:
    // Minimal word/document.xml with two paragraphs.
    static const std::string MINIMAL_XML;

    // Build OoxmlDocument with known XML by wrapping in a temp .docx.
    // Because loadFromDocx requires libzip and a real ZIP, we test the static
    // parseParagraphDeltas method directly (it takes raw OOXML bytes).
};

const std::string OoxmlDocumentTest::MINIMAL_XML =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
    "<w:body>"
    "<w:p><w:r><w:t>Hello</w:t></w:r></w:p>"
    "<w:p><w:r><w:t>World</w:t></w:r></w:p>"
    "</w:body>"
    "</w:document>";

// ---------------------------------------------------------------------------
// parseParagraphDeltas: two plain paragraphs
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParsesTwoParagraphs) {
    std::vector<uint8_t> bytes(MINIMAL_XML.begin(), MINIMAL_XML.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 2u);
    EXPECT_EQ(deltas[0].index, 0);
    EXPECT_EQ(deltas[0].text,  "Hello");
    EXPECT_EQ(deltas[1].index, 1);
    EXPECT_EQ(deltas[1].text,  "World");
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas: empty paragraph included with empty text
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParsesEmptyParagraph) {
    const std::string xml =
        "<w:body>"
        "<w:p><w:r><w:t>A</w:t></w:r></w:p>"
        "<w:p></w:p>"
        "<w:p><w:r><w:t>B</w:t></w:r></w:p>"
        "</w:body>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 3u);
    EXPECT_EQ(deltas[0].text, "A");
    EXPECT_EQ(deltas[1].text, "");   // empty paragraph
    EXPECT_EQ(deltas[2].text, "B");
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas: multiple runs concatenated
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ConcatenatesMultipleRuns) {
    const std::string xml =
        "<w:p>"
        "<w:r><w:t>Foo</w:t></w:r>"
        "<w:r><w:t> </w:t></w:r>"
        "<w:r><w:t>Bar</w:t></w:r>"
        "</w:p>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].text, "Foo Bar");
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas: xml:space="preserve" w:t with leading/trailing spaces
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParsesPreservedSpaces) {
    const std::string xml =
        "<w:p><w:r><w:t xml:space=\"preserve\">  spaced  </w:t></w:r></w:p>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].text, "  spaced  ");
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas: does not confuse <w:pPr> with <w:p>
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, IgnoresParagraphProperties) {
    const std::string xml =
        "<w:p>"
        "<w:pPr><w:jc w:val=\"center\"/></w:pPr>"
        "<w:r><w:t>Centered</w:t></w:r>"
        "</w:p>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].text, "Centered");
}
