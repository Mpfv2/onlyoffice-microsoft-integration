# OnlyOffice MS-FSSHTTP Co-Authoring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fork OnlyOffice Desktop Editors and add real-time co-authoring with Microsoft Word desktop users via MS-FSSHTTP and Microsoft 365 OneDrive (school tenant: techcollege.dk).

**Architecture:** A C++ Auth module handles OAuth 2.0 PKCE with the school's Azure AD tenant. An MS-FSSHTTP client manages coauthoring sessions with OneDrive, serializing OnlyOffice change events into Microsoft's SOAP/XML delta format. An Integration Bridge hooks into OnlyOffice's existing Qt ↔ JavaScript message bus to route changes in and out without touching the core document engine.

**Tech Stack:** C++17, Qt5, libcurl, libxml2, libsecret, Google Test. Target: Arch Linux. Build system: qmake + upstream build_tools.

---

## File Map

**New files (all under `DesktopEditors/src/mscollab/`):**

| File | Responsibility |
|------|---------------|
| `auth/AuthModule.h/.cpp` | OAuth 2.0 PKCE flow, token refresh, login UI trigger |
| `auth/TokenStore.h/.cpp` | Read/write tokens via libsecret keyring |
| `fsshttp/FsshttpSession.h/.cpp` | Coauthoring session lifecycle (join/refresh/exit) |
| `fsshttp/FsshttpSerializer.h/.cpp` | SOAP/XML encode + decode for all SubRequest types |
| `fsshttp/FsshttpClient.h/.cpp` | Top-level client: wires session + serializer + HTTP |
| `fsshttp/FsshttpDelta.h/.cpp` | OnlyOffice change event ↔ FsshttpCellStorageData |
| `bridge/IntegrationBridge.h/.cpp` | Subclasses CAscApplicationManager, owns the client, routes events |
| `merge/MergeEngine.h/.cpp` | Paragraph-level last-write-wins + tracked-change fallback |
| `mscollab.pri` | qmake include, lists all sources + deps |

**Modified files:**
| File | Change |
|------|--------|
| `DesktopEditors/desktop-sdk.pro` | Add `include(src/mscollab/mscollab.pri)` |
| `DesktopEditors/src/applicationmanager/CAscApplicationManager.h` | Add `IntegrationBridge* m_bridge` member |
| `DesktopEditors/src/applicationmanager/CAscApplicationManager.cpp` | Instantiate bridge on init, delegate open/save |

**New JS file:**
| File | Responsibility |
|------|---------------|
| `sdkjs/common/mscollab-shim.js` | `window.MsCollab` — exposes `onLocalChange` / `applyRemoteChange` |

**Test files:**
| File | Tests |
|------|-------|
| `DesktopEditors/tests/mscollab/test_token_store.cpp` | TokenStore read/write/delete |
| `DesktopEditors/tests/mscollab/test_auth.cpp` | Token refresh logic, PKCE verifier generation |
| `DesktopEditors/tests/mscollab/test_fsshttp_serializer.cpp` | SOAP encode/decode round-trips |
| `DesktopEditors/tests/mscollab/test_fsshttp_session.cpp` | Session state machine |
| `DesktopEditors/tests/mscollab/test_merge.cpp` | Conflict resolution cases |
| `DesktopEditors/tests/mscollab/CMakeLists.txt` | gtest build for mscollab tests |

---

## Task 1: Repo Setup — Submodules + Build Scaffold

**Files:**
- Modify: `onlyoffice-mscollab/` (root)
- Create: `DesktopEditors/src/mscollab/mscollab.pri`
- Modify: `DesktopEditors/desktop-sdk.pro`

- [ ] **Step 1: Fork the upstream repos on GitHub**

  Go to:
  - https://github.com/ONLYOFFICE/DesktopEditors → Fork → name it `DesktopEditors`
  - https://github.com/ONLYOFFICE/sdkjs → Fork → name it `sdkjs`

  Do this under your own GitHub account. You need both forks before continuing.

- [ ] **Step 2: Add submodules**

  ```bash
  cd ~/onlyoffice-mscollab
  git submodule add https://github.com/YOUR_USERNAME/DesktopEditors DesktopEditors
  git submodule add https://github.com/YOUR_USERNAME/sdkjs sdkjs
  git submodule update --init --recursive
  ```

- [ ] **Step 3: Install build dependencies (run on Arch Linux)**

  ```bash
  sudo pacman -S --needed \
    qt5-base qt5-webengine qt5-tools \
    libcurl-gnutls libxml2 libsecret \
    cmake make gcc python3 git
  ```

  Verify:
  ```bash
  pkg-config --modversion libcurl libxml-2.0 libsecret-1
  # Expected: some version strings, no errors
  ```

- [ ] **Step 4: Create the qmake module file**

  Create `DesktopEditors/src/mscollab/mscollab.pri`:
  ```qmake
  INCLUDEPATH += $$PWD

  HEADERS += \
      $$PWD/auth/AuthModule.h \
      $$PWD/auth/TokenStore.h \
      $$PWD/fsshttp/FsshttpClient.h \
      $$PWD/fsshttp/FsshttpSerializer.h \
      $$PWD/fsshttp/FsshttpSession.h \
      $$PWD/fsshttp/FsshttpDelta.h \
      $$PWD/bridge/IntegrationBridge.h \
      $$PWD/merge/MergeEngine.h

  SOURCES += \
      $$PWD/auth/AuthModule.cpp \
      $$PWD/auth/TokenStore.cpp \
      $$PWD/fsshttp/FsshttpClient.cpp \
      $$PWD/fsshttp/FsshttpSerializer.cpp \
      $$PWD/fsshttp/FsshttpSession.cpp \
      $$PWD/fsshttp/FsshttpDelta.cpp \
      $$PWD/bridge/IntegrationBridge.cpp \
      $$PWD/merge/MergeEngine.cpp

  CONFIG += link_pkgconfig
  PKGCONFIG += libcurl libxml-2.0 libsecret-1
  ```

- [ ] **Step 5: Wire the module into the main pro file**

  In `DesktopEditors/desktop-sdk.pro`, find the last `include(...)` line and add after it:
  ```qmake
  include(src/mscollab/mscollab.pri)
  ```

