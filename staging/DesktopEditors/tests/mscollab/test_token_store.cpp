#include <gtest/gtest.h>
#include "auth/TokenStore.h"

TEST(TokenStore, StoreAndRetrieveAccessToken) {
    TokenStore store("mscollab-test");
    store.clear();
    store.setAccessToken("test-access-token");
    EXPECT_EQ(store.accessToken(), "test-access-token");
}

TEST(TokenStore, StoreAndRetrieveRefreshToken) {
    TokenStore store("mscollab-test");
    store.clear();
    store.setRefreshToken("test-refresh-token");
    EXPECT_EQ(store.refreshToken(), "test-refresh-token");
}

TEST(TokenStore, ClearRemovesTokens) {
    TokenStore store("mscollab-test");
    store.setAccessToken("token");
    store.clear();
    EXPECT_TRUE(store.accessToken().empty());
    EXPECT_TRUE(store.refreshToken().empty());
}

TEST(TokenStore, ExpiryRoundTrip) {
    TokenStore store("mscollab-test");
    store.clear();
    store.setExpiresAt(9999999999LL);
    EXPECT_EQ(store.expiresAt(), 9999999999LL);
}
