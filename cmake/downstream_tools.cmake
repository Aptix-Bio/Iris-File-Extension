# R-8 (development/PIPELINE.md, rule 12): the system tools the `downstream` job's Iris-Slide build
# needs, as one `cmake -P` step whose logic is here, so the workflow holds no shell logic and the
# same command runs locally. nasm (for the aom port), Ninja and pkg-config are what Iris-Slide's own
# .github/workflows/_build.yml installs on Linux, because vcpkg cannot fetch them on Blacksmith's
# Linux runners; vcpkg builds every library.
#
#   cmake -P cmake/downstream_tools.cmake      (Debian or Ubuntu, with sudo)
#
# Exits non-zero naming the command that failed.
cmake_minimum_required(VERSION 3.27)

find_program(SUDO sudo REQUIRED)
find_program(APT_GET apt-get REQUIRED)
foreach(step IN ITEMS "update;--quiet" "install;--yes;--no-install-recommends;nasm;ninja-build;pkg-config")
    execute_process(COMMAND "${SUDO}" "${APT_GET}" ${step} RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "downstream_tools: apt-get ${step} exited with '${result}'")
    endif()
endforeach()