- [ ] **Step 6: Create stub header files so the project compiles**

  Create each of these with empty class stubs. Example for `AuthModule.h`:
  ```cpp
  // DesktopEditors/src/mscollab/auth/AuthModule.h
  #pragma once
  #include <string>

  class AuthModule {
  public:
      AuthModule() = default;
  };
  ```

  Repeat the same empty stub pattern for: `TokenStore.h`, `FsshttpClient.h`, `FsshttpSerializer.h`, `FsshttpSession.h`, `FsshttpDelta.h`, `IntegrationBridge.h`, `MergeEngine.h`.

  Create matching `.cpp` files that just include their header:
  ```cpp
  #include "AuthModule.h"
  ```

- [ ] **Step 7: Create the test CMakeLists**

  Create `DesktopEditors/tests/mscollab/CMakeLists.txt`:
  ```cmake
  cmake_minimum_required(VERSION 3.14)
  project(mscollab_tests)

  find_package(GTest REQUIRED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
  pkg_check_modules(LIBCURL REQUIRED libcurl)

  include_directories(
      ${CMAKE_SOURCE_DIR}/../../src
      ${LIBSECRET_INCLUDE_DIRS}
      ${LIBCURL_INCLUDE_DIRS}
  )

  add_executable(mscollab_tests
      test_token_store.cpp
      test_auth.cpp
      test_fsshttp_serializer.cpp
      test_fsshttp_session.cpp
      test_merge.cpp
      ../../src/mscollab/auth/TokenStore.cpp
      ../../src/mscollab/auth/AuthModule.cpp
      ../../src/mscollab/fsshttp/FsshttpSerializer.cpp
      ../../src/mscollab/fsshttp/FsshttpSession.cpp
      ../../src/mscollab/merge/MergeEngine.cpp
  )

  target_link_libraries(mscollab_tests
      GTest::GTest GTest::Main
      ${LIBSECRET_LIBRARIES}
      ${LIBCURL_LIBRARIES}
      ssl crypto
  )
  ```

- [ ] **Step 8: Verify the project compiles with stubs**

  ```bash
  cd ~/onlyoffice-mscollab/DesktopEditors
  python3 build_tools/build.py --module desktop --platform linux_64 2>&1 | tail -20
  # Expected: build completes with no errors referencing mscollab files
  ```

- [ ] **Step 9: Commit**

  ```bash
  git add DesktopEditors sdkjs
  git commit -m "feat: add submodules and mscollab build scaffold"
  ```



---

## Task 2: TokenStore — Secure Token Persistence

**Files:**
- Create: `DesktopEditors/src/mscollab/auth/TokenStore.h`
- Create: `DesktopEditors/src/mscollab/auth/TokenStore.cpp`
- Create: `DesktopEditors/tests/mscollab/test_token_store.cpp`

- [ ] **Step 1: Write failing tests**

  Create `DesktopEditors/tests/mscollab/test_token_store.cpp`:
  ```cpp
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
  ```

- [ ] **Step 2: Run tests to confirm they fail**

  ```bash
  cd ~/onlyoffice-mscollab/DesktopEditors
  cmake -B build-tests -DBUILD_MSCOLLAB_TESTS=ON && cmake --build build-tests
  ./build-tests/mscollab_tests --gtest_filter="TokenStore*"
  # Expected: link errors — TokenStore not implemented yet
  ```

- [ ] **Step 3: Write the header**

  Replace `DesktopEditors/src/mscollab/auth/TokenStore.h`:
  ```cpp
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
  ```

- [ ] **Step 4: Write the implementation**

  Replace `DesktopEditors/src/mscollab/auth/TokenStore.cpp`:
  ```cpp
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
  ```

- [ ] **Step 5: Run tests and confirm they pass**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="TokenStore*"
  # Expected: 4 tests pass
  ```

- [ ] **Step 6: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/auth/TokenStore.h src/mscollab/auth/TokenStore.cpp \
      tests/mscollab/test_token_store.cpp
  git -C DesktopEditors commit -m "feat: TokenStore — secure token persistence via libsecret"
  ```

---

## Task 3: AuthModule — OAuth 2.0 PKCE Flow

**Files:**
- Create: `DesktopEditors/src/mscollab/auth/AuthModule.h`
- Create: `DesktopEditors/src/mscollab/auth/AuthModule.cpp`
- Create: `DesktopEditors/tests/mscollab/test_auth.cpp`

- [ ] **Step 1: Write failing tests**

  Create `DesktopEditors/tests/mscollab/test_auth.cpp`:
  ```cpp
  #include <gtest/gtest.h>
  #include "auth/AuthModule.h"

  TEST(AuthModule, GeneratesPKCEVerifier) {
      auto v1 = AuthModule::generateCodeVerifier();
      auto v2 = AuthModule::generateCodeVerifier();
      EXPECT_GE(v1.size(), 43u);
      EXPECT_LE(v1.size(), 128u);
      EXPECT_NE(v1, v2);  // must be random
  }

  TEST(AuthModule, PKCEChallengeIsDeterministic) {
      std::string verifier = "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
      // SHA-256(verifier) base64url = expected value per RFC 7636 appendix B
      std::string expected = "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM";
      EXPECT_EQ(AuthModule::codeChallenge(verifier), expected);
  }

  TEST(AuthModule, BuildsAuthUrlWithTenant) {
      AuthModule auth("techcollege.dk", "test-client-id");
      std::string url = auth.buildAuthUrl("verifier123", 9999);
      EXPECT_NE(url.find("techcollege.dk"), std::string::npos);
      EXPECT_NE(url.find("test-client-id"), std::string::npos);
      EXPECT_NE(url.find("9999"), std::string::npos);
  }
  ```

