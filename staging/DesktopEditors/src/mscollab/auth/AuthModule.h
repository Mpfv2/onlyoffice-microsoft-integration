#pragma once
#include "TokenStore.h"
#include <string>
#include <functional>

// Handles OAuth 2.0 Authorization Code + PKCE flow for Microsoft 365.
// Call authenticate() to trigger the browser-based login.
// After success, accessToken() returns a valid bearer token.
class AuthModule {
public:
    AuthModule(const std::string& tenantDomain, const std::string& clientId);

    // Blocks until auth completes or fails. Opens system browser.
    // Returns true on success.
    bool authenticate();

    // Returns a valid access token, refreshing silently if needed.
    // Returns empty string if not authenticated.
    std::string accessToken();

    bool isAuthenticated() const;
    void logout();

    // Static helpers exposed for testing
    static std::string generateCodeVerifier();
    static std::string codeChallenge(const std::string& verifier);
    std::string        buildAuthUrl(const std::string& verifier, int port) const;

private:
    std::string  m_tenantDomain;
    std::string  m_clientId;
    TokenStore   m_store;

    bool refreshTokens();
    std::string exchangeCode(const std::string& code,
                             const std::string& verifier,
                             int port);
    static std::string base64UrlEncode(const unsigned char* data, size_t len);
};
