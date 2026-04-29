#include "OoxmlDocument.h"
#include <zip.h>
#include <algorithm>
#include <cstring>

static std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// loadFromDocx
// ---------------------------------------------------------------------------
bool OoxmlDocument::loadFromDocx(const std::string& docxPath)
{
    int err = 0;
    zip_t* za = zip_open(docxPath.c_str(), ZIP_RDONLY, &err);
    if (!za) return false;

    zip_int64_t idx = zip_name_locate(za, "word/document.xml", 0);
    if (idx < 0) { zip_close(za); return false; }

    zip_stat_t sb{};
    zip_stat_index(za, idx, 0, &sb);

    zip_file_t* zf = zip_fopen_index(za, idx, 0);
    if (!zf) { zip_close(za); return false; }

    m_documentXml.resize(static_cast<size_t>(sb.size));
    zip_fread(zf, m_documentXml.data(), sb.size);
    zip_fclose(zf);
    zip_close(za);
    return true;
}

// ---------------------------------------------------------------------------
// applyParagraphDelta
// ---------------------------------------------------------------------------

// Returns the position of the Nth <w:p ...> opening tag (not <w:pPr>, etc.).
// Returns std::string::npos if there are fewer than (paragraphIndex+1) paragraphs.
static size_t findNthParagraphStart(const std::string& xml, int n)
{
    int count = 0;
    size_t pos = 0;
    while (pos < xml.size()) {
        size_t found = xml.find("<w:p", pos);
        if (found == std::string::npos) break;

        // The character immediately after "<w:p" must be '>', ' ', or '\n'
        // to distinguish <w:p> from <w:pPr>, <w:pStyle>, etc.
        size_t afterTag = found + 4; // length of "<w:p"
        if (afterTag < xml.size()) {
            char c = xml[afterTag];
            if (c == '>' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                if (count == n) return found;
                ++count;
            }
        }
        pos = found + 1;
    }
    return std::string::npos;
}

// Count total <w:p ...> elements (same rules as above).
static int countParagraphs(const std::string& xml)
{
    int count = 0;
    size_t pos = 0;
    while (pos < xml.size()) {
        size_t found = xml.find("<w:p", pos);
        if (found == std::string::npos) break;
        size_t afterTag = found + 4;
        if (afterTag < xml.size()) {
            char c = xml[afterTag];
            if (c == '>' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                ++count;
            }
        }
        pos = found + 1;
    }
    return count;
}

void OoxmlDocument::applyParagraphDelta(int paragraphIndex, const std::string& text)
{
    const std::string newPara =
        "<w:p><w:r><w:rPr/><w:t xml:space=\"preserve\">" + xmlEscape(text) + "</w:t></w:r></w:p>";

    size_t paraStart = findNthParagraphStart(m_documentXml, paragraphIndex);

    if (paraStart == std::string::npos) {
        // paragraphIndex is beyond the current paragraph count — append before </w:body>
        size_t bodyEnd = m_documentXml.rfind("</w:body>");
        if (bodyEnd == std::string::npos) {
            // No </w:body> found; just append at end
            m_documentXml += newPara;
        } else {
            m_documentXml.insert(bodyEnd, newPara);
        }
        return;
    }

    // Find the matching </w:p> starting from paraStart.
    // OOXML does not nest <w:p> elements, so the first </w:p> after paraStart
    // closes this paragraph.
    size_t closeTag = m_documentXml.find("</w:p>", paraStart);
    if (closeTag == std::string::npos) {
        // Malformed XML — replace from paraStart to end of string
        m_documentXml.replace(paraStart, m_documentXml.size() - paraStart, newPara);
        return;
    }

    const std::string closeStr = "</w:p>";
    size_t replaceEnd = closeTag + closeStr.size();
    m_documentXml.replace(paraStart, replaceEnd - paraStart, newPara);
}

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------
std::vector<uint8_t> OoxmlDocument::serialize() const
{
    return std::vector<uint8_t>(m_documentXml.begin(), m_documentXml.end());
}