- [ ] **Step 2: Run tests to confirm they fail**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="AuthModule*"
  # Expected: link errors
  ```

- [ ] **Step 3: Write the header**

  Replace `DesktopEditors/src/mscollab/auth/AuthModule.h`:
  ```cpp
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
  ```

- [ ] **Step 4: Write the implementation**

  Create `DesktopEditors/src/mscollab/auth/AuthModule.cpp`:
  ```cpp
  #include "AuthModule.h"
  #include <openssl/sha.h>
  #include <openssl/rand.h>
  #include <curl/curl.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
  #include <chrono>
  #include <thread>
  #include <sstream>
  #include <stdexcept>
  #include <cstring>

  // ---- libcurl write callback ----
  static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* s) {
      s->append((char*)ptr, size * nmemb);
      return size * nmemb;
  }

  AuthModule::AuthModule(const std::string& tenantDomain, const std::string& clientId)
      : m_tenantDomain(tenantDomain), m_clientId(clientId),
        m_store("mscollab." + tenantDomain) {}

  // RFC 4648 base64url (no padding)
  std::string AuthModule::base64UrlEncode(const unsigned char* data, size_t len) {
      static const char* table =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      std::string out;
      for (size_t i = 0; i < len; i += 3) {
          unsigned int v = data[i] << 16;
          if (i+1 < len) v |= data[i+1] << 8;
          if (i+2 < len) v |= data[i+2];
          out += table[(v >> 18) & 0x3F];
          out += table[(v >> 12) & 0x3F];
          out += (i+1 < len) ? table[(v >> 6) & 0x3F] : '=';
          out += (i+2 < len) ? table[v & 0x3F] : '=';
      }
      // base64url: replace + with -, / with _, strip =
      for (auto& c : out) {
          if (c == '+') c = '-';
          else if (c == '/') c = '_';
      }
      out.erase(std::remove(out.begin(), out.end(), '='), out.end());
      return out;
  }

  std::string AuthModule::generateCodeVerifier() {
      unsigned char buf[32];
      RAND_bytes(buf, sizeof(buf));
      return base64UrlEncode(buf, sizeof(buf));
  }

  std::string AuthModule::codeChallenge(const std::string& verifier) {
      unsigned char hash[SHA256_DIGEST_LENGTH];
      SHA256(reinterpret_cast<const unsigned char*>(verifier.data()),
             verifier.size(), hash);
      return base64UrlEncode(hash, SHA256_DIGEST_LENGTH);
  }

  std::string AuthModule::buildAuthUrl(const std::string& verifier, int port) const {
      // Resolve tenant ID from domain using OIDC discovery (simplified: use /common for now)
      std::string redirect = "http%3A%2F%2Flocalhost%3A" + std::to_string(port) + "%2Fcallback";
      return "https://login.microsoftonline.com/" + m_tenantDomain +
             "/oauth2/v2.0/authorize"
             "?client_id=" + m_clientId +
             "&response_type=code"
             "&redirect_uri=" + redirect +
             "&scope=Files.ReadWrite%20offline_access"
             "&code_challenge=" + codeChallenge(verifier) +
             "&code_challenge_method=S256";
  }

  // Starts a loopback HTTP server, returns the auth code from the redirect.
  static std::string waitForCode(int port) {
      int server = socket(AF_INET, SOCK_STREAM, 0);
      int opt = 1;
      setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
      addr.sin_port = htons(port);
      bind(server, (sockaddr*)&addr, sizeof(addr));
      listen(server, 1);
      int client = accept(server, nullptr, nullptr);
      char buf[4096]{};
      recv(client, buf, sizeof(buf)-1, 0);
      // Send a minimal HTML response
      const char* resp = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                         "<h2>Login complete. You can close this tab.</h2>";
      send(client, resp, strlen(resp), 0);
      close(client);
      close(server);
      // Parse code= from request line
      std::string req(buf);
      auto codePos = req.find("code=");
      if (codePos == std::string::npos) return {};
      codePos += 5;
      auto end = req.find_first_of(" &", codePos);
      return req.substr(codePos, end - codePos);
  }

  std::string AuthModule::exchangeCode(const std::string& code,
                                       const std::string& verifier,
                                       int port) {
      std::string redirect = "http://localhost:" + std::to_string(port) + "/callback";
      std::string body =
          "client_id=" + m_clientId +
          "&grant_type=authorization_code"
          "&code=" + code +
          "&redirect_uri=" + redirect +
          "&code_verifier=" + verifier;

      std::string response;
      CURL* curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_URL,
          ("https://login.microsoftonline.com/" + m_tenantDomain +
           "/oauth2/v2.0/token").c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_perform(curl);
      curl_easy_cleanup(curl);
      return response;
  }

  bool AuthModule::authenticate() {
      int port = 49152 + (rand() % 16383);
      std::string verifier = generateCodeVerifier();
      std::string url = buildAuthUrl(verifier, port);

      // Open system browser
      system(("xdg-open '" + url + "' &").c_str());

      std::string code = waitForCode(port);
      if (code.empty()) return false;

      std::string tokenJson = exchangeCode(code, verifier, port);
      // Parse access_token, refresh_token, expires_in from JSON
      // Minimal JSON parsing — find key-value pairs
      auto extract = [&](const std::string& key) -> std::string {
          auto pos = tokenJson.find("\"" + key + "\":");
          if (pos == std::string::npos) return {};
          pos = tokenJson.find('"', pos + key.size() + 3) + 1;
          auto end = tokenJson.find('"', pos);
          return tokenJson.substr(pos, end - pos);
      };
      auto extractNum = [&](const std::string& key) -> int64_t {
          auto pos = tokenJson.find("\"" + key + "\":");
          if (pos == std::string::npos) return 0;
          pos += key.size() + 3;
          auto end = tokenJson.find_first_of(",}", pos);
          return std::stoll(tokenJson.substr(pos, end - pos));
      };

      std::string access  = extract("access_token");
      std::string refresh = extract("refresh_token");
      int64_t expiresIn   = extractNum("expires_in");
      if (access.empty()) return false;

      auto now = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      m_store.setAccessToken(access);
      m_store.setRefreshToken(refresh);
      m_store.setExpiresAt(now + expiresIn);
      return true;
  }

  bool AuthModule::refreshTokens() {
      std::string rt = m_store.refreshToken();
      if (rt.empty()) return false;
      std::string body =
          "client_id=" + m_clientId +
          "&grant_type=refresh_token"
          "&refresh_token=" + rt +
          "&scope=Files.ReadWrite%20offline_access";
      std::string response;
      CURL* curl = curl_easy_init();
      curl_easy_setopt(curl, CURLOPT_URL,
          ("https://login.microsoftonline.com/" + m_tenantDomain +
           "/oauth2/v2.0/token").c_str());
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      auto extract = [&](const std::string& key) -> std::string {
          auto pos = response.find("\"" + key + "\":");
          if (pos == std::string::npos) return {};
          pos = response.find('"', pos + key.size() + 3) + 1;
          auto end = response.find('"', pos);
          return response.substr(pos, end - pos);
      };
      auto extractNum = [&](const std::string& key) -> int64_t {
          auto pos = response.find("\"" + key + "\":");
          if (pos == std::string::npos) return 0;
          pos += key.size() + 3;
          auto end = response.find_first_of(",}", pos);
          return std::stoll(response.substr(pos, end - pos));
      };

      std::string access = extract("access_token");
      if (access.empty()) return false;
      auto now = std::chrono::duration_cast<std::chrono::seconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
      m_store.setAccessToken(access);
      m_store.setExpiresAt(now + extractNum("expires_in"));
      auto newRt = extract("refresh_token");
      if (!newRt.empty()) m_store.setRefreshToken(newRt);
      return true;
  }

  std::string AuthModule::accessToken() {
      if (m_store.isExpired()) {
          if (!refreshTokens()) return {};
      }
      return m_store.accessToken();
  }

  bool AuthModule::isAuthenticated() const {
      return !m_store.accessToken().empty();
  }

  void AuthModule::logout() {
      m_store.clear();
  }
  ```

- [ ] **Step 5: Run tests and confirm they pass**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="AuthModule*"
  # Expected: 3 tests pass
  ```

