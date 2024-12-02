if(TARGET QuaZip::QuaZip)
    return()
endif()

message(STATUS "[frite] Adding target QuaZip::QuaZip")

include(FetchContent)
FetchContent_Declare(
    QuaZip
    GIT_REPOSITORY https://github.com/stachenov/quazip.git
    GIT_TAG v1.4
    GIT_SHALLOW FALSE
)

option(BUILD_SHARED_LIBS "" OFF)
option(QUAZIP_INSTALL "" OFF)
option(QUAZIP_USE_QT_ZLIB "" OFF)
set(QUAZIP_QT_MAJOR_VERSION 6 CACHE STRING "" FORCE)

FetchContent_MakeAvailable(QuaZip)