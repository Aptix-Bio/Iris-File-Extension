# R-8 (development/WBS.md; PIPELINE.md, rule 12): ci.yml's `downstream` job, Iris-Slide built against
# this change, as one `cmake -P` step (development/ci_runners.md, "Workflows are simple; CMake does
# the work"). The verdict is the one Iris-Headers' downstream verdict gives its consumers:
#   change green                                 -> pass
#   change red, base green                       -> this change breaks Iris-Slide: fail
#   change red, base red                         -> Iris-Slide is already broken: warn, pass
# and, when Iris-Slide has a paired branch and the change is green on it, Iris-Slide main as well:
#   main green against the change                -> pass
#   main red against the change, green on base   -> a coordinated change: fail, naming Iris-Slide's
#                                                   pull request, until both merge
#   main red against the base too                -> main already broken: warn, pass
#
#   HEAD_REF=<pull request head branch, empty on a push> [OWNER=<repository owner>] [GH_TOKEN=<token>]
#   cmake -DCHANGE=<this checkout> -DBASE=<the base's checkout> [-DOWNER_URL=<clone root>]
#         [-DAPI_URL=<REST root, or none>] -P cmake/downstream_iris_slide.cmake
#
# Run from the directory Iris-Slide is cloned and built in. Iris-Slide takes IFE as a vcpkg port
# pinned by commit, so each build goes through the consumer's own scripts/local_ports.cmake, which
# writes an overlay port whose source is the IFE checkout named; until 2026-11-25 this job passed
# a FETCHCONTENT_SOURCE_DIR override, which Iris-Slide reported unused, and built the pinned commit.
# A change that breaks Iris-Slide's API use is made on a branch of the same name in both
# repositories (the paired branch). OWNER's Iris-Slide is used when it exists, so a fork tests its
# own consumer; else OWNER_URL's (Aptix-Bio's). The branch name is text the pull request's author
# chose, so it arrives through the environment and reaches git and the API as one argument.
# cmake/tests/downstream_iris_slide_test.cmake holds the verdict to the table above.
cmake_minimum_required(VERSION 3.27)

foreach(checkout CHANGE BASE)
    if(NOT ${checkout} OR NOT EXISTS "${${checkout}}/CMakeLists.txt")
        message(FATAL_ERROR "downstream_iris_slide: -D${checkout}=<IFE checkout> is required; '${${checkout}}' holds no CMakeLists.txt")
    endif()
    cmake_path(ABSOLUTE_PATH ${checkout} NORMALIZE)
endforeach()
if(NOT OWNER_URL)
    set(OWNER_URL "https://github.com/Aptix-Bio")
endif()
if(NOT API_URL)
    set(API_URL "https://api.github.com")
endif()
find_program(GIT git REQUIRED)
set(head_ref "$ENV{HEAD_REF}")
# A git branch may hold `;`, which a CMake list would split: refuse it by name.
if(head_ref MATCHES ";")
    message(FATAL_ERROR "downstream_iris_slide: HEAD_REF holds ';', which this script does not pass to git: '${head_ref}'")
endif()
# The token only looks up Iris-Slide's pull request: taken out of the environment, so no child
# process (Iris-Slide's scripts, its configure, vcpkg's port builds) inherits it.
set(token "$ENV{GH_TOKEN}")
unset(ENV{GH_TOKEN})
set(work "${CMAKE_CURRENT_BINARY_DIR}")

