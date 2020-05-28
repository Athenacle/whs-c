include(ExternalProject)
include(FetchContent)
include(FindPkgConfig)
include(FindPackageHandleStandardArgs)

set(FETCHCONTENT_BASE_DIR ${THIRD_PARTY_DIR})

add_custom_target(3party)

set(CMAKE_FMT_ROOT ${THIRD_PARTY_DIR}/fmt)

pkg_check_modules(fmt QUIET fmt>=1.5)
if (NOT fmt_FOUND)
    message(STATUS "pkg-config fmt not found")

    # third-party project: fmtlib
    # from:    https://github.com/fmtlib/fmt/
    # desc:    A modern formatting library http://fmtlib.net
    # ver:     6.2.1
    # license: BSD-2c

    FetchContent_Declare(
        fmt
        URL https://github.com/fmtlib/fmt/releases/download/6.2.1/fmt-6.2.1.zip
        URL_HASH SHA256=94fea742ddcccab6607b517f6e608b1e5d63d712ddbc5982e44bafec5279881a
        SUBBUILD_DIR ${CMAKE_FMT_ROOT}/subbuild
        BINARY_DIR ${CMAKE_FMT_ROOT}/build SOURCE_DIR ${CMAKE_FMT_ROOT}/src
        BUILD_COMMAND "")

    FetchContent_MakeAvailable(fmt)
    FetchContent_GetProperties(fmt)

    if (NOT fmt_POPULATED)
        FetchContent_Populate(fmt)
    endif ()

    include_directories(${CMAKE_FMT_ROOT}/src/include)
    list(
        APPEND
        THIRD_PARTY_SOURCE
        ${CMAKE_FMT_ROOT}/src/src/format.cc
        ${CMAKE_FMT_ROOT}/src/src/os.cc)
else ()
    message(STATUS "Use system fmt")
    include_directories(${fmt_INCLUDE_DIRS})
    add_definitions(${fmt_CFLAGS})
    add_link_options(-L${fmt_LIBDIR} -l${fmt_LIBRARIES})
    list(
        APPEND
        THIRD_PARTY_LIBRARIES
        -L${fmt_LIBDIR}
        -l${fmt_LIBRARIES})
endif ()

if (ENABLE_WHSFSD)
    pkg_check_modules(spdlog QUIET spdlog>=1.5)
    if (spdlog_FOUND)
        message(STATUS "Use system spdlog")
        list(APPEND THIRD_PARTY_LIBRARIES ${spdlog_LDFLAGS})
    else ()
        message(STATUS "pkg-config spdlog not found")
        #
        # third-party project: spdlog
        # from:    https://github.com/gabime/spdlog
        # desc:    Very fast, header only, C++ logging library
        # ver:     1.6.0 license: MIT
        ExternalProject_Add(
            spdlog
            URL https://github.com/gabime/spdlog/archive/v1.6.0.zip
            URL_HASH SHA256=effae21434c926cf924549b6835e0b7a96b96b073449a5b2259d59e0711c9d19
            DOWNLOAD_NO_PROGRESS ON
            PREFIX ${THIRD_PARTY_DIR}/spdlog
            INSTALL_COMMAND ""
            BUILD_COMMAND "")

        ExternalProject_Get_Property(spdlog source_dir)
        set(SPDLOG_INCLUDE_DIRS ${source_dir}/include CACHE INTERNAL "SPDLOG_INC")
        mark_as_advanced(SPDLOG_INCLUDE_DIRS)
        add_dependencies(3party spdlog)
    endif ()
endif ()

# Find the header and library
find_path(HTTP_PARSER_INCLUDE_DIR NAMES http_parser.h)
find_library(HTTP_PARSER_LIBRARY NAMES http_parser libhttp_parser)

# Found the header, read version
if (HTTP_PARSER_INCLUDE_DIR AND EXISTS "${HTTP_PARSER_INCLUDE_DIR}/http_parser.h")
    file(READ "${HTTP_PARSER_INCLUDE_DIR}/http_parser.h" HTTP_PARSER_H)
    if (HTTP_PARSER_H)
        string(
            REGEX
            REPLACE ".*#define[\t ]+HTTP_PARSER_VERSION_MAJOR[\t ]+([0-9]+).*"
                    "\\1"
                    HTTP_PARSER_VERSION_MAJOR
                    "${HTTP_PARSER_H}")
        string(
            REGEX
            REPLACE ".*#define[\t ]+HTTP_PARSER_VERSION_MINOR[\t ]+([0-9]+).*"
                    "\\1"
                    HTTP_PARSER_VERSION_MINOR
                    "${HTTP_PARSER_H}")
        set(HTTP_PARSER_VERSION_STRING "${HTTP_PARSER_VERSION_MAJOR}.${HTTP_PARSER_VERSION_MINOR}")
    endif ()
    unset(HTTP_PARSER_H)
endif ()

# Handle the QUIETLY and REQUIRED arguments and set HTTP_PARSER_FOUND to TRUE if all listed
# variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HTTP_Parser REQUIRED_VARS HTTP_PARSER_INCLUDE_DIR
                                                            HTTP_PARSER_LIBRARY)

# Hide advanced variables
mark_as_advanced(HTTP_PARSER_INCLUDE_DIR HTTP_PARSER_LIBRARY)

# Set standard variables
if (HTTP_PARSER_FOUND)
    set(HTTP_PARSER_LIBRARIES ${HTTP_PARSER_LIBRARY})
    set(HTTP_PARSER_INCLUDE_DIRS ${HTTP_PARSER_INCLUDE_DIR})
endif ()

if (NOT HTTP_PARSER_FOUND)
    set(CMAKE_HP_ROOT ${THIRD_PARTY_DIR}/http-parser)
    FetchContent_Declare(
        http-parser
        URL https://github.com/nodejs/http-parser/archive/v2.9.4.tar.gz
        URL_HASH SHA256=467b9e30fd0979ee301065e70f637d525c28193449e1b13fbcb1b1fab3ad224f
        SUBBUILD_DIR ${CMAKE_HP_ROOT}/subbuild
        BINARY_DIR ${CMAKE_HP_ROOT}/build SOURCE_DIR ${CMAKE_HP_ROOT}/src
        BUILD_COMMAND "")

    FetchContent_MakeAvailable(http-parser)
    FetchContent_GetProperties(http-parser)

    if (NOT http-parser_POPULATED)
        FetchContent_Populate(http-parser)
    endif ()
    include_directories(${CMAKE_HP_ROOT}/src/)
    list(APPEND THIRD_PARTY_SOURCE ${CMAKE_HP_ROOT}/src/http_parser.c)
endif ()
