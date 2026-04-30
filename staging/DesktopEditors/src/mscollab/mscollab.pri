INCLUDEPATH += $$PWD

QT += widgets

# IntegrationBridge uses std::make_unique (C++14). Pin C++17 so we get
# uniform behaviour regardless of what desktop-sdk.pro inherits — and to
# keep the same standard the test CMakeLists asks for.
CONFIG += c++17

HEADERS += \
    $$PWD/auth/AuthModule.h \
    $$PWD/auth/TokenStore.h \
    $$PWD/fsshttp/FsshttpBinaryEncoder.h \
    $$PWD/fsshttp/FsshttpCellSubRequest.h \
    $$PWD/fsshttp/FsshttpClient.h \
    $$PWD/fsshttp/FsshttpSerializer.h \
    $$PWD/fsshttp/FsshttpSession.h \
    $$PWD/fsshttp/OoxmlDocument.h \
    $$PWD/graph/GraphApiClient.h \
    $$PWD/ui/OneDriveDialog.h \
    $$PWD/bridge/IntegrationBridge.h \
    $$PWD/merge/MergeEngine.h

SOURCES += \
    $$PWD/auth/AuthModule.cpp \
    $$PWD/auth/TokenStore.cpp \
    $$PWD/fsshttp/FsshttpBinaryEncoder.cpp \
    $$PWD/fsshttp/FsshttpCellSubRequest.cpp \
    $$PWD/fsshttp/FsshttpClient.cpp \
    $$PWD/fsshttp/FsshttpSerializer.cpp \
    $$PWD/fsshttp/FsshttpSession.cpp \
    $$PWD/fsshttp/OoxmlDocument.cpp \
    $$PWD/graph/GraphApiClient.cpp \
    $$PWD/ui/OneDriveDialog.cpp \
    $$PWD/bridge/IntegrationBridge.cpp \
    $$PWD/merge/MergeEngine.cpp

CONFIG += link_pkgconfig
# libxml2 was removed when FsshttpSerializer dropped XPath/parser usage; we now
# use std::string find for SOAP/XML attribute extraction.
# OpenSSL: AuthModule uses SHA256 + RAND_bytes for PKCE.  Linking via openssl.pc
# (libssl + libcrypto) rather than bare -lssl -lcrypto so build picks up the
# correct include path on hosts where OpenSSL lives outside /usr/include.
PKGCONFIG += libcurl libsecret-1 libzip openssl
