# CI-3: one command of the review bot, so that each `run:` step of .github/workflows/review.yml is
# one `cmake -P` and its logic is here, where it is checked and tested, and no shell line in the
# workflow can reach the model key or the write token.
#
#   cmake -DSTEP=<transcript-key|review|publish|fork-notice> -P base/cmake/review_bot.cmake
#
# Run from the job's workspace, where the default branch is checked out at base/ and the bot at
# governance/. The inputs come from the step's `env:`: PYTHON and SCRIPT (the bot's action
# outputs) and the values each command's table below names. Pull request data arrives only that
# way and reaches the bot as separate arguments with no shell between, so no value is parsed as a
# command. The bot's own output, and its step outputs in $GITHUB_OUTPUT, pass through unchanged.
# Exits non-zero naming the command when an input is missing, the bot fails, or `review` writes
# no verdict.
cmake_minimum_required(VERSION 3.20)

# The arguments of each command, as <flag> <environment variable> pairs; `=` marks a literal value.
set(transcript-key_arguments --pr PR_NUMBER)
set(review_arguments
    --pr PR_NUMBER --base-dir =base --base-sha BASE_SHA --head-sha HEAD_SHA
    --rubric-commit RULES_SHA --bot-ref BOT_REF --run-url RUN_URL
    --out =verdict.json --transcript =transcript.json)
set(publish_arguments
    --pr PR_NUMBER --head-sha HEAD_SHA --rubric =base/.github/review/rubric.toml --base-dir =base
    --verdict =verdict.json --run-url RUN_URL)

# A fork's pull request gets no model review; the run says why instead. A workflow command must
# begin its line, which message() would prefix, so it is printed through `cmake -E echo`.
if(STEP STREQUAL "fork-notice")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo
        "::notice title=No model review for a fork::The review bot holds the model key and a write token, which pull_request_target would give to a fork's pull request; a maintainer reviews this change, or pushes it to a branch here to have the bot review it (.github/workflows/review.yml).")
    return()
endif()
if(NOT DEFINED ${STEP}_arguments)
    message(FATAL_ERROR "review_bot: unknown STEP '${STEP}'; "
                        "expected transcript-key, review, publish or fork-notice")
endif()

set(missing "")
foreach(name PYTHON SCRIPT)
    if("$ENV{${name}}" STREQUAL "")
        list(APPEND missing "${name}")
    endif()
endforeach()
set(arguments "")
set(pending "${${STEP}_arguments}")
while(pending)
    list(POP_FRONT pending flag source)
    if(source MATCHES "^=(.*)$")
        list(APPEND arguments "${flag}" "${CMAKE_MATCH_1}")
        continue()
    endif()
    if("$ENV{${source}}" STREQUAL "")
        list(APPEND missing "${source}")
    endif()
    # A CMake list splits on ';', so a value holding one would reach the bot as two arguments.
    # No input has one (numbers, commit SHAs, the run's URL); the value is not printed.
    if("$ENV{${source}}" MATCHES ";")
        message(FATAL_ERROR "review_bot ${STEP}: ${source} holds a ';'")
    endif()
    list(APPEND arguments "${flag}" "$ENV{${source}}")
endwhile()
if(missing)
    list(JOIN missing ", " names)
    message(FATAL_ERROR "review_bot ${STEP}: the step's env: does not set ${names}")
endif()

execute_process(COMMAND "$ENV{PYTHON}" "$ENV{SCRIPT}" ${STEP} ${arguments} RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "review_bot ${STEP}: the bot exited with '${result}'")
endif()

# An exit code of 0 with nothing written is a failure, since an empty verdict would publish no review.
if(NOT STEP STREQUAL "review")
    return()
endif()
# In script mode CMAKE_CURRENT_BINARY_DIR is the working directory, where the bot wrote it.
set(verdict "${CMAKE_CURRENT_BINARY_DIR}/verdict.json")
if(NOT EXISTS "${verdict}")
    message(FATAL_ERROR "review_bot review: the bot exited 0 and wrote no verdict.json")
endif()
file(SIZE "${verdict}" size)
if(size EQUAL 0)
    message(FATAL_ERROR "review_bot review: the bot exited 0 and wrote an empty verdict.json")
endif()
