cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(vtk-discrete-isocontouring-evaluation LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VTK flags
set(VTK_SMP_IMPLEMENTATION_TYPE TBB CACHE STRING "" FORCE)

set(FETCHCONTENT_QUIET OFF)
include(FetchContent)

FetchContent_Declare(
  VTK
  GIT_REPOSITORY https://gitlab.kitware.com/vtk/vtk.git
  GIT_TAG v9.7.0.rc3
  GIT_PROGRESS ON
  SOURCE_DIR VTK
)
FetchContent_MakeAvailable(VTK)

FetchContent_Declare(
  CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.6.2
  GIT_PROGRESS ON
  SOURCE_DIR CLI11
)
FetchContent_MakeAvailable(CLI11)

set(headers
  src/EightSpheres.h
  src/NonManifoldStats.h

  src/vtkFlyingEdgesSurfaceNets.h

  src/MMCellFlag.h
  src/MMCellMap.h
  src/MMGeometryOBJ.h
  src/MMGeometryGL.h
  src/MMSurfaceNet.h

  src/Arguments.h
  src/YamlWriter.h
)

set(sources
  src/EightSpheres.cxx
  src/NonManifoldStats.cxx

  src/vtkFlyingEdgesSurfaceNets.cxx

  src/MMCellFlag.cpp
  src/MMCellMap.cpp
  src/MMGeometryOBJ.cpp
  src/MMGeometryGL.cpp
  src/MMSurfaceNet.cpp

  src/Arguments.cxx
  src/IsocontouringEvaluation.cxx
)

add_executable(${PROJECT_NAME} ${headers} ${sources})

target_link_libraries(${PROJECT_NAME} PUBLIC
  VTK::CommonCore
  VTK::CommonDataModel
  VTK::CommonSystem
  VTK::FiltersCore
  VTK::FiltersGeneral
  VTK::IOImage
  VTK::IOLegacy
  VTK::IOXML
  CLI11::CLI11)

vtk_module_autoinit(
  TARGETS ${PROJECT_NAME}
  MODULES VTK::CommonCore VTK::CommonDataModel VTK::CommonSystem VTK::FiltersCore
  VTK::FiltersGeneral VTK::IOImage VTK::IOLegacy VTK::IOXML
)
