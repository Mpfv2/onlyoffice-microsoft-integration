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
