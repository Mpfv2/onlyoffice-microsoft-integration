cmake_minimum_required(VERSION 3.14)
project(mscollab_tests)

find_package(GTest REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBZIP REQUIRED libzip)

include_directories(
    ${CMAKE_SOURCE_DIR}/../../src
    ${CMAKE_SOURCE_DIR}/../../src/mscollab
    ${LIBSECRET_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIRS}
)

add_executable(mscollab_tests
    test_token_store.cpp
    test_auth.cpp
    test_fsshttp_serializer.cpp
    test_fsshttp_session.cpp
    test_merge.cpp
    test_graph_api_client.cpp
    ../../src/mscollab/auth/TokenStore.cpp
    ../../src/mscollab/auth/AuthModule.cpp
    ../../src/mscollab/fsshttp/FsshttpSerializer.cpp
    ../../src/mscollab/fsshttp/FsshttpSession.cpp
    ../../src/mscollab/fsshttp/FsshttpBinaryEncoder.cpp
    ../../src/mscollab/fsshttp/FsshttpCellSubRequest.cpp
    ../../src/mscollab/fsshttp/OoxmlDocument.cpp
    ../../src/mscollab/merge/MergeEngine.cpp
    ../../src/mscollab/graph/GraphApiClient.cpp
)

target_link_libraries(mscollab_tests
    GTest::GTest GTest::Main
    ${LIBSECRET_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBZIP_LIBRARIES}
    ssl crypto
)

# Standalone binary encoder tests (uses plain assert/main, not GTest)
add_executable(mscollab_fsshttpb_tests
    test_fsshttpb.cpp
    ../../src/mscollab/fsshttp/FsshttpBinaryEncoder.cpp
    ../../src/mscollab/fsshttp/FsshttpCellSubRequest.cpp
    ../../src/mscollab/fsshttp/OoxmlDocument.cpp
)
target_link_libraries(mscollab_fsshttpb_tests
    ${LIBCURL_LIBRARIES}
    ${LIBZIP_LIBRARIES}
)