# OWNER's Iris-Slide when it has one, else OWNER_URL's.
if(NOT "$ENV{OWNER}" STREQUAL "")
    execute_process(COMMAND "${GIT}" ls-remote --exit-code "https://github.com/$ENV{OWNER}/Iris-Slide.git" HEAD
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(OWNER_URL "https://github.com/$ENV{OWNER}")
    endif()
endif()
set(origin "${OWNER_URL}/Iris-Slide.git")
cmake_path(GET OWNER_URL FILENAME owner)

# The paired branch when Iris-Slide has one; `paired` is the branch or empty.
set(paired "")
if(NOT head_ref STREQUAL "")
    execute_process(COMMAND "${GIT}" ls-remote --exit-code --heads "${origin}" "refs/heads/${head_ref}"
                    RESULT_VARIABLE result OUTPUT_QUIET ERROR_QUIET)
    if(result EQUAL 0)
        set(paired "${head_ref}")
    endif()
endif()

function(clone ref directory)
    if(EXISTS "${directory}")
        return()
    endif()
    message(STATUS "downstream_iris_slide: ${origin} at ${ref}")
    execute_process(COMMAND "${GIT}" clone --quiet --depth 1 "--branch=${ref}" "${origin}" "${directory}"
                    RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "downstream_iris_slide: cloning ${origin} at ${ref} failed (exit ${result})")
    endif()
endfunction()

# Iris-Slide in `consumer` against the IFE checkout `ife`: 0 green, 1 red at build, 2 red at
# configure (the overlay or the configure).
function(build consumer ife out result_variable)
    set(overlay "${out}-overlay")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DOUT=${overlay}" "-DIRIS_FILE_EXTENSION=${ife}"
                            -P "${consumer}/scripts/local_ports.cmake" RESULT_VARIABLE result)
    if(result EQUAL 0)
        execute_process(COMMAND "${CMAKE_COMMAND}" -S "${consumer}" -B "${out}" -DCMAKE_BUILD_TYPE=Release
                                "-DVCPKG_OVERLAY_PORTS=${overlay}" RESULT_VARIABLE result)
    endif()
    if(NOT result EQUAL 0)
        set(${result_variable} 2 PARENT_SCOPE)
        return()
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" --build "${out}" --config Release RESULT_VARIABLE result)
    if(result EQUAL 0)
        set(${result_variable} 0 PARENT_SCOPE)
    else()
        set(${result_variable} 1 PARENT_SCOPE)
    endif()
endfunction()

# "Iris-Slide#<number>" for the open pull request from `branch`, or where it should be when the
# lookup finds none. The branch reaches the URL only when made of characters a query carries
# unescaped.
function(pull_request branch text_variable)
    set(${text_variable} "Iris-Slide's pull request from ${branch}" PARENT_SCOPE)
    if(API_URL STREQUAL "none" OR NOT branch MATCHES "^[A-Za-z0-9._/-]+$")
        return()
    endif()
    set(headers HTTPHEADER "Accept: application/vnd.github+json")
    if(NOT token STREQUAL "")
        list(APPEND headers HTTPHEADER "Authorization: Bearer ${token}")
    endif()
    set(json "${work}/iris-slide-pulls.json")
    file(DOWNLOAD "${API_URL}/repos/${owner}/Iris-Slide/pulls?state=open&head=${owner}:${branch}" "${json}"
         ${headers} STATUS status)
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        message(WARNING "downstream_iris_slide: looking up Iris-Slide's pull request from ${branch} failed: ${status}")
        return()
    endif()
    file(READ "${json}" pulls)
    string(JSON count ERROR_VARIABLE error LENGTH "${pulls}")
    if(error OR count EQUAL 0)
        return()
    endif()
    string(JSON number GET "${pulls}" 0 number)
    set(${text_variable} "Iris-Slide#${number}" PARENT_SCOPE)
endfunction()

set(ref main)
if(paired)
    set(ref "${paired}")
endif()
clone("${ref}" "${work}/Iris-Slide-${ref}")
build("${work}/Iris-Slide-${ref}" "${CHANGE}" "${work}/build-change" change)
if(NOT change EQUAL 0)
    message(STATUS "downstream_iris_slide: Iris-Slide at ${ref} is red against the change (${change}: 1 at build, 2 at configure)")
    build("${work}/Iris-Slide-${ref}" "${BASE}" "${work}/build-base" base)
    if(base EQUAL 0)
        message(FATAL_ERROR "This change breaks Iris-Slide at ${ref}: it builds against the base and fails against the change.")
    endif()
    message(WARNING "Iris-Slide at ${ref} fails against the base too: already broken, not blocked by this change.")
    return()
endif()
message(STATUS "downstream_iris_slide: Iris-Slide at ${ref} builds against the change")
if(NOT paired)
    return()
endif()

# Green on the paired branch: Iris-Slide main against the same change, so a change green only on
# its paired branch fails until both pull requests merge.
clone(main "${work}/Iris-Slide-main")
build("${work}/Iris-Slide-main" "${CHANGE}" "${work}/build-main-change" main_change)
if(main_change EQUAL 0)
    message(STATUS "downstream_iris_slide: Iris-Slide main builds against the change")
    return()
endif()
build("${work}/Iris-Slide-main" "${BASE}" "${work}/build-main-base" main_base)
if(NOT main_base EQUAL 0)
    message(WARNING "Iris-Slide main fails against the base too: already broken, not blocked by this change.")
    return()
endif()
pull_request("${paired}" blocker)
message(FATAL_ERROR "A coordinated change: merging this breaks Iris-Slide main until ${blocker} merges; merge them "
                    "together, this one first. Green on the paired branch ${paired}; red on main, which builds against the base.")
