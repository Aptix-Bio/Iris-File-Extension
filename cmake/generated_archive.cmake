# R-7 (development/WBS.md): the generated C++ layer as a release archive, for release.yml's
# `generated` job, so that its one `run:` step is a `cmake -P` and its logic is here
# (development/ci_runners.md, "Workflows are simple; CMake does the work").
#
#   PKG=IrisFileExtension-<version>-generated-source cmake -P cmake/generated_archive.cmake
#
# Run from the directory the archive is written in. Runs generate_IFE.py, the command CMake runs
# at configure, into stage/<PKG>/generated_source, adds LICENSE, the code's licence, and writes
# stage/<PKG>.tar.gz. Exits non-zero naming the step that failed, or when the archive is missing
# or empty (development/ci_runners.md, Robustness).
cmake_minimum_required(VERSION 3.20)

set(package "$ENV{PKG}")
if(NOT package MATCHES "^IrisFileExtension-[0-9A-Za-z.+-]+-generated-source$")
    message(FATAL_ERROR "generated_archive: PKG is '${package}'; "
                        "expected IrisFileExtension-<version>-generated-source")
endif()
get_filename_component(repository "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
# In script mode CMAKE_CURRENT_BINARY_DIR is the working directory.
set(stage "${CMAKE_CURRENT_BINARY_DIR}/stage")
file(REMOVE_RECURSE "${stage}/${package}" "${stage}/${package}.tar.gz")

find_program(PYTHON NAMES python3 python REQUIRED)
execute_process(COMMAND "${PYTHON}" "${repository}/generate_IFE.py"
                        --out-dir "${stage}/${package}/generated_source"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "generated_archive: generate_IFE.py exited with '${result}'")
endif()
file(COPY "${repository}/LICENSE" DESTINATION "${stage}/${package}")

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar czf "${package}.tar.gz" "${package}"
                WORKING_DIRECTORY "${stage}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "generated_archive: tar exited with '${result}'")
endif()
if(NOT EXISTS "${stage}/${package}.tar.gz")
    message(FATAL_ERROR "generated_archive: no ${package}.tar.gz was written")
endif()
file(SIZE "${stage}/${package}.tar.gz" size)
if(size EQUAL 0)
    message(FATAL_ERROR "generated_archive: ${package}.tar.gz is empty")
endif()
message(STATUS "generated_archive: stage/${package}.tar.gz, ${size} bytes")
