if(TARGET Eigen3::Eigen)
  return()
endif()

message(STATUS "[frite] Adding target Eigen3::Eigen")

include(FetchContent)

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/toastations/eigen.git
  GIT_TAG frite
  GIT_SHALLOW TRUE
)

FetchContent_GetProperties(eigen)
if(NOT eigen_POPULATED)
  FetchContent_Populate(eigen)
endif()
set(EIGEN3_INCLUDE_DIR ${eigen_SOURCE_DIR})


add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)

target_include_directories(Eigen3::Eigen SYSTEM INTERFACE ${EIGEN3_INCLUDE_DIR})
target_compile_definitions(Eigen3::Eigen INTERFACE
  -DEIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
)