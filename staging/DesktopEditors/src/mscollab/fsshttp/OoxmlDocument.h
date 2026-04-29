#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Maintains in-memory copy of word/document.xml from a .docx file.
// Used by FsshttpClient to produce outgoing PutChanges payloads and
// to decode incoming GetChanges data blobs.
class OoxmlDocument {
public:
    // Load word/document.xml from a .docx ZIP archive (requires libzip).
    // Returns false if the file cannot be opened or has no word/document.xml.
    bool loadFromDocx(const std::string& docxPath);

    // True after a successful loadFromDocx call.
    bool isLoaded() const { return !m_documentXml.empty(); }

    // Replace paragraph at paragraphIndex with new text content.
    // Paragraph indices are 0-based and match OnlyOffice's paragraphIndex.
    // If paragraphIndex >= current paragraph count, appends a new paragraph.
    void applyParagraphDelta(int paragraphIndex, const std::string& text);

    // Return the current word/document.xml as bytes, suitable for embedding
    // as the cell payload in a FsshttpCellSubRequest::buildPutChanges call.
    std::vector<uint8_t> serialize() const;

    struct ParagraphDelta {
        int         index;
        std::string text;
    };

    struct CommentDelta {
        int         id;
        std::string author;
        std::string date;    // ISO 8601 from w:date attribute, may be empty
        std::string text;    // concatenated <w:t> content from the comment body
    };

    // Parse OOXML bytes received from a GetChanges DataElement blob.
    // Extracts text from each <w:p> element and returns one ParagraphDelta per paragraph.
    // Empty paragraphs are included with empty text (so the caller can blank them out).
    static std::vector<ParagraphDelta> parseParagraphDeltas(
        const std::vector<uint8_t>& ooxmlBytes);

    // Parse a word/comments.xml blob.
    // Returns one CommentDelta per <w:comment> element.
    // Returns empty vector if the blob does not look like a comments part.
    static std::vector<CommentDelta> parseCommentDeltas(
        const std::vector<uint8_t>& commentXmlBytes);

    // Add or replace a comment in the in-memory comments XML.
    // Creates the comments document if it hasn't been initialised yet.
    void applyCommentDelta(int id, const std::string& author,
                           const std::string& date, const std::string& text);

    // Serialise the current word/comments.xml as bytes.
    // Returns empty vector if no comments have been applied.
    std::vector<uint8_t> serializeComments() const;

    bool hasComments() const { return !m_commentsXml.empty(); }

private:
    std::string m_documentXml;
    std::string m_commentsXml;  // word/comments.xml; empty until first applyCommentDelta

    // Extract text from all <w:t> children of a <w:p> block.
    static std::string extractParaText(const std::string& para);
};
