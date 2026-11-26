# R-8 (development/WBS.md; PIPELINE.md, rule 12): cmake/downstream_iris_slide.cmake gives the
# verdict its header states, including a coordinated change: green on Iris-Slide's paired branch
# and red on Iris-Slide main fails until both pull requests merge.
#
#   cmake -P cmake/tests/downstream_iris_slide_test.cmake      (from any directory; needs git)
#
# Hermetic: Iris-Slide is a local git repository whose scripts/local_ports.cmake records the IFE
# checkout it is given and whose configure requires one header in it (main the old name, the
# paired branch the new one), and the IFE trees differ only in which headers they carry, so each
# case runs in seconds with no compiler. Each case states the verdict it expects and a phrase of
# its message.
cmake_minimum_required(VERSION 3.27)

set(script "${CMAKE_CURRENT_LIST_DIR}/../downstream_iris_slide.cmake")
set(root "${CMAKE_CURRENT_BINARY_DIR}/downstream-iris-slide-test")
file(REMOVE_RECURSE "${root}")
set(paired feature/ApiChanged)

# IFE trees: before the change (old), after it (new), one both build against, one neither does.
foreach(tree IN ITEMS old new both neither)
    file(WRITE "${root}/ife-${tree}/CMakeLists.txt" "")
endforeach()
foreach(tree IN ITEMS old both)
    file(WRITE "${root}/ife-${tree}/include/OldName.hpp" "#pragma once\n")
endforeach()
foreach(tree IN ITEMS new both)
    file(WRITE "${root}/ife-${tree}/include/NewName.hpp" "#pragma once\n")
endforeach()

function(git_in directory)
    execute_process(COMMAND git -c user.name=test -c user.email=test@example.invalid -c commit.gpgsign=false ${ARGN}
                    WORKING_DIRECTORY "${directory}" RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE error)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} in ${directory}: ${error}")
    endif()
endfunction()

# Iris-Slide, cloned as <OWNER_URL>/Iris-Slide.git.
set(repository "${root}/owner/Iris-Slide.git")
file(WRITE "${repository}/scripts/local_ports.cmake"
     "file(WRITE \"\${OUT}/ife.txt\" \"\${IRIS_FILE_EXTENSION}\")\n")
function(consumer_requires header)
    file(WRITE "${repository}/CMakeLists.txt"
         "cmake_minimum_required(VERSION 3.27)\n"
         "project(consumer LANGUAGES NONE)\n"
         "file(READ \"\${VCPKG_OVERLAY_PORTS}/ife.txt\" ife)\n"
         "if(NOT EXISTS \"\${ife}/include/${header}\")\n"
         "    message(FATAL_ERROR \"Cannot find source file: ${header}\")\n"
         "endif()\n")
endfunction()
git_in("${repository}" init --quiet --initial-branch=main)
consumer_requires(OldName.hpp)
git_in("${repository}" add CMakeLists.txt scripts/local_ports.cmake)
git_in("${repository}" commit --quiet -m "main uses the old name")
git_in("${repository}" switch --quiet -c ${paired})
consumer_requires(NewName.hpp)
git_in("${repository}" commit --quiet -am "the paired branch uses the new name")
git_in("${repository}" switch --quiet main)

set(failures 0)
# expect: PASS or FAIL; phrase: text the script's output must contain.
function(verdict_case name head_ref change base expect phrase)
    set(ENV{HEAD_REF} "${head_ref}")
    set(ENV{OWNER} "")
    file(MAKE_DIRECTORY "${root}/work-${name}")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DCHANGE=${root}/ife-${change}" "-DBASE=${root}/ife-${base}"
                            "-DOWNER_URL=file://${root}/owner" -DAPI_URL=none -P "${script}"
                    WORKING_DIRECTORY "${root}/work-${name}" RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE out)
    set(outcome PASS)
    if(NOT result EQUAL 0)
        set(outcome FAIL)
    endif()
    # CMake wraps a message's lines; compare with every run of whitespace as one space.
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}")
    string(FIND "${flat}" "${phrase}" at)
    if(NOT outcome STREQUAL expect OR at EQUAL -1)
        message(SEND_ERROR "${name}: expected ${expect} with '${phrase}', got ${outcome}:\n${out}")
        math(EXPR failures "${failures} + 1")
        set(failures ${failures} PARENT_SCOPE)
    else()
        message(STATUS "${name}: ${outcome} ('${phrase}')")
    endif()
endfunction()

verdict_case(coordinated ${paired} new old FAIL "A coordinated change: merging this breaks Iris-Slide main")
verdict_case(paired-and-main ${paired} both old PASS "Iris-Slide main builds against the change")
verdict_case(breaks "" new old FAIL "This change breaks Iris-Slide at main")
verdict_case(unpaired-green "" both old PASS "Iris-Slide at main builds against the change")
verdict_case(already-broken "" neither neither PASS "already broken, not blocked by this change")
verdict_case(no-such-pair feature/NoPair new old FAIL "This change breaks Iris-Slide at main")
verdict_case(semicolon "feature/a\\;b" new old FAIL "HEAD_REF holds ';'")

if(failures GREATER 0)
    message(FATAL_ERROR "downstream_iris_slide_test: ${failures} case(s) failed")
endif()
message(STATUS "downstream_iris_slide_test: every case gives its verdict")
