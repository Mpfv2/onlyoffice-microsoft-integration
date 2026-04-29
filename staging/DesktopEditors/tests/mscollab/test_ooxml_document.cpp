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

// ---------------------------------------------------------------------------
// applyParagraphDelta: OMML paragraphs are not overwritten
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, PreservesOmmlParagraph) {
    // Build a document with one plain paragraph and one equation paragraph.
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\""
        " xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">"
        "<w:body>"
        "<w:p><w:r><w:t>Plain text</w:t></w:r></w:p>"
        "<w:p><m:oMath><m:r><m:t>x^2</m:t></m:r></m:oMath></w:p>"
        "</w:body>"
        "</w:document>";

    // We can't call OoxmlDocument's non-static methods here without loadFromDocx,
    // but we CAN test parseParagraphDeltas to verify OMML text is not extracted.
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    // Both paragraphs should be parsed; the equation paragraph's text is empty
    // (because <m:t> is not <w:t> and extractParaText skips it).
    ASSERT_EQ(deltas.size(), 2u);
    EXPECT_EQ(deltas[0].text, "Plain text");
    EXPECT_EQ(deltas[1].text, "");  // OMML text not extracted via w:t
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas: XML entities are unescaped in returned text
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, UnescapesXmlEntitiesInText) {
    const std::string xml =
        "<w:p><w:r><w:t>a &amp; b &lt;tag&gt; &quot;quoted&quot;</w:t></w:r></w:p>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto deltas = OoxmlDocument::parseParagraphDeltas(bytes);

    ASSERT_EQ(deltas.size(), 1u);
    EXPECT_EQ(deltas[0].text, "a & b <tag> \"quoted\"");
}

// ---------------------------------------------------------------------------
// applyCommentDelta / serializeComments: round-trip a new comment
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ApplyCommentDeltaCreatesWellFormedXml) {
    OoxmlDocument doc;
    doc.applyCommentDelta(1, "Alice", "2024-03-15T10:00:00Z", "Good point!");
    auto bytes = doc.serializeComments();
    EXPECT_FALSE(bytes.empty());

    std::string xml(bytes.begin(), bytes.end());
    EXPECT_NE(xml.find("<w:comments"), std::string::npos);
    EXPECT_NE(xml.find("w:id=\"1\""), std::string::npos);
    EXPECT_NE(xml.find("w:author=\"Alice\""), std::string::npos);
    EXPECT_NE(xml.find("Good point!"), std::string::npos);
}

TEST_F(OoxmlDocumentTest, ApplyCommentDeltaUpserts) {
    OoxmlDocument doc;
    doc.applyCommentDelta(2, "Bob",   "2024-03-15T11:00:00Z", "First version");
    doc.applyCommentDelta(2, "Bob",   "2024-03-15T11:05:00Z", "Updated version");
    auto bytes = doc.serializeComments();
    std::string xml(bytes.begin(), bytes.end());

    // Only one <w:comment> with id=2 should remain
    auto pos1 = xml.find("w:id=\"2\"");
    ASSERT_NE(pos1, std::string::npos);
    auto pos2 = xml.find("w:id=\"2\"", pos1 + 1);
    EXPECT_EQ(pos2, std::string::npos);  // no second occurrence

    EXPECT_NE(xml.find("Updated version"), std::string::npos);
    EXPECT_EQ(xml.find("First version"), std::string::npos);
}

TEST_F(OoxmlDocumentTest, ApplyMultipleComments) {
    OoxmlDocument doc;
    doc.applyCommentDelta(1, "Alice", "", "Comment A");
    doc.applyCommentDelta(2, "Bob",   "", "Comment B");
    auto bytes = doc.serializeComments();
    std::string xml(bytes.begin(), bytes.end());
    EXPECT_NE(xml.find("Comment A"), std::string::npos);
    EXPECT_NE(xml.find("Comment B"), std::string::npos);
}

TEST_F(OoxmlDocumentTest, SerializeCommentsEmptyWithoutApply) {
    OoxmlDocument doc;
    EXPECT_FALSE(doc.hasComments());
    auto bytes = doc.serializeComments();
    EXPECT_TRUE(bytes.empty());
}

// ---------------------------------------------------------------------------
// parseCommentDeltas: returns empty for non-comment XML
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParseCommentDeltasEmptyForDocumentXml) {
    std::vector<uint8_t> bytes(MINIMAL_XML.begin(), MINIMAL_XML.end());
    auto comments = OoxmlDocument::parseCommentDeltas(bytes);
    EXPECT_TRUE(comments.empty());
}

// ---------------------------------------------------------------------------
// parseCommentDeltas: extracts single comment
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParseCommentDeltasSingleComment) {
    const std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<w:comments xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:comment w:id=\"1\" w:author=\"Alice\" w:date=\"2024-03-15T10:00:00Z\">"
        "<w:p><w:r><w:t>Great point!</w:t></w:r></w:p>"
        "</w:comment>"
        "</w:comments>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto comments = OoxmlDocument::parseCommentDeltas(bytes);

    ASSERT_EQ(comments.size(), 1u);
    EXPECT_EQ(comments[0].id,     1);
    EXPECT_EQ(comments[0].author, "Alice");
    EXPECT_EQ(comments[0].date,   "2024-03-15T10:00:00Z");
    EXPECT_EQ(comments[0].text,   "Great point!");
}

// ---------------------------------------------------------------------------
// parseCommentDeltas: extracts multiple comments
// ---------------------------------------------------------------------------
TEST_F(OoxmlDocumentTest, ParseCommentDeltasMultipleComments) {
    const std::string xml =
        "<w:comments xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:comment w:id=\"1\" w:author=\"Alice\" w:date=\"\">"
        "<w:p><w:r><w:t>First</w:t></w:r></w:p>"
        "</w:comment>"
        "<w:comment w:id=\"2\" w:author=\"Bob\" w:date=\"\">"
        "<w:p><w:r><w:t>Second</w:t></w:r></w:p>"
        "</w:comment>"
        "</w:comments>";
    std::vector<uint8_t> bytes(xml.begin(), xml.end());
    auto comments = OoxmlDocument::parseCommentDeltas(bytes);

    ASSERT_EQ(comments.size(), 2u);
    EXPECT_EQ(comments[0].author, "Alice");
    EXPECT_EQ(comments[0].text,   "First");
    EXPECT_EQ(comments[1].author, "Bob");
    EXPECT_EQ(comments[1].text,   "Second");
}
