INCLUDEPATH += $$PWD

QT += widgets

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
PKGCONFIG += libcurl libxml-2.0 libsecret-1 libzip