- [ ] **Step 6: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/auth/AuthModule.h src/mscollab/auth/AuthModule.cpp \
      tests/mscollab/test_auth.cpp
  git -C DesktopEditors commit -m "feat: AuthModule — OAuth 2.0 PKCE with Microsoft 365"
  ```

---

## Task 4: FsshttpSerializer — SOAP/XML Encode + Decode

**Files:**
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpSerializer.h`
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpSerializer.cpp`
- Create: `DesktopEditors/tests/mscollab/test_fsshttp_serializer.cpp`

- [ ] **Step 1: Write failing tests**

  Create `DesktopEditors/tests/mscollab/test_fsshttp_serializer.cpp`:
  ```cpp
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
  ```

- [ ] **Step 2: Run tests to confirm they fail**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="FsshttpSerializer*"
  # Expected: link errors
  ```

- [ ] **Step 3: Write the header**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpSerializer.h`:
  ```cpp
  #pragma once
  #include <string>

  struct JoinResponse {
      bool        success = false;
      std::string sessionToken;
      std::string errorCode;
  };

  struct CoauthorsInfo {
      int         count = 0;
      std::string sessionToken;
  };

  // Encodes MS-FSSHTTP SubRequests as SOAP/XML and decodes SubResponses.
  // All methods are stateless. Reference: [MS-FSSHTTP] sections 2.3–2.9.
  class FsshttpSerializer {
  public:
      std::string encodeJoin(const std::string& fileUrl,
                             const std::string& clientId,
                             const std::string& sessionToken) const;

      std::string encodeRefresh(const std::string& fileUrl,
                                const std::string& clientId,
                                const std::string& sessionToken) const;

      std::string encodeExit(const std::string& fileUrl,
                             const std::string& clientId,
                             const std::string& sessionToken) const;

      JoinResponse   decodeJoinResponse(const std::string& xml) const;
      bool           decodeRefreshResponse(const std::string& xml) const;

  private:
      std::string wrapEnvelope(const std::string& fileUrl,
                               const std::string& subrequest) const;
      std::string xmlAttrValue(const std::string& xml,
                               const std::string& attr) const;
  };
  ```

- [ ] **Step 4: Write the implementation**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpSerializer.cpp`:
  ```cpp
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
      r.errorCode   = xmlAttrValue(xml, "ErrorCode");
      r.sessionToken = xmlAttrValue(xml, "SessionToken");
      r.success = (r.errorCode == "Success" || r.errorCode.empty()) &&
                  !r.sessionToken.empty();
      return r;
  }

  bool FsshttpSerializer::decodeRefreshResponse(const std::string& xml) const {
      auto code = xmlAttrValue(xml, "ErrorCode");
      return code == "Success" || code.empty();
  }
  ```

- [ ] **Step 5: Run tests and confirm they pass**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="FsshttpSerializer*"
  # Expected: 4 tests pass
  ```

- [ ] **Step 6: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/fsshttp/FsshttpSerializer.h \
      src/mscollab/fsshttp/FsshttpSerializer.cpp \
      tests/mscollab/test_fsshttp_serializer.cpp
  git -C DesktopEditors commit -m "feat: FsshttpSerializer — SOAP/XML encode+decode for coauthoring subrequests"
  ```

---

## Task 5: FsshttpSession — Session State Machine

**Files:**
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpSession.h`
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpSession.cpp`
- Create: `DesktopEditors/tests/mscollab/test_fsshttp_session.cpp`

- [ ] **Step 1: Write failing tests**

  Create `DesktopEditors/tests/mscollab/test_fsshttp_session.cpp`:
  ```cpp
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
  ```

- [ ] **Step 2: Run tests to confirm they fail**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="FsshttpSession*"
  # Expected: link errors
  ```

- [ ] **Step 3: Write the header**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpSession.h`:
  ```cpp
  #pragma once
  #include <string>

  // Tracks coauthoring session state.
  // All state transitions happen via handle*() methods.
  class FsshttpSession {
  public:
      enum class State { Disconnected, Joining, Joined, Exiting };

      FsshttpSession(const std::string& fileUrl, const std::string& clientId);

      State       state() const;
      std::string sessionToken() const;
      std::string fileUrl() const;
      std::string clientId() const;
      int         retryCount() const;

      void handleJoinResponse(bool success, const std::string& token);
      void handleRefreshSuccess();
      void handleRefreshFailure();
      void handleExit();
      void setJoining();

      static constexpr int MAX_RETRIES = 3;

  private:
      std::string m_fileUrl;
      std::string m_clientId;
      std::string m_sessionToken;
      State       m_state = State::Disconnected;
      int         m_retryCount = 0;
  };
  ```

