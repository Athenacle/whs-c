cmake_minimum_required(VERSION 3.14)

project(whs C CXX)
set(CMAKE_CXX_STANDARD 20)
set(WHSFSD_EXE whsfsd)

set(MAJOR_VERSION 0)
set(MINOR_VERSION 0)
set(PATCH_VERSION 4)
set(THIRD_PARTY_LIBRARIES)
set(THIRD_PARTY_SOURCE)

set(WHS_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

set(OUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/out/)
file(MAKE_DIRECTORY ${OUT_PATH})

set(EXECUTABLE_OUTPUT_PATH ${OUT_PATH})
set(LIBRARY_OUTPUT_PATH ${OUT_PATH})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

option(ENABLE_JSON "Enable Json Support" OFF)
option(ENABLE_LIBUV "Enable Libuv Support" ON)
option(ENABLE_WHSFSD "Enable main" ON)
option(ENABLE_EXCEPTIONS "Enable C++ Exceptions" ON)
option(ENABLE_TEST "Enable Tests" ON)
option(ENABLE_DOC "Enable Documentation (need Doxygen)" OFF)
option(ENABLE_SHARED "Enable Shared Library" OFF)

set(THIRD_PARTY_DIR ${CMAKE_CURRENT_BINARY_DIR}/third_party)
file(MAKE_DIRECTORY ${THIRD_PARTY_DIR})

find_package(OpenSSL REQUIRED)
include(FindPkgConfig)
include(FindThreads)
include(ThirdParty)
include(Check)

set(TEST_LIBRARY)

pkg_check_modules(libpcre2-8 libpcre2-8>=10.0)
if (libpcre2-8_FOUND)
    set(HAVE_LIBPCRE2 ON)
    list(APPEND TEST_LIBRARY ${libpcre2-8_LDFLAGS})
else ()
    set(HAVE_LIBPCRE2 OFF)
endif ()

if (CMAKE_USE_PTHREADS_INIT)
    set(UNIX_HAVE_PTHREAD ON)
endif ()

if (${ENABLE_LIBUV})
    include(FindLibuv)
    list(APPEND TEST_LIBRARY ${LIBUV_LIBRARIES})
endif ()

if (${ENABLE_JSON})
    include(FindRapidJSON)
endif ()

aux_source_directory(${CMAKE_SOURCE_DIR}/src/ whs_src)
list(APPEND whs_src ${THIRD_PARTY_SOURCE})

if (ENABLE_SHARED)
    add_library(whs SHARED ${whs_src})
else ()
    add_library(whs STATIC ${whs_src})
endif ()

target_link_libraries(whs ${HTTP_PARSER_LIBRARIES} ${THIRD_PARTY_LIBRARIES})

file(COPY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/whs/)
configure_file(${CMAKE_SOURCE_DIR}/whs_config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/whs/whs_config.h)
configure_file(${CMAKE_SOURCE_DIR}/src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${PROJECT_NAME}.pc.in ${OUT_PATH}/${PROJECT_NAME}.pc @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})

if (ENABLE_DOC)
    find_package(Doxygen OPTIONAL_COMPONENTS mscgen dia dot)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/doc/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)
        set(DOXYGEN_OUT_PATH ${OUT_PATH}/doc)
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(
            doc ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/src
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)
    else ()
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif ()
endif ()

if (${ENABLE_WHSFSD})
    add_executable(${WHSFSD_EXE} ${CMAKE_SOURCE_DIR}/examples/main.cpp)
    target_include_directories(${WHSFSD_EXE} PUBLIC ${SPDLOG_INCLUDE_DIRS})
    target_compile_options(${WHSFSD_EXE} PUBLIC ${spdlog_CFLAGS})
    add_dependencies(${WHSFSD_EXE} whs)
    target_link_libraries(
        ${WHSFSD_EXE}
        PRIVATE whs
                ${THIRD_PARTY_LIBRARIES}
                ${TEST_LIBRARY}
                ${HTTP_PARSER_LIBRARIES}
                OpenSSL::Crypto
                pthread)
endif ()

add_executable(bench ${CMAKE_SOURCE_DIR}/examples/bench.cpp)
target_link_libraries(
    bench
    PRIVATE whs
            ${THIRD_PARTY_LIBRARIES}
            ${TEST_LIBRARY}
            ${HTTP_PARSER_LIBRARIES}
            OpenSSL::Crypto)

if (${ENABLE_TEST})
    enable_testing()
    add_subdirectory(${CMAKE_SOURCE_DIR}/src/test)
endif ()
