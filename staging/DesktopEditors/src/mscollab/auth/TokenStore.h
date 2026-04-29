#pragma once
#include <string>
#include <cstdint>

// Persists OAuth tokens in the system keyring via libsecret.
// Each instance is scoped to a keyring label prefix.
class TokenStore {
public:
    explicit TokenStore(const std::string& label);

    std::string accessToken() const;
    std::string refreshToken() const;
    int64_t     expiresAt() const;   // Unix timestamp

    void setAccessToken(const std::string& token);
    void setRefreshToken(const std::string& token);
    void setExpiresAt(int64_t unixTs);

    void clear();
    bool isExpired() const;  // true if expiresAt < now + 60s buffer

private:
    std::string m_label;
    std::string secretGet(const std::string& key) const;
    void        secretSet(const std::string& key, const std::string& value);
    void        secretDelete(const std::string& key);
};
