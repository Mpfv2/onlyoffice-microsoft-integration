#include <gtest/gtest.h>
#include "auth/AuthModule.h"

TEST(AuthModule, GeneratesPKCEVerifier) {
    auto v1 = AuthModule::generateCodeVerifier();
    auto v2 = AuthModule::generateCodeVerifier();
    EXPECT_GE(v1.size(), 43u);
    EXPECT_LE(v1.size(), 128u);
    EXPECT_NE(v1, v2);
}

TEST(AuthModule, PKCEChallengeIsDeterministic) {
    std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
    std::string expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
    EXPECT_EQ(AuthModule::codeChallenge(verifier), expected);
}

TEST(AuthModule, BuildsAuthUrlWithTenant) {
    AuthModule auth("techcollege.dk", "test-client-id");
    std::string url = auth.buildAuthUrl("verifier123", 9999);
    EXPECT_NE(url.find("techcollege.dk"), std::string::npos);
    EXPECT_NE(url.find("test-client-id"), std::string::npos);
    EXPECT_NE(url.find("9999"), std::string::npos);
    // Must request Sites.ReadWrite.All for FSSHTTP SharePoint access
    EXPECT_NE(url.find("Sites.ReadWrite.All"), std::string::npos);
    EXPECT_NE(url.find("Files.ReadWrite"), std::string::npos);
    EXPECT_NE(url.find("offline_access"), std::string::npos);
}