- [ ] **Step 4: Write the implementation**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpSession.cpp`:
  ```cpp
  #include "FsshttpSession.h"

  FsshttpSession::FsshttpSession(const std::string& fileUrl, const std::string& clientId)
      : m_fileUrl(fileUrl), m_clientId(clientId) {}

  FsshttpSession::State FsshttpSession::state() const        { return m_state; }
  std::string FsshttpSession::sessionToken() const           { return m_sessionToken; }
  std::string FsshttpSession::fileUrl() const                { return m_fileUrl; }
  std::string FsshttpSession::clientId() const               { return m_clientId; }
  int         FsshttpSession::retryCount() const             { return m_retryCount; }

  void FsshttpSession::setJoining() {
      m_state = State::Joining;
  }

  void FsshttpSession::handleJoinResponse(bool success, const std::string& token) {
      if (success) {
          m_sessionToken = token;
          m_state = State::Joined;
          m_retryCount = 0;
      } else {
          m_sessionToken.clear();
          m_state = State::Disconnected;
      }
  }

  void FsshttpSession::handleRefreshSuccess() {
      m_retryCount = 0;
  }

  void FsshttpSession::handleRefreshFailure() {
      ++m_retryCount;
      if (m_retryCount >= MAX_RETRIES) {
          m_state = State::Disconnected;
          m_sessionToken.clear();
      }
  }

  void FsshttpSession::handleExit() {
      m_state = State::Disconnected;
      m_sessionToken.clear();
      m_retryCount = 0;
  }
  ```

- [ ] **Step 5: Run tests and confirm they pass**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="FsshttpSession*"
  # Expected: 6 tests pass
  ```

- [ ] **Step 6: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/fsshttp/FsshttpSession.h \
      src/mscollab/fsshttp/FsshttpSession.cpp \
      tests/mscollab/test_fsshttp_session.cpp
  git -C DesktopEditors commit -m "feat: FsshttpSession — coauthoring session state machine"
  ```

---

## Task 6: FsshttpClient — HTTP Transport + Session Orchestration

**Files:**
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpClient.h`
- Create: `DesktopEditors/src/mscollab/fsshttp/FsshttpClient.cpp`

- [ ] **Step 1: Write the header**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpClient.h`:
  ```cpp
  #pragma once
  #include "FsshttpSession.h"
  #include "FsshttpSerializer.h"
  #include <string>
  #include <functional>
  #include <thread>
  #include <atomic>

  // Top-level MS-FSSHTTP client.
  // Call joinSession() to enter co-authoring. Heartbeat runs on a background thread.
  // Set onRemoteDelta to receive incoming changes.
  class FsshttpClient {
  public:
      FsshttpClient(const std::string& fileUrl,
                    const std::string& clientId,
                    std::function<std::string()> tokenProvider);
      ~FsshttpClient();

      bool joinSession();
      void exitSession();

      // Called by IntegrationBridge when the user makes a local edit.
      void sendDelta(const std::string& deltaJson);

      // Set before calling joinSession(). Called on the heartbeat thread.
      std::function<void(const std::string& deltaJson)> onRemoteDelta;
      std::function<void()> onSessionDropped;

      bool isJoined() const;

  private:
      FsshttpSession    m_session;
      FsshttpSerializer m_serializer;
      std::function<std::string()> m_tokenProvider;

      std::thread   m_heartbeatThread;
      std::atomic<bool> m_running{false};

      std::string post(const std::string& xml);
      std::string endpointUrl() const;
      void        heartbeatLoop();
  };
  ```

- [ ] **Step 2: Write the implementation**

  Create `DesktopEditors/src/mscollab/fsshttp/FsshttpClient.cpp`:
  ```cpp
  #include "FsshttpClient.h"
  #include <curl/curl.h>
  #include <chrono>
  #include <thread>

  static size_t curlWrite(void* ptr, size_t size, size_t nmemb, std::string* s) {
      s->append((char*)ptr, size * nmemb);
      return size * nmemb;
  }

  FsshttpClient::FsshttpClient(const std::string& fileUrl,
                               const std::string& clientId,
                               std::function<std::string()> tokenProvider)
      : m_session(fileUrl, clientId), m_tokenProvider(std::move(tokenProvider)) {}

  FsshttpClient::~FsshttpClient() {
      exitSession();
  }

  // MS-FSSHTTP endpoint: derive from file's web URL.
  // OneDrive for Business URLs follow the pattern:
  //   https://{tenant}-my.sharepoint.com/personal/{upn}/Documents/file.docx
  // Endpoint: base host + "/_vti_bin/vti_aut/author.dll"
  std::string FsshttpClient::endpointUrl() const {
      const auto& url = m_session.fileUrl();
      auto slash = url.find('/', 8);  // skip https://
      auto host = url.substr(0, slash);
      return host + "/_vti_bin/vti_aut/author.dll";
  }

  std::string FsshttpClient::post(const std::string& xml) {
      std::string token = m_tokenProvider();
      if (token.empty()) return {};
      std::string response;
      CURL* curl = curl_easy_init();
      struct curl_slist* headers = nullptr;
      headers = curl_slist_append(headers, "Content-Type: text/xml; charset=utf-8");
      headers = curl_slist_append(headers, ("Authorization: Bearer " + token).c_str());
      headers = curl_slist_append(headers, "X-WOPI-SessionToken: " "");  // set if needed

      curl_easy_setopt(curl, CURLOPT_URL, endpointUrl().c_str());
      curl_easy_setopt(curl, CURLOPT_POST, 1L);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, xml.c_str());
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWrite);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
      curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
      curl_easy_perform(curl);
      curl_slist_free_all(headers);
      curl_easy_cleanup(curl);
      return response;
  }

  bool FsshttpClient::joinSession() {
      m_session.setJoining();
      auto xml = m_serializer.encodeJoin(
          m_session.fileUrl(), m_session.clientId(), "");
      auto resp = post(xml);
      auto result = m_serializer.decodeJoinResponse(resp);
      m_session.handleJoinResponse(result.success, result.sessionToken);
      if (!result.success) return false;

      m_running = true;
      m_heartbeatThread = std::thread(&FsshttpClient::heartbeatLoop, this);
      return true;
  }

  void FsshttpClient::exitSession() {
      m_running = false;
      if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
      if (m_session.state() == FsshttpSession::State::Joined) {
          auto xml = m_serializer.encodeExit(
              m_session.fileUrl(), m_session.clientId(), m_session.sessionToken());
          post(xml);
          m_session.handleExit();
      }
  }

  void FsshttpClient::heartbeatLoop() {
      while (m_running) {
          std::this_thread::sleep_for(std::chrono::seconds(30));
          if (!m_running) break;
          auto xml = m_serializer.encodeRefresh(
              m_session.fileUrl(), m_session.clientId(), m_session.sessionToken());
          auto resp = post(xml);
          if (m_serializer.decodeRefreshResponse(resp)) {
              m_session.handleRefreshSuccess();
          } else {
              m_session.handleRefreshFailure();
              if (m_session.state() == FsshttpSession::State::Disconnected) {
                  if (onSessionDropped) onSessionDropped();
                  break;
              }
          }
      }
  }

  void FsshttpClient::sendDelta(const std::string& /*deltaJson*/) {
      // CellSubRequest (FsshttpCellStorageData) binary encoding — Phase 2 (see Known Gaps)
  }

  bool FsshttpClient::isJoined() const {
      return m_session.state() == FsshttpSession::State::Joined;
  }
  ```

- [ ] **Step 3: Confirm the project still compiles**

  ```bash
  cd ~/onlyoffice-mscollab/DesktopEditors
  python3 build_tools/build.py --module desktop --platform linux_64 2>&1 | grep -E "error:|warning:" | head -20
  # Expected: no errors in mscollab files
  ```

- [ ] **Step 4: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/fsshttp/FsshttpClient.h \
      src/mscollab/fsshttp/FsshttpClient.cpp
  git -C DesktopEditors commit -m "feat: FsshttpClient — HTTP transport and session orchestration"
  ```

