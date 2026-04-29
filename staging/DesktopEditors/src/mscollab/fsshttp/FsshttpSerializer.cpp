#include "FsshttpSerializer.h"
#include "FsshttpBinaryEncoder.h"
#include "FsshttpCellSubRequest.h"
#include <sstream>
#include <cstdint>

static std::string xmlEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if      (c == '<')  out += "&lt;";
        else if (c == '>')  out += "&gt;";
        else if (c == '&')  out += "&amp;";
        else if (c == '"')  out += "&quot;";
        else                out += c;
    }
    return out;
}

std::string FsshttpSerializer::wrapEnvelope(const std::string& fileUrl,
                                            const std::string& subrequest) const {
    return
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<soap:Envelope xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<soap:Body>"
        "<RequestCollection xmlns=\"http://schemas.microsoft.com/sharepoint/soap/\" "
                           "Version=\"2.2\">"
        "<Request Url=\"" + xmlEscape(fileUrl) + "\" "
                "RequestToken=\"1\">"
        "<SubRequestCollection>" +
        subrequest +
        "</SubRequestCollection>"
        "</Request>"
        "</RequestCollection>"
        "</soap:Body>"
        "</soap:Envelope>";
}

std::string FsshttpSerializer::encodeJoin(const std::string& fileUrl,
                                          const std::string& clientId,
                                          const std::string& sessionToken) const {
    std::string sub =
        "<SubRequest Type=\"Coauthoring\" SubRequestToken=\"1\">"
        "<SubRequestData CoauthRequestType=\"JoinCoauthoringSession\" "
                        "ClientId=\"" + xmlEscape(clientId) + "\" "
                        "SessionToken=\"" + xmlEscape(sessionToken) + "\" "
                        "SchemaLockId=\"{00000000-0000-0000-0000-000000000000}\" "
                        "Timeout=\"3600\"/>"
        "</SubRequest>";
    return wrapEnvelope(fileUrl, sub);
}

std::string FsshttpSerializer::encodeRefresh(const std::string& fileUrl,
                                             const std::string& clientId,
                                             const std::string& sessionToken) const {
    std::string sub =
        "<SubRequest Type=\"Coauthoring\" SubRequestToken=\"1\">"
        "<SubRequestData CoauthRequestType=\"RefreshCoauthoringSession\" "
                        "ClientId=\"" + xmlEscape(clientId) + "\" "
                        "SessionToken=\"" + xmlEscape(sessionToken) + "\" "
                        "Timeout=\"3600\"/>"
        "</SubRequest>";
    return wrapEnvelope(fileUrl, sub);
}

std::string FsshttpSerializer::encodeExit(const std::string& fileUrl,
                                          const std::string& clientId,
                                          const std::string& sessionToken) const {
    std::string sub =
        "<SubRequest Type=\"Coauthoring\" SubRequestToken=\"1\">"
        "<SubRequestData CoauthRequestType=\"ExitCoauthoringSession\" "
                        "ClientId=\"" + xmlEscape(clientId) + "\" "
                        "SessionToken=\"" + xmlEscape(sessionToken) + "\"/>"
        "</SubRequest>";
    return wrapEnvelope(fileUrl, sub);
}

std::string FsshttpSerializer::xmlAttrValue(const std::string& xml,
                                            const std::string& attr) const {
    auto pos = xml.find(attr + "=\"");
    if (pos == std::string::npos) return {};
    pos += attr.size() + 2;
    auto end = xml.find('"', pos);
    if (end == std::string::npos) return {};
    return xml.substr(pos, end - pos);
}

JoinResponse FsshttpSerializer::decodeJoinResponse(const std::string& xml) const {
    JoinResponse r;
    r.errorCode    = xmlAttrValue(xml, "ErrorCode");
    r.sessionToken = xmlAttrValue(xml, "SessionToken");
    r.success = (r.errorCode == "Success" || r.errorCode.empty()) &&
                !r.sessionToken.empty();
    return r;
}

bool FsshttpSerializer::decodeRefreshResponse(const std::string& xml) const {
    auto code = xmlAttrValue(xml, "ErrorCode");
    return code == "Success" || code.empty();
}

std::string FsshttpSerializer::encodeCellPutChanges(
        const std::string& fileUrl,
        const std::string& clientId,
        const std::string& sessionToken,
        const std::string& base64Payload,
        uint32_t           payloadBytes) const {
    std::string sub =
        "<SubRequest Type=\"Cell\" SubRequestToken=\"2\" "
                    "ClientId=\"" + xmlEscape(clientId) + "\" "
                    "SessionToken=\"" + xmlEscape(sessionToken) + "\">"
        "<SubRequestData PartitionID=\"00000001-0000-0000-0000-000000000000\" "
                        "BinaryDataSize=\"" + std::to_string(payloadBytes) + "\">"
        "<BinaryData Value=\"" + base64Payload + "\"/>"
        "</SubRequestData>"
        "</SubRequest>";
    return wrapEnvelope(fileUrl, sub);
}

std::string FsshttpSerializer::encodeCellGetChanges(
        const std::string& fileUrl,
        const std::string& clientId,
        const std::string& sessionToken,
        const std::string& knowledgeB64) const {
    std::string knowledgeAttr;
    if (!knowledgeB64.empty())
        knowledgeAttr = " CurrentKnowledge=\"" + knowledgeB64 + "\"";

    std::string sub =
        "<SubRequest Type=\"Cell\" SubRequestToken=\"2\" "
                    "ClientId=\"" + xmlEscape(clientId) + "\" "
                    "SessionToken=\"" + xmlEscape(sessionToken) + "\">"
        "<SubRequestData PartitionID=\"00000001-0000-0000-0000-000000000000\" "
                        "BinaryDataSize=\"0\"" + knowledgeAttr + "/>"
        "</SubRequest>";
    return wrapEnvelope(fileUrl, sub);
}

CellResponse FsshttpSerializer::decodeCellSubResponse(const std::string& xml) const {
    CellResponse r;
    auto code = xmlAttrValue(xml, "ErrorCode");
    r.success = (code == "Success" || code.empty());
    if (!r.success) return r;

    // Extract knowledge token so the next GetChanges is a true delta fetch.
    r.currentKnowledgeB64 = xmlAttrValue(xml, "CurrentKnowledge");

    // Extract OOXML content from Cell SubResponse BinaryData elements.
    // Each Value attribute is a base64-encoded FSSHTTPB binary; walk it to
    // find ObjectGroupObjectData (type 0x3C) payloads that contain OOXML bytes.
    auto pos = xml.find("<BinaryData ");
    while (pos != std::string::npos) {
        std::string b64 = xmlAttrValue(xml.substr(pos), "Value");
        if (!b64.empty()) {
            auto fsshttpbBytes = FsshttpBinaryReader::fromBase64(b64);
            if (!fsshttpbBytes.empty()) {
                for (auto& blob : FsshttpCellSubRequest::extractOoxmlBlobs(fsshttpbBytes))
                    r.dataBlobs.push_back(std::move(blob));
            }
        }
        pos = xml.find("<BinaryData ", pos + 12);
    }
    return r;
}
