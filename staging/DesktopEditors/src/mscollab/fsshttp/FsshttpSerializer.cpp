#include "FsshttpSerializer.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <sstream>

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
