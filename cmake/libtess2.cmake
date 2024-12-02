if(TARGET Libtess2::libtess2)
    return()
endif()

message(STATUS "[frite] Adding target Libtess2::libtess2")

include(FetchContent)
FetchContent_Declare(
    libtess2
    GIT_REPOSITORY https://github.com/Toastation/libtess2.git
    GIT_TAG 5fd48ce7a92581ae387dc5a7889e5b119c29a6b5
    GIT_SHALLOW FALSE
)

FetchContent_MakeAvailable(libtess2)

add_library(Libtess2::libtess2 ALIAS libtess2)