cmake_minimum_required(VERSION 3.18)
project(zehra C CXX)

# Depo koku: android/app/src/main/cpp -> ../../../../..
set(ZK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../..)

add_library(native_app_glue STATIC ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(zehra SHARED
    android_main.cpp
    ${ZK_ROOT}/src/app/App.cpp
    ${ZK_ROOT}/src/app/Renderer.cpp
    ${ZK_ROOT}/src/app/GarageScreen.cpp
    ${ZK_ROOT}/src/app/DragScreen.cpp
    ${ZK_ROOT}/src/app/MenuScreens.cpp
    ${ZK_ROOT}/src/app/GLApi.cpp
    ${ZK_ROOT}/src/sim/WheelSimulation.cpp
    ${ZK_ROOT}/src/sim/PowertrainCore.cpp
    ${ZK_ROOT}/src/sim/DrivetrainFailure.cpp
    ${ZK_ROOT}/src/sim/Suspension.cpp
    ${ZK_ROOT}/src/sim/VehicleSim.cpp
    ${ZK_ROOT}/src/game/DragRace.cpp
    ${ZK_ROOT}/src/game/Career.cpp
    ${ZK_ROOT}/src/garage/VehicleCatalog.cpp
    ${ZK_ROOT}/src/garage/LowPolyModel.cpp
    ${ZK_ROOT}/src/audio/ProceduralEngineAudio.cpp
    ${ZK_ROOT}/src/audio/TireAudio.cpp)
target_include_directories(zehra PRIVATE ${ZK_ROOT}/src)
target_compile_features(zehra PRIVATE cxx_std_17)
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")
target_link_libraries(zehra native_app_glue android log EGL GLESv3 aaudio)
