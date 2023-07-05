if(TARGET nanoflann::nanoflann)
    return()
endif()

message(STATUS "[frite] Adding target nanoflann::nanoflann")

include(FetchContent)
FetchContent_Declare(
    nanoflann
    GIT_REPOSITORY https://github.com/jlblancoc/nanoflann.git
    GIT_TAG master
    GIT_SHALLOW FALSE
)

option(NANOFLANN_BUILD_EXAMPLES        "Build example" OFF)
option(NANOFLANN_BUILD_TESTS           "Build tests" OFF)

FetchContent_MakeAvailable(nanoflann)

add_library(nanoflann::nanoflann ALIAS nanoflann)