cmake_minimum_required(VERSION 3.14)

project(frite VERSION 1.0)

if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING
  "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
  FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(cmake/eigen.cmake)
include(cmake/fgt.cmake)
include(cmake/cpd.cmake)
include(cmake/nanoflann.cmake)

unset(EXECUTABLE_OUTPUT_PATH CACHE)

add_subdirectory(src)
