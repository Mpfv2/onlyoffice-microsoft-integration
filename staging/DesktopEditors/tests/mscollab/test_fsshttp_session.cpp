#include <gtest/gtest.h>
#include "fsshttp/FsshttpSession.h"

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
