#include "TokenStore.h"
#include <libsecret/secret.h>
#include <chrono>
#include <stdexcept>

static const SecretSchema MSCOLLAB_SCHEMA = {
    "dk.techcollege.mscollab",
    SECRET_SCHEMA_NONE,
    {{"key", SECRET_SCHEMA_ATTRIBUTE_STRING}, {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING}}
};

TokenStore::TokenStore(const std::string& label) : m_label(label) {}

std::string TokenStore::secretGet(const std::string& key) const {
    GError* err = nullptr;
    gchar* val = secret_password_lookup_sync(
        &MSCOLLAB_SCHEMA, nullptr, &err,
        "key", (m_label + "." + key).c_str(), nullptr);
    if (err) { g_error_free(err); return {}; }
    if (!val) return {};
    std::string result(val);
    secret_password_free(val);
    return result;
}

void TokenStore::secretSet(const std::string& key, const std::string& value) {
    GError* err = nullptr;
    secret_password_store_sync(
        &MSCOLLAB_SCHEMA, SECRET_COLLECTION_DEFAULT,
        ("mscollab:" + key).c_str(), value.c_str(),
        nullptr, &err,
        "key", (m_label + "." + key).c_str(), nullptr);
    if (err) { g_error_free(err); }
}

void TokenStore::secretDelete(const std::string& key) {
    GError* err = nullptr;
    secret_password_clear_sync(
        &MSCOLLAB_SCHEMA, nullptr, &err,
        "key", (m_label + "." + key).c_str(), nullptr);
    if (err) g_error_free(err);
}

std::string TokenStore::accessToken() const  { return secretGet("access_token"); }
std::string TokenStore::refreshToken() const { return secretGet("refresh_token"); }

int64_t TokenStore::expiresAt() const {
    auto s = secretGet("expires_at");
    if (s.empty()) return 0;
    return std::stoll(s);
}

void TokenStore::setAccessToken(const std::string& t)  { secretSet("access_token", t); }
void TokenStore::setRefreshToken(const std::string& t) { secretSet("refresh_token", t); }
void TokenStore::setExpiresAt(int64_t ts)              { secretSet("expires_at", std::to_string(ts)); }

void TokenStore::clear() {
    secretDelete("access_token");
    secretDelete("refresh_token");
    secretDelete("expires_at");
}

bool TokenStore::isExpired() const {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return expiresAt() < (now + 60);
}