---

## Task 7: MergeEngine — Conflict Resolution

**Files:**
- Create: `DesktopEditors/src/mscollab/merge/MergeEngine.h`
- Create: `DesktopEditors/src/mscollab/merge/MergeEngine.cpp`
- Create: `DesktopEditors/tests/mscollab/test_merge.cpp`

- [ ] **Step 1: Write failing tests**

  Create `DesktopEditors/tests/mscollab/test_merge.cpp`:
  ```cpp
  #include <gtest/gtest.h>
  #include "merge/MergeEngine.h"

  TEST(MergeEngine, NoConflictReturnsRemoteDelta) {
      MergeEngine m;
      // Local changed para 1, remote changed para 2 — no conflict
      MergeEngine::Delta local  = {1, "hello world"};
      MergeEngine::Delta remote = {2, "foo bar"};
      auto result = m.merge(local, remote);
      EXPECT_EQ(result.action, MergeEngine::Action::Apply);
      EXPECT_EQ(result.paragraphIndex, 2);
  }

  TEST(MergeEngine, SameParagraphRemoteWins) {
      MergeEngine m;
      MergeEngine::Delta local  = {3, "local text"};
      MergeEngine::Delta remote = {3, "remote text"};
      auto result = m.merge(local, remote);
      EXPECT_EQ(result.action, MergeEngine::Action::Apply);
      EXPECT_EQ(result.content, "remote text");
  }

  TEST(MergeEngine, StructuralConflictBecomesTrackedChange) {
      MergeEngine m;
      MergeEngine::Delta local  = {5, "STRUCTURAL_DELETE"};
      MergeEngine::Delta remote = {5, "STRUCTURAL_INSERT"};
      auto result = m.merge(local, remote);
      EXPECT_EQ(result.action, MergeEngine::Action::TrackedChange);
  }
  ```

- [ ] **Step 2: Run tests to confirm they fail**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="MergeEngine*"
  # Expected: link errors
  ```

- [ ] **Step 3: Write the header**

  Create `DesktopEditors/src/mscollab/merge/MergeEngine.h`:
  ```cpp
  #pragma once
  #include <string>

  class MergeEngine {
  public:
      struct Delta {
          int         paragraphIndex = -1;
          std::string content;
      };

      enum class Action { Apply, TrackedChange, Discard };

      struct MergeResult {
          Action      action = Action::Apply;
          int         paragraphIndex = -1;
          std::string content;
      };

      // Returns what to do with the remote delta given a simultaneous local delta.
      MergeResult merge(const Delta& local, const Delta& remote) const;

  private:
      static bool isStructural(const std::string& content);
  };
  ```

- [ ] **Step 4: Write the implementation**

  Create `DesktopEditors/src/mscollab/merge/MergeEngine.cpp`:
  ```cpp
  #include "MergeEngine.h"

  bool MergeEngine::isStructural(const std::string& content) {
      return content.find("STRUCTURAL_") == 0;
  }

  MergeEngine::MergeResult MergeEngine::merge(const Delta& local,
                                              const Delta& remote) const {
      MergeResult r;
      r.paragraphIndex = remote.paragraphIndex;
      r.content = remote.content;

      if (local.paragraphIndex != remote.paragraphIndex) {
          r.action = Action::Apply;
          return r;
      }
      // Same paragraph — structural conflict becomes tracked change
      if (isStructural(local.content) && isStructural(remote.content)) {
          r.action = Action::TrackedChange;
          return r;
      }
      // Text conflict — remote wins (last-write-wins)
      r.action = Action::Apply;
      return r;
  }
  ```

- [ ] **Step 5: Run tests and confirm they pass**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests --gtest_filter="MergeEngine*"
  # Expected: 3 tests pass
  ```