// ---------------------------------------------------------------------------
// extractParaText  (private, static)
// ---------------------------------------------------------------------------
std::string OoxmlDocument::extractParaText(const std::string& para)
{
    std::string result;
    size_t pos = 0;
    while (pos < para.size()) {
        // Find next <w:t or <w:t> or <w:t xml:space=...>
        size_t tagStart = para.find("<w:t", pos);
        if (tagStart == std::string::npos) break;

        // Make sure the character after "<w:t" is '>' or ' ' (not e.g. "<w:tbl>")
        size_t afterTag = tagStart + 4;
        if (afterTag >= para.size()) break;
        char c = para[afterTag];
        if (c != '>' && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            pos = tagStart + 1;
            continue;
        }

        // Advance past the opening tag (find closing '>')
        size_t tagClose = para.find('>', tagStart);
        if (tagClose == std::string::npos) break;

        size_t textStart = tagClose + 1;

        // Find </w:t>
        size_t textEnd = para.find("</w:t>", textStart);
        if (textEnd == std::string::npos) break;

        result += para.substr(textStart, textEnd - textStart);
        pos = textEnd + 6; // length of "</w:t>"
    }
    return result;
}

// ---------------------------------------------------------------------------
// parseParagraphDeltas  (static)
// ---------------------------------------------------------------------------
std::vector<OoxmlDocument::ParagraphDelta> OoxmlDocument::parseParagraphDeltas(
    const std::vector<uint8_t>& ooxmlBytes)
{
    const std::string xml(reinterpret_cast<const char*>(ooxmlBytes.data()), ooxmlBytes.size());

    std::vector<ParagraphDelta> deltas;
    int paraIndex = 0;
    size_t pos = 0;

    while (pos < xml.size()) {
        // Find start of a <w:p ...> element
        size_t paraStart = std::string::npos;
        while (pos < xml.size()) {
            size_t found = xml.find("<w:p", pos);
            if (found == std::string::npos) { pos = xml.size(); break; }
            size_t afterTag = found + 4;
            if (afterTag < xml.size()) {
                char c = xml[afterTag];
                if (c == '>' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                    paraStart = found;
                    pos = found + 1;
                    break;
                }
            }
            pos = found + 1;
        }

        if (paraStart == std::string::npos) break;

        // Find matching </w:p>
        size_t closeTag = xml.find("</w:p>", paraStart);
        if (closeTag == std::string::npos) break;

        size_t paraEnd = closeTag + 6; // length of "</w:p>"
        std::string paraBlock = xml.substr(paraStart, paraEnd - paraStart);

        ParagraphDelta delta;
        delta.index = paraIndex++;
        delta.text  = extractParaText(paraBlock);
        deltas.push_back(std::move(delta));

        pos = paraEnd;
    }

    return deltas;
}

// ---------------------------------------------------------------------------
// parseCommentDeltas  (static)
// ---------------------------------------------------------------------------
// Parses a word/comments.xml blob. Each <w:comment> element is extracted into
// a CommentDelta. Returns empty vector if the blob is not a comments part.
//
// Expected structure:
//   <w:comments ...>
//     <w:comment w:id="1" w:author="Alice" w:date="2024-01-01T00:00:00Z">
//       <w:p><w:r><w:t>Comment text here</w:t></w:r></w:p>
//     </w:comment>
//   </w:comments>
// ---------------------------------------------------------------------------

// Extract an XML attribute value from an opening tag string.
// tagContent: the content between '<' and '>' of the tag.
static std::string xmlAttr(const std::string& tagContent, const std::string& attr) {
    const std::string search = attr + "=\"";
    auto pos = tagContent.find(search);
    if (pos == std::string::npos) return {};
    pos += search.size();
    auto end = tagContent.find('"', pos);
    if (end == std::string::npos) return {};
    return tagContent.substr(pos, end - pos);
}

