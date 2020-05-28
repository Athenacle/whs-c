find_package(Threads REQUIRED)
include(FetchContent)
#include(ExternalProject)

set(GTEST_ROOT ${THIRD_PARTY_DIR}/gtest/root CACHE FILEPATH "")

ExternalProject_Add(
    gtest
    URL https://github.com/google/googletest/archive/release-1.8.1.zip
    URL_HASH SHA256=927827c183d01734cc5cfef85e0ff3f5a92ffe6188e0d18e909c5efebf28a0c7
    DOWNLOAD_NO_PROGRESS ON
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${GTEST_ROOT}
    LOG_INSTALL OFF
    PREFIX ${THIRD_PARTY_DIR}/gtest
    INSTALL_COMMAND "")

ExternalProject_Get_Property(gtest source_dir binary_dir)

if (${COMPILER_SUPPORT_NO_ZERO_AS_NULL})
    add_compile_options(-Wno-zero-as-null-pointer-constant)
endif ()

add_library(
    libgtest
    IMPORTED
    STATIC
    GLOBAL)
add_dependencies(libgtest gtest)

set_target_properties(libgtest PROPERTIES IMPORTED_LOCATION
                                          ${binary_dir}/googlemock/gtest/libgtest.a)

add_library(
    libgmock
    IMPORTED
    STATIC
    GLOBAL)

add_dependencies(libgmock gtest)

set_target_properties(libgmock PROPERTIES IMPORTED_LOCATION ${binary_dir}/googlemock/libgmock.a)

add_library(
    libgtest_main
    IMPORTED
    STATIC
    GLOBAL)
add_dependencies(libgtest_main gtest)
set_target_properties(libgtest_main PROPERTIES IMPORTED_LOCATION
                                               ${binary_dir}/googlemock/gtest/libgtest_main.a)

if (Threads_FOUND)
    target_link_libraries(libgmock INTERFACE ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries(libgtest INTERFACE ${CMAKE_THREAD_LIBS_INIT})
else ()
    target_compile_definitions(libgmock PUBLIC GTEST_HAS_PTHREAD=0)
    target_compile_definitions(libgtest PUBLIC GTEST_HAS_PTHREAD=0)
endif ()

set(GTEST_INCLUDE_DIR ${source_dir}/googletest/include ${source_dir}/googlemock/include
    CACHE INTERNAL "GTEST_INC")
