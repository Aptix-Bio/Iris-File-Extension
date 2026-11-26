# R-8 (development/WBS.md; PIPELINE.md, rule 12): ci.yml's `downstream` job, Iris-Slide built against
# this checkout, as one `cmake -P` step (development/ci_runners.md, "Workflows are simple; CMake does
# the work").
#
#   HEAD_REF=<pull request head branch, empty on a push> OWNER=<repository owner>
#   cmake -DIFE=<this checkout> -P cmake/downstream_iris_slide.cmake
#
# Run from the directory the consumer is cloned and built in. Iris-Slide takes IFE as a vcpkg port
# pinned by commit (its ports/iris-file-extension), so the FETCHCONTENT_SOURCE_DIR override this
# job passed until 2026-11-25 was reported unused and the job built the pinned commit instead of
# this checkout. The build goes through the consumer's own scripts/local_ports.cmake, which writes
# an overlay port whose source is this checkout.
#
# A change that breaks Iris-Slide's API use is made in both repositories on a branch of the same
# name (the paired branch), as Iris-Headers' downstream verdict pairs them: Iris-Slide is cloned at
# the branch named like the pull request's head when it has one, else at main. OWNER's Iris-Slide
# is used when it exists, so a fork tests its own consumer; else Aptix-Bio's. The branch name is
# text the pull request's author chose, so it arrives through the environment and reaches git as
# one argument, never through a shell. Exits non-zero naming the step that failed.
cmake_minimum_required(VERSION 3.27)

if(NOT IFE OR NOT EXISTS "${IFE}/CMakeLists.txt")
    message(FATAL_ERROR "downstream_iris_slide: -DIFE=<this checkout> is required; '${IFE}' holds no CMakeLists.txt")
endif()
cmake_path(ABSOLUTE_PATH IFE NORMALIZE)
find_program(GIT git REQUIRED)
set(head_ref "$ENV{HEAD_REF}")
set(work "${CMAKE_CURRENT_BINARY_DIR}")

# OWNER's Iris-Slide when it has one, else Aptix-Bio's.
set(origin "https://github.com/Aptix-Bio/Iris-Slide.git")
if(NOT "$ENV{OWNER}" STREQUAL "")
    execute_process(COMMAND "${GIT}" ls-remote --exit-code "https://github.com/$ENV{OWNER}/Iris-Slide.git" HEAD
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(origin "https://github.com/$ENV{OWNER}/Iris-Slide.git")
    endif()
endif()

# The paired branch when the consumer has one, else main.
set(ref main)
if(NOT head_ref STREQUAL "")
    execute_process(COMMAND "${GIT}" ls-remote --exit-code --heads "${origin}" "refs/heads/${head_ref}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(ref "${head_ref}")
    endif()
endif()
message(STATUS "downstream_iris_slide: ${origin} at ${ref}")

set(consumer "${work}/Iris-Slide")
file(REMOVE_RECURSE "${consumer}")
execute_process(COMMAND "${GIT}" clone --quiet --depth 1 "--branch=${ref}" "${origin}" "${consumer}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream_iris_slide: cloning ${origin} at ${ref} failed (exit ${result})")
endif()

set(overlay "${work}/iris-slide-overlay")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DOUT=${overlay}" "-DIRIS_FILE_EXTENSION=${IFE}"
                        -P "${consumer}/scripts/local_ports.cmake"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream_iris_slide: Iris-Slide's scripts/local_ports.cmake exited with '${result}'")
endif()

set(build "${work}/iris-slide-build")
execute_process(COMMAND "${CMAKE_COMMAND}" -S "${consumer}" -B "${build}" -DCMAKE_BUILD_TYPE=Release
                        "-DVCPKG_OVERLAY_PORTS=${overlay}"
                RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream_iris_slide: Iris-Slide at ${ref} does not configure against this checkout (exit ${result})")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" --build "${build}" --config Release RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "downstream_iris_slide: Iris-Slide at ${ref} does not build against this checkout (exit ${result})")
endif()
message(STATUS "downstream_iris_slide: Iris-Slide at ${ref} builds against this checkout")
