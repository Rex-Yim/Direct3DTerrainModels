cmake_minimum_required(VERSION 3.20)
project(maeg4060_stage2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} WIN32
    src/main.cpp
    src/Camera.cpp
    src/Camera.h
    src/Graphics.cpp
    src/Graphics.h
    src/ModelLoader.cpp
    src/ModelLoader.h
)

if(DEFINED ENV{DXSDK_DIR})
    target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Include")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Lib/x64")
    else()
        target_link_directories(${PROJECT_NAME} PRIVATE "$ENV{DXSDK_DIR}/Lib/x86")
    endif()
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE d3d9 d3dx9)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
endif()
