if(TARGET Ink_stroke_modeler::ink_stroke_modeler)
    return()
endif()

message(STATUS "[frite] Adding target Ink_stroke_modeler::ink_stroke_modeler")

include(FetchContent)
FetchContent_Declare(
    ink_stroke_modeler
    GIT_REPOSITORY https://github.com/Toastation/ink-stroke-modeler.git
    GIT_TAG 2ba167d6e3
    GIT_SHALLOW FALSE
)


option(INK_STROKE_MODELER_BUILD_TESTING "Build tests and testonly libraries" OFF)

FetchContent_MakeAvailable(ink_stroke_modeler)