- [ ] **Step 6: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/merge/MergeEngine.h \
      src/mscollab/merge/MergeEngine.cpp \
      tests/mscollab/test_merge.cpp
  git -C DesktopEditors commit -m "feat: MergeEngine — paragraph-level conflict resolution"
  ```

---

## Task 8: sdkjs Shim — JS ↔ Native Bridge

**Files:**
- Create: `sdkjs/common/mscollab-shim.js`
- Modify: `sdkjs/common/AllFonts.js` (or the main entry point that loads common scripts — find by running `grep -r "AllFonts" sdkjs/` to confirm the loader)

- [ ] **Step 1: Create the shim**

  Create `sdkjs/common/mscollab-shim.js`:
  ```js
  (function () {
      'use strict';

      if (typeof window === 'undefined' || typeof window.AscDesktopEditor === 'undefined') {
          return; // not running inside OnlyOffice desktop shell
      }

      window.MsCollab = {
          // Called by sdkjs when the user makes a local edit.
          // delta: serialized change event from asc_docs_api
          onLocalChange: function (delta) {
              window.AscDesktopEditor.sendToNative(
                  'onOutgoingChange',
                  typeof delta === 'string' ? delta : JSON.stringify(delta)
              );
          },

          // Called from native (C++) when a remote delta arrives.
          // delta: JSON string of a change event
          applyRemoteChange: function (deltaJson) {
              try {
                  var delta = typeof deltaJson === 'string'
                      ? JSON.parse(deltaJson) : deltaJson;
                  if (window.Asc && window.Asc.plugin) return; // ignore if in plugin context
                  window.asc_docs_api && window.asc_docs_api.asc_ApplyChanges(delta);
              } catch (e) {
                  console.error('[MsCollab] applyRemoteChange failed:', e);
              }
          }
      };

      // Hook into the document change event emitted by the editing engine.
      // The event name may vary by OnlyOffice version — verify against sdkjs source.
      document.addEventListener('asc_onDocumentContentReady', function () {
          if (!window.asc_docs_api) return;
          window.asc_docs_api.asc_registerCallback('asc_onDocumentChanged', function (data) {
              window.MsCollab.onLocalChange(data);
          });
      });
  }());
  ```

- [ ] **Step 2: Find the script loader and add the shim**

  ```bash
  grep -r "AllFonts\|loadScripts\|common/" sdkjs/web-apps/apps/documenteditor/main/index.html | head -10
  ```

  Locate where common JS files are loaded and add:
  ```html
  <script type="text/javascript" src="../../../common/mscollab-shim.js"></script>
  ```
  Add this line after the last existing `<script>` include from `common/`.

- [ ] **Step 3: Verify the shim loads without errors**

  Build and launch OnlyOffice Desktop. Open any `.docx`. Open browser DevTools (`F12` in the web view) and run:
  ```js
  typeof window.MsCollab
  // Expected: "object"
  ```

- [ ] **Step 4: Commit**

  ```bash
  git -C sdkjs add common/mscollab-shim.js
  git -C sdkjs commit -m "feat: MsCollab shim — JS side of native bridge"
  git add sdkjs
  git commit -m "chore: update sdkjs submodule"
  ```

---

## Task 9: IntegrationBridge — Wire Everything Together

**Files:**
- Create: `DesktopEditors/src/mscollab/bridge/IntegrationBridge.h`
- Create: `DesktopEditors/src/mscollab/bridge/IntegrationBridge.cpp`
- Modify: `DesktopEditors/src/applicationmanager/CAscApplicationManager.h`
- Modify: `DesktopEditors/src/applicationmanager/CAscApplicationManager.cpp`

- [ ] **Step 1: Write the header**

  Create `DesktopEditors/src/mscollab/bridge/IntegrationBridge.h`:
  ```cpp
  #pragma once
  #include "../auth/AuthModule.h"
  #include "../fsshttp/FsshttpClient.h"
  #include "../merge/MergeEngine.h"
  #include <string>
  #include <memory>
  #include <functional>

  // Owns the FsshttpClient and AuthModule.
  // Connects OnlyOffice document events to the MS-FSSHTTP protocol layer.
  class IntegrationBridge {
  public:
      IntegrationBridge();

      // Called when user opens a file. If it's a OneDrive path, starts co-auth session.
      void onDocumentOpened(const std::string& filePath);

      // Called when user closes a document.
      void onDocumentClosed(const std::string& filePath);

      // Called by the Qt ↔ JS event bus when the JS shim fires onOutgoingChange.
      void onOutgoingChange(const std::string& deltaJson);

      // Set by CAscApplicationManager to push incoming changes into sdkjs.
      std::function<void(const std::string& deltaJson)> sendToJs;

  private:
      AuthModule              m_auth;
      std::unique_ptr<FsshttpClient> m_client;
      MergeEngine             m_merge;

      bool isOneDrivePath(const std::string& path) const;
      std::string resolveOneDriveUrl(const std::string& localPath) const;
  };
  ```

- [ ] **Step 2: Write the implementation**

  Create `DesktopEditors/src/mscollab/bridge/IntegrationBridge.cpp`:
  ```cpp
  #include "IntegrationBridge.h"
  #include <iostream>

  static const std::string CLIENT_ID = "YOUR_AZURE_APP_CLIENT_ID"; // set via config in Task 10

  IntegrationBridge::IntegrationBridge()
      : m_auth("techcollege.dk", CLIENT_ID) {}

  bool IntegrationBridge::isOneDrivePath(const std::string& path) const {
      return path.find("OneDrive") != std::string::npos ||
             path.find("sharepoint.com") != std::string::npos;
  }

  // For now, returns the path directly. In production, resolve via Graph API
  // GET /me/drive/root:/relative/path — returns the item's webUrl for FSSHTTP.
  std::string IntegrationBridge::resolveOneDriveUrl(const std::string& localPath) const {
      return localPath;
  }

  void IntegrationBridge::onDocumentOpened(const std::string& filePath) {
      if (!isOneDrivePath(filePath)) return;

      if (!m_auth.isAuthenticated()) {
          if (!m_auth.authenticate()) {
              std::cerr << "[MsCollab] Authentication failed — local only mode\n";
              return;
          }
      }

      std::string fileUrl = resolveOneDriveUrl(filePath);
      std::string clientId = "onlyoffice-" + std::to_string(
          std::hash<std::string>{}(filePath));

      m_client = std::make_unique<FsshttpClient>(
          fileUrl, clientId,
          [this]() { return m_auth.accessToken(); });

      m_client->onRemoteDelta = [this](const std::string& delta) {
          if (sendToJs) sendToJs(delta);
      };

      m_client->onSessionDropped = [this]() {
          std::cerr << "[MsCollab] Session dropped — attempting rejoin\n";
          // Re-join is handled by FsshttpClient's retry logic
      };

      if (!m_client->joinSession()) {
          std::cerr << "[MsCollab] Failed to join coauthoring session\n";
          m_client.reset();
      }
  }

  void IntegrationBridge::onDocumentClosed(const std::string& /*filePath*/) {
      if (m_client) {
          m_client->exitSession();
          m_client.reset();
      }
  }

  void IntegrationBridge::onOutgoingChange(const std::string& deltaJson) {
      if (!m_client || !m_client->isJoined()) return;
      m_client->sendDelta(deltaJson);
  }
  ```

- [ ] **Step 3: Hook into CAscApplicationManager**

  In `DesktopEditors/src/applicationmanager/CAscApplicationManager.h`, find the class declaration and add:
  ```cpp
  // At top of file, add include:
  #include "mscollab/bridge/IntegrationBridge.h"

  // Inside the class, add member:
  private:
      IntegrationBridge m_mscollabBridge;
  ```

  In `DesktopEditors/src/applicationmanager/CAscApplicationManager.cpp`, find the method that handles document open events (look for `onDocumentOpen` or equivalent — run `grep -n "OpenDocument\|openDocument\|documentOpen" src/applicationmanager/CAscApplicationManager.cpp | head -10` to locate it), and add at the end of the open handler:
  ```cpp
  m_mscollabBridge.onDocumentOpened(sFilePath.toStdString());
  ```

  Similarly for document close — find the close handler and add:
  ```cpp
  m_mscollabBridge.onDocumentClosed(sFilePath.toStdString());
  ```

  For the JS → native message routing, find where `sendToNative` messages are dispatched (search for `onOutgoingChange` or the native message handler), and add:
  ```cpp
  if (sMessageName == "onOutgoingChange") {
      m_mscollabBridge.onOutgoingChange(sMessageData.toStdString());
  }
  ```

  Find the method OnlyOffice uses to execute JS in its web view. Run:
  ```bash
  grep -rn "executeJS\|evaluateJavaScript\|sendToRenderer\|CallJSMethod" \
      src/applicationmanager/ | head -10
  ```
  Use whichever method appears. It will look like one of:
  ```cpp
  // Option A (CEF-based):
  m_mscollabBridge.sendToJs = [this](const std::string& delta) {
      std::string escaped = delta; // delta is already valid JSON
      GetFrame()->ExecuteJavaScript(
          "window.MsCollab.applyRemoteChange(" + escaped + ")", "", 0);
  };

  // Option B (Qt WebChannel):
  m_mscollabBridge.sendToJs = [this](const std::string& delta) {
      emit jsChannelMessage("applyRemoteChange",
                            QString::fromStdString(delta));
  };
  ```
  Pick the one that matches the pattern found by grep.

- [ ] **Step 4: Build and confirm no errors**

  ```bash
  python3 build_tools/build.py --module desktop --platform linux_64 2>&1 | grep -E "^.*error:" | head -20
  # Expected: zero errors
  ```

- [ ] **Step 5: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/bridge/IntegrationBridge.h \
      src/mscollab/bridge/IntegrationBridge.cpp \
      src/applicationmanager/CAscApplicationManager.h \
      src/applicationmanager/CAscApplicationManager.cpp
  git -C DesktopEditors commit -m "feat: IntegrationBridge — wire auth, FSSHTTP, and sdkjs events"
  ```

