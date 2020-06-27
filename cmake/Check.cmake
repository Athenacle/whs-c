include(CheckIncludeFile)
include(CheckFunctionExists)
include(CheckIncludeFileCXX)
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
include(CheckCXXSymbolExists)

check_function_exists(getopt_long UNIX_HAVE_GETOPTLONG)
check_function_exists(bzero HAVE_BZERO)
if (UNIX)
    check_function_exists(pthread_mutex_lock UNIX_HAVE_PTHREAD_MUTEX)
    check_include_file_cxx(pthread.h UNIX_HAVE_PTHREAD_H)
    check_cxx_symbol_exists(epoll_create sys/epoll.h UNIX_HAVE_EPOLL)
    check_cxx_symbol_exists(inotify_init sys/inotify.h UNIX_HAVE_INOTIFY)
endif ()

if (UNIX_HAVE_EPOLL)
    add_compile_definitions(UNIX_HAVE_EPOLL)
endif ()

if (UNIX_HAVE_INOTIFY)
    add_compile_definitions(UNIX_HAVE_INOTIFY)
endif ()

check_cxx_source_compiles(
    "
int main() {
  return __builtin_expect(0, 1);
}"
    HAVE_BUILTIN_EXPECT)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    macro (CXX_COMPILER_CHECK_ADD)
        set(list_var "${ARGN}")
        foreach (flag IN LISTS list_var)
            string(TOUPPER ${flag} FLAG_NAME1)
            string(
                REPLACE "-"
                        "_"
                        FLAG_NAME2
                        ${FLAG_NAME1})
            string(CONCAT FLAG_NAME "COMPILER_SUPPORT_" ${FLAG_NAME2})
            check_cxx_compiler_flag(-${flag} ${FLAG_NAME})
            if (${${FLAG_NAME}})
                add_compile_options(-${flag})
            endif ()
        endforeach ()
    endmacro ()

    cxx_compiler_check_add(
        Wall
        Wno-useless-cast
        Wextra
        Wpedantic
        Wduplicated-branches
        Wduplicated-cond
        Wlogical-op
        Wrestrict
        Wnull-dereference
        Wno-c99-extensions
        fno-permissive)

    # check_cxx_compiler_flag(-fno-permissive COMPILER_SUPPORT_FNOPERMISSIVE)
    #
    # if (${COMPILER_SUPPORT_FNOPERMISSIVE}) set(CMAKE_CXX_FLAGS "-fno-permissive ${CMAKE_CXX_FLAGS}")
    # endif()

endif ()

macro (get_git_hash _git_hash)
    find_package(Git QUIET)
    if (GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%H OUTPUT_VARIABLE ${_git_hash}
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif ()
endmacro ()
