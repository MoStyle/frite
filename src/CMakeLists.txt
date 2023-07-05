set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

find_package(OpenGL)
find_package(Qt6 COMPONENTS Widgets REQUIRED Xml REQUIRED Svg REQUIRED OpenGL REQUIRED OpenGLWidgets REQUIRED Core5Compat REQUIRED)

add_definitions("-DEIGEN_QT_SUPPORT")

include_directories(core)
include_directories(gui)
include_directories(managers)
include_directories(commands)

file(GLOB_RECURSE frite_CPP "*.cpp")
file(GLOB_RECURSE frite_H "*.h")
if( APPLE )
  file(GLOB_RECURSE frite_M "*.mm")
endif()

QT6_ADD_RESOURCES(frite_RCC images.qrc shaders.qrc)

list(APPEND frite_Sources ${frite_CPP} ${frite_M} ${frite_H} ${frite_RCC})

if( APPLE )
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
  set(MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
  set(MACOSX_BUNDLE_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.app/Contents/Resources")
  set(MACOSX_BUNDLE_ICON_FILE "fries.icns")
  set(MACOSX_BUNDLE_ICON "${PROJECT_SOURCE_DIR}/src/images/${MACOSX_BUNDLE_ICON_FILE}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${MACOSX_BUNDLE_RESOURCES})
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MACOSX_BUNDLE_ICON} ${MACOSX_BUNDLE_RESOURCES})
  set(CMAKE_INSTALL_PREFIX "/Applications")

  find_library(CARBON_LIBRARY Carbon)
  find_library(APPKIT_LIBRARY AppKit)
endif( APPLE )

if( WIN32 )
  list(APPEND frite_Sources images/fries.rc)
endif( WIN32)

add_executable(frite MACOSX_BUNDLE ${frite_Sources})

target_link_libraries(frite
  PUBLIC
  Qt6::Widgets Qt6::Xml Qt6::Svg Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Core5Compat
  Cpd::cpd Fgt::fgt
  nanoflann::nanoflann
  Eigen3::Eigen
  OpenGL::GL
)

if (APPLE)
  target_link_libraries(frite PUBLIC ${APPKIT_LIBRARY} ${CARBON_LIBRARY})
endif()

if(UNIX AND NOT APPLE)
  string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)
  set(BIN_INSTALL_DIR "bin")
  set(DOC_INSTALL_DIR "share/doc/${PROJECT_NAME_LOWERCASE}/")
else()
  set(BIN_INSTALL_DIR ".")
  set(DOC_INSTALL_DIR ".")
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})

# Installation
if(UNIX AND NOT APPLE)
  string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)
  set(BIN_INSTALL_DIR "bin")
  set(DOC_INSTALL_DIR "share/doc/${PROJECT_NAME_LOWERCASE}/")
else()
  set(BIN_INSTALL_DIR ".")
  set(DOC_INSTALL_DIR ".")
endif()

install(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_INSTALL_DIR})

if(WIN32)
  include(Windeployqt)
  windeployqt(${PROJECT_NAME} ${BIN_INSTALL_DIR})
endif()

# if(APPLE)
#   include(Macdeployqt)
#   macdeployqt(${PROJECT_NAME})
# endif()

include(FriteCPack)