---

## Task 10: Azure App Registration + Config

**Files:**
- Create: `DesktopEditors/src/mscollab/config.h`
- Modify: `DesktopEditors/src/mscollab/bridge/IntegrationBridge.cpp`

- [ ] **Step 1: Register an Azure app for the school tenant**

  Go to https://portal.azure.com → Azure Active Directory → App Registrations → New Registration:
  - Name: `OnlyOffice MSCollab`
  - Supported account types: `Accounts in this organizational directory only (techcollege.dk)`
  - Redirect URI: `http://localhost` (type: Public client / native)

  After creation, note the **Application (client) ID**.

  Under **API Permissions** → Add permission → Microsoft Graph → Delegated:
  - `Files.ReadWrite`
  - `offline_access`

  Click **Grant admin consent** (or ask your school IT admin to do this).

- [ ] **Step 2: Create the config header**

  Create `DesktopEditors/src/mscollab/config.h`:
  ```cpp
  #pragma once
  #include <string>

  namespace MsCollabConfig {
      // Replace with the client ID from your Azure app registration
      inline const std::string CLIENT_ID   = "REPLACE_WITH_YOUR_CLIENT_ID";
      inline const std::string TENANT      = "techcollege.dk";
      inline const std::string ONEDRIVE_FOLDER = "OneDrive - TECHCOLLEGE";
  }
  ```

- [ ] **Step 3: Replace the hardcoded CLIENT_ID in IntegrationBridge**

  In `IntegrationBridge.cpp`, change:
  ```cpp
  // Before:
  static const std::string CLIENT_ID = "YOUR_AZURE_APP_CLIENT_ID";
  // ...
  : m_auth("techcollege.dk", CLIENT_ID) {}

  // After:
  #include "../config.h"
  // ...
  : m_auth(MsCollabConfig::TENANT, MsCollabConfig::CLIENT_ID) {}
  ```

- [ ] **Step 4: Commit**

  ```bash
  git -C DesktopEditors add src/mscollab/config.h src/mscollab/bridge/IntegrationBridge.cpp
  git -C DesktopEditors commit -m "feat: Azure app config — client ID and tenant settings"
  ```

---

## Task 11: End-to-End Smoke Test

- [ ] **Step 1: Run a full build**

  ```bash
  cd ~/onlyoffice-mscollab/DesktopEditors
  python3 build_tools/build.py --module desktop --platform linux_64
  # Expected: build succeeds, binary at build/desktop/linux_64/onlyoffice/DesktopEditors
  ```

- [ ] **Step 2: Launch OnlyOffice**

  ```bash
  ./build/desktop/linux_64/onlyoffice/DesktopEditors
  ```

- [ ] **Step 3: Test auth flow**

  Open a `.docx` file located inside your OneDrive folder (`~/OneDrive - TECHCOLLEGE/`).
  Expected: browser opens to Microsoft login page → you log in → browser shows "Login complete" → file opens in OnlyOffice.

- [ ] **Step 4: Test session join**

  After opening the file, check the terminal output.
  Expected: no `Authentication failed` or `Failed to join coauthoring session` messages.

- [ ] **Step 5: Test with a classmate**

  Have a classmate open the same file in Word desktop on Windows via OneDrive sync.
  Make a text edit in OnlyOffice and save.
  Expected: change appears in Word within the next autosave cycle (~10-30s).

- [ ] **Step 6: Run the full test suite**

  ```bash
  cmake --build build-tests && ./build-tests/mscollab_tests
  # Expected: all tests pass
  ```

- [ ] **Step 7: Final commit**

  ```bash
  cd ~/onlyoffice-mscollab
  git add .
  git commit -m "feat: complete phase 1 OnlyOffice MS-FSSHTTP co-authoring integration"
  ```

---

## Known Gaps (Phase 2)

- `FsshttpDelta.h/.cpp` — full `FsshttpCellStorageData` binary encoding for granular deltas. Phase 1 uses file-level sync; this enables true keystroke-level co-authoring.
- Equation (OMML) change sync
- Comments and tracked changes sync
- Excel and PowerPoint support
- "Open from OneDrive" toolbar button with Graph API file picker
