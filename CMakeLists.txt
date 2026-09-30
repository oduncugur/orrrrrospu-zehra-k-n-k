cmake_minimum_required(VERSION 3.16)
project(ZehraKinik CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MINGW)
  # Windows: tek basina calisan .exe (libgcc/libstdc++/winpthread gomulu)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

add_library(zk_sim STATIC
  src/sim/WheelSimulation.cpp
  src/sim/PowertrainCore.cpp
  src/sim/DrivetrainFailure.cpp
  src/sim/Suspension.cpp
  src/sim/VehicleSim.cpp
  src/game/DragRace.cpp
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

# Grafik masaustu istemcisi (SDL3 + OpenGL 3.3). SDL3 bulunamazsa atlanir.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  add_executable(zehra_game WIN32 src/desktop_main.cpp src/app/App.cpp src/app/Renderer.cpp src/app/GarageScreen.cpp
                 src/app/DragScreen.cpp src/app/GLApi.cpp)
  target_include_directories(zehra_game PRIVATE src third_party/khronos)
  if(TARGET SDL3::SDL3-static)
    target_link_libraries(zehra_game PRIVATE zk_sim SDL3::SDL3-static)
  else()
    target_link_libraries(zehra_game PRIVATE zk_sim SDL3::SDL3)
  endif()
  message(STATUS "zehra_game (SDL3) derlenecek")
else()
  message(STATUS "SDL3 bulunamadi: zehra_game atlandi (CMAKE_PREFIX_PATH ile SDL3 kurulumunu gosterin)")
endif()

# Testler: ctest --test-dir build
enable_testing()
add_test(NAME fizik_regresyon COMMAND ${CMAKE_SOURCE_DIR}/tests/run_regression.sh $<TARGET_FILE:zehra_sim>)
add_executable(drag_race_test tests/drag_race_test.cpp)
target_link_libraries(drag_race_test PRIVATE zk_sim)
target_include_directories(drag_race_test PRIVATE src)
add_test(NAME drag_yarisi COMMAND drag_race_test)
add_executable(tune_test tests/tune_test.cpp)
target_link_libraries(tune_test PRIVATE zk_sim)
target_include_directories(tune_test PRIVATE src)
add_test(NAME parca_etkileri COMMAND tune_test)