// ---------------------------------------------------------------------------
// applyCommentDelta
// ---------------------------------------------------------------------------
void OoxmlDocument::applyCommentDelta(int id, const std::string& author,
                                       const std::string& date, const std::string& text)
{
    const std::string ns =
        "xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"";

    // Initialise the document skeleton on first use.
    if (m_commentsXml.empty()) {
        m_commentsXml =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<w:comments " + ns + ">"
            "</w:comments>";
    }

    // Build the new <w:comment> element.
    std::string entry =
        "<w:comment w:id=\"" + std::to_string(id) + "\""
        " w:author=\"" + xmlEscape(author) + "\""
        " w:date=\"" + xmlEscape(date) + "\">"
        "<w:p><w:r><w:t xml:space=\"preserve\">" + xmlEscape(text) + "</w:t></w:r></w:p>"
        "</w:comment>";

    // Remove an existing entry with the same id, then insert before </w:comments>.
    const std::string idAttr = "w:id=\"" + std::to_string(id) + "\"";
    auto existStart = m_commentsXml.find("<w:comment " + idAttr);
    if (existStart == std::string::npos)
        existStart = m_commentsXml.find("<w:comment w:id=\"" + std::to_string(id) + "\"");
    if (existStart != std::string::npos) {
        auto existEnd = m_commentsXml.find("</w:comment>", existStart);
        if (existEnd != std::string::npos)
            m_commentsXml.erase(existStart, (existEnd + 12) - existStart);
    }

    auto closeTag = m_commentsXml.rfind("</w:comments>");
    if (closeTag != std::string::npos)
        m_commentsXml.insert(closeTag, entry);
    else
        m_commentsXml += entry;
}

// ---------------------------------------------------------------------------
// serializeComments
// ---------------------------------------------------------------------------
std::vector<uint8_t> OoxmlDocument::serializeComments() const
{
    if (m_commentsXml.empty()) return {};
    return std::vector<uint8_t>(m_commentsXml.begin(), m_commentsXml.end());
}

// ---------------------------------------------------------------------------
// parseCommentDeltas  (static)
// ---------------------------------------------------------------------------
std::vector<OoxmlDocument::CommentDelta> OoxmlDocument::parseCommentDeltas(
    const std::vector<uint8_t>& commentXmlBytes)
{
    const std::string xml(reinterpret_cast<const char*>(commentXmlBytes.data()),
                          commentXmlBytes.size());

    // Quick check: must contain the comments root element
    if (xml.find("<w:comments") == std::string::npos) return {};

    std::vector<CommentDelta> deltas;
    size_t pos = 0;

    while (pos < xml.size()) {
        // Find the next <w:comment opening tag
        size_t tagStart = xml.find("<w:comment ", pos);
        if (tagStart == std::string::npos) break;

        // Find closing '>' of the opening tag
        size_t tagEnd = xml.find('>', tagStart);
        if (tagEnd == std::string::npos) break;
        bool selfClosing = (tagEnd > 0 && xml[tagEnd - 1] == '/');

        std::string openTag = xml.substr(tagStart + 1, tagEnd - tagStart - 1);

        CommentDelta d;
        std::string idStr = xmlAttr(openTag, "w:id");
        d.id     = idStr.empty() ? -1 : std::stoi(idStr);
        d.author = xmlAttr(openTag, "w:author");
        d.date   = xmlAttr(openTag, "w:date");

        if (selfClosing) {
            pos = tagEnd + 1;
        } else {
            // Find the closing </w:comment>
            size_t closeTag = xml.find("</w:comment>", tagEnd);
            if (closeTag == std::string::npos) break;
            // Extract the body and collect all <w:t> text
            std::string body = xml.substr(tagEnd + 1, closeTag - tagEnd - 1);
            d.text = extractParaText(body);  // reuse paragraph text extractor
            pos = closeTag + 12; // length of "</w:comment>"
        }

        deltas.push_back(std::move(d));
    }

    return deltas;
}
