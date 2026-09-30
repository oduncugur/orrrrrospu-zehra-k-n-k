cmake_minimum_required(VERSION 3.16)
project(ZehraKinik CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_library(zk_sim STATIC
  src/sim/WheelSimulation.cpp
  src/sim/PowertrainCore.cpp
  src/sim/DrivetrainFailure.cpp
  src/sim/Suspension.cpp
  src/garage/VehicleCatalog.cpp
  src/garage/LowPolyModel.cpp
  src/audio/ProceduralEngineAudio.cpp)
target_include_directories(zk_sim PUBLIC src)
if(NOT MSVC)
  target_compile_options(zk_sim PRIVATE -Wall -Wextra)
endif()
add_executable(zehra_sim src/main.cpp)
target_link_libraries(zehra_sim PRIVATE zk_sim)
add_executable(zehra_garage src/garage_main.cpp)
target_link_libraries(zehra_garage PRIVATE zk_sim)
