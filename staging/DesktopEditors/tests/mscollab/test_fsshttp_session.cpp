#include <gtest/gtest.h>
#include "fsshttp/FsshttpSession.h"
#include <string>

// ---------------------------------------------------------------------------
// endpointUrl construction: verifies the path-stripping algorithm used by
// FsshttpClient::endpointUrl() (replicated here since it's a private method).
// The endpoint must be at the site-collection root, not the host root, so
// that SharePoint can resolve the FSSHTTP handler.
// ---------------------------------------------------------------------------
static std::string computeEndpointUrl(const std::string& url) {
    size_t hostStart = url.find("://");
    if (hostStart == std::string::npos) return url;
    hostStart += 3;
    size_t pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos)
        return url + "/_vti_bin/cellstorage.svc";
    size_t siteRoot = pathStart;
    for (int segments = 0; segments < 2 && siteRoot < url.size(); ++segments) {
        siteRoot = url.find('/', siteRoot + 1);
        if (siteRoot == std::string::npos) break;
    }
    if (siteRoot == std::string::npos) siteRoot = url.size();
    return url.substr(0, siteRoot) + "/_vti_bin/cellstorage.svc";
}

TEST(FsshttpClient, EndpointUrlIncludesPersonalSitePath) {
    const std::string fileUrl =
        "https://techcollege-my.sharepoint.com/personal/elias_techcollege_dk"
        "/Documents/Homework.docx";
    std::string endpoint = computeEndpointUrl(fileUrl);
    EXPECT_EQ(endpoint,
        "https://techcollege-my.sharepoint.com/personal/elias_techcollege_dk"
        "/_vti_bin/cellstorage.svc");
}

TEST(FsshttpClient, EndpointUrlHostOnlyFallback) {
    const std::string fileUrl = "https://example.sharepoint.com/nodocs";
    // No third slash → fallback appends to the full URL
    std::string endpoint = computeEndpointUrl(fileUrl);
    EXPECT_EQ(endpoint,
        "https://example.sharepoint.com/nodocs/_vti_bin/cellstorage.svc");
}

TEST(FsshttpSession, InitialStateIsDisconnected) {
    FsshttpSession s("file-url", "client-id");
    EXPECT_EQ(s.state(), FsshttpSession::State::Disconnected);
}

TEST(FsshttpSession, TransitionsToJoinedAfterSuccessfulJoin) {
    FsshttpSession s("file-url", "client-id");
    s.handleJoinResponse(true, "token-123");
    EXPECT_EQ(s.state(), FsshttpSession::State::Joined);
    EXPECT_EQ(s.sessionToken(), "token-123");
}

TEST(FsshttpSession, TransitionsToDisconnectedAfterFailedJoin) {
    FsshttpSession s("file-url", "client-id");
    s.handleJoinResponse(false, "");
    EXPECT_EQ(s.state(), FsshttpSession::State::Disconnected);
}

TEST(FsshttpSession, ExitFromJoinedGoesToDisconnected) {
    FsshttpSession s("file-url", "client-id");
    s.handleJoinResponse(true, "token-xyz");
    s.handleExit();
    EXPECT_EQ(s.state(), FsshttpSession::State::Disconnected);
    EXPECT_TRUE(s.sessionToken().empty());
}

TEST(FsshttpSession, RetryCountIncreasesOnFailure) {
    FsshttpSession s("file-url", "client-id");
    s.handleJoinResponse(true, "tok");
    s.handleRefreshFailure();
    s.handleRefreshFailure();
    EXPECT_EQ(s.retryCount(), 2);
}

TEST(FsshttpSession, RetryCountResetsOnSuccess) {
    FsshttpSession s("file-url", "client-id");
    s.handleJoinResponse(true, "tok");
    s.handleRefreshFailure();
    s.handleRefreshSuccess();
    EXPECT_EQ(s.retryCount(), 0);
}
