include(CTest)
include(GTest)

add_executable(whstest test.cpp builder.cpp raw.cpp)

add_test(NAME AllInOneTest COMMAND whstest WORKING_DIRECTORY ${OUT_PATH})

target_include_directories(whstest PUBLIC ${GTEST_INCLUDE_DIR} ../)

target_link_libraries(
    whstest
    PUBLIC libgtest
           libgmock
           libgtest_main
           whs
           OpenSSL::Crypto
           ${HTTP_PARSER_LIBRARIES}
           ${TEST_LIBRARY})
