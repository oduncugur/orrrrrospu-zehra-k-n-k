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
  src/game/Career.cpp
  src/game/RoadPath.cpp
  src/game/RoadCar.cpp
  src/game/RoadSession.cpp
  src/game/Settings.cpp
  src/game/FlowScore.cpp
  src/game/Contact.cpp
  src/garage/VehicleCatalog.cpp
  src/garage/LowPolyModel.cpp
  src/sim/PartTables.cpp
  src/audio/ProceduralEngineAudio.cpp
  src/audio/TireAudio.cpp)
target_include_directories(zk_sim PUBLIC src)
if(NOT MSVC)
  target_compile_options(zk_sim PRIVATE -Wall -Wextra)
endif()
add_executable(zehra_sim src/main.cpp)
target_link_libraries(zehra_sim PRIVATE zk_sim)
add_executable(zehra_garage src/garage_main.cpp)
target_link_libraries(zehra_garage PRIVATE zk_sim)
# Rakip dengesi: tum araclar x YZ parca setleri 1/4 mil tablosu (src/game/EtTable.inc) uretir / analiz eder
add_executable(zehra_ettable src/ettable_main.cpp)
target_link_libraries(zehra_ettable PRIVATE zk_sim)

# Grafik masaustu istemcisi (SDL3 + OpenGL 3.3). SDL3 bulunamazsa atlanir.
find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
  add_executable(zehra_game WIN32 src/desktop_main.cpp src/app/App.cpp src/app/Renderer.cpp src/app/GarageScreen.cpp
                 src/app/DragScreen.cpp src/app/MenuScreens.cpp src/app/PartsScreen.cpp src/app/JunkScreens.cpp src/app/Lang.cpp src/app/RoadScreen.cpp src/app/SettingsScreen.cpp src/app/Cockpit.cpp src/app/GLApi.cpp)
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
add_executable(career_test tests/career_test.cpp)
target_link_libraries(career_test PRIVATE zk_sim)
target_include_directories(career_test PRIVATE src)
add_test(NAME kariyer_kayit COMMAND career_test)
add_executable(planar_test tests/planar_test.cpp)
target_link_libraries(planar_test PRIVATE zk_sim)
target_include_directories(planar_test PRIVATE src)
add_test(NAME duzlemsel_dinamik COMMAND planar_test)
add_executable(road_test tests/road_test.cpp)
target_link_libraries(road_test PRIVATE zk_sim)
add_test(NAME acik_yol COMMAND road_test)
add_executable(settings_test tests/settings_test.cpp)
target_link_libraries(settings_test PRIVATE zk_sim)
add_test(NAME ayarlar COMMAND settings_test)
add_executable(flow_test tests/flow_test.cpp)
target_link_libraries(flow_test PRIVATE zk_sim)
add_test(NAME otoban_akisi COMMAND flow_test)
add_executable(contact_test tests/contact_test.cpp)
target_link_libraries(contact_test PRIVATE zk_sim)
add_test(NAME arac_temasi COMMAND contact_test)
add_executable(parts_test tests/parts_test.cpp)
target_link_libraries(parts_test PRIVATE zk_sim)
add_test(NAME modifiye COMMAND parts_test)
add_executable(model_test tests/model_test.cpp)
target_link_libraries(model_test PRIVATE zk_sim)
add_test(NAME arac_modelleri COMMAND model_test)
add_executable(electronics_test tests/electronics_test.cpp)
target_link_libraries(electronics_test PRIVATE zk_sim)
add_test(NAME abs_tc COMMAND electronics_test)
