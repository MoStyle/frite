if(TARGET Clipper::clipper)
    return()
endif()

message(STATUS "[frite] Adding target Clipper::clipper")

include(FetchContent)
FetchContent_Declare(
    clipper
    GIT_REPOSITORY https://github.com/Toastation/Clipper2.git
    GIT_TAG 230ca0bb5cd0c2ead723ff2437adb1b2a803bbc5
    GIT_SHALLOW FALSE
    SOURCE_SUBDIR CPP
)

option(CLIPPER2_EXAMPLES "Build examples" OFF)
option(CLIPPER2_TESTS "Build tests" OFF)
option(CLIPPER2_USINGZ "USINGZ" OFF)

FetchContent_MakeAvailable(clipper)

add_library(Clipper::clipper ALIAS Clipper2)
