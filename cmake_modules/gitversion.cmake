set(_build_version "unknown")
set(_build_branch "unknown")
set(_build_changes "")

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE _build_version
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE _build_branch
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} diff --no-ext-diff --quiet
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        RESULT_VARIABLE _ret
        ERROR_QUIET)
    if(_ret EQUAL 1)
        set(_build_changes "*")
    endif()
endif()

string(TIMESTAMP _time_stamp)

configure_file(${PROJECT_SOURCE_DIR}/src/gitversion.h.in ${PROJECT_BINARY_DIR}/generated/gitversion.h @ONLY)
