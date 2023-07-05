if(TARGET Cpd::cpd)
    return()
endif()

message(STATUS "[frite] Adding target Cpd::cpd")

include(FetchContent)
FetchContent_Declare(
    cpd
    GIT_REPOSITORY https://github.com/benardp/cpd.git
    GIT_TAG 886b13d7ebe0221728273efcc01bb05fc261f4e4
    GIT_SHALLOW FALSE
)

option(WITH_TESTS           "Build test suite" OFF)
option(WITH_DOCS            "Add documentation target" OFF)
option(WITH_STRICT_WARNINGS "Build with stricter warnings" ON)
option(WITH_JSONCPP         "Build with jsoncpp" OFF)
option(WITH_FGT             "Build with fgt" ON)

FetchContent_MakeAvailable(cpd)

add_library(Cpd::cpd ALIAS cpd)