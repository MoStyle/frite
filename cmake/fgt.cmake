if(TARGET Fgt::fgt)
    return()
endif()

message(STATUS "[frite] Adding target Fgt::fgt")

include(FetchContent)
FetchContent_Declare(
    fgt
    GIT_REPOSITORY https://github.com/benardp/fgt.git
    GIT_TAG 583b1c91d47fa0407d9485832fb752e1e52857aa
    GIT_SHALLOW FALSE
)

option(WITH_TESTS          "Build test suite" OFF)
option(WITH_OPENMP         "Use OpenMP parallelization" OFF)
option(BUILD_SHARED_LIBS   "Build shared libs" OFF)

FetchContent_MakeAvailable(fgt)

add_library(Fgt::fgt ALIAS fgt)