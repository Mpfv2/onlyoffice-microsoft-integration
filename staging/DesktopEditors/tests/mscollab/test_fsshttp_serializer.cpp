#include <gtest/gtest.h>
#include "fsshttp/FsshttpSerializer.h"

TEST(FsshttpSerializer, EncodeJoinRequest) {
    FsshttpSerializer s;
    auto xml = s.encodeJoin("https://tenant-my.sharepoint.com/file.docx",
                            "client-id-123", "session-token-abc");
    EXPECT_NE(xml.find("JoinCoauthoringSession"), std::string::npos);
    EXPECT_NE(xml.find("client-id-123"), std::string::npos);
}

TEST(FsshttpSerializer, EncodeExitRequest) {
    FsshttpSerializer s;
    auto xml = s.encodeExit("https://tenant-my.sharepoint.com/file.docx",
                            "client-id-123", "session-token-abc");
    EXPECT_NE(xml.find("ExitCoauthoringSession"), std::string::npos);
}

TEST(FsshttpSerializer, DecodeJoinResponse) {
    FsshttpSerializer s;
    std::string resp = R"(<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <ResponseCollection>
      <Response>
        <SubResponseCollection>
          <SubResponse Type="Coauthoring">
            <SubResponseData CoauthRequestType="JoinCoauthoringSession"
                             SessionToken="tok-xyz" ErrorCode="Success"/>
          </SubResponse>
        </SubResponseCollection>
      </Response>
    </ResponseCollection>
  </soap:Body>
</soap:Envelope>)";
    auto result = s.decodeJoinResponse(resp);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.sessionToken, "tok-xyz");
}

TEST(FsshttpSerializer, DecodeJoinResponseError) {
    FsshttpSerializer s;
    std::string resp = R"(<?xml version="1.0"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <ResponseCollection>
      <Response>
        <SubResponseCollection>
          <SubResponse Type="Coauthoring">
            <SubResponseData CoauthRequestType="JoinCoauthoringSession"
                             ErrorCode="LockNotGranted"/>
          </SubResponse>
        </SubResponseCollection>
      </Response>
    </ResponseCollection>
  </soap:Body>
</soap:Envelope>)";
    auto result = s.decodeJoinResponse(resp);
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, "LockNotGranted");
}
