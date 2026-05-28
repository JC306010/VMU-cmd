cmake_minimum_required(VERSION 3.16)
project(vmuexplorer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PNG REQUIRED)

add_executable(vmu-explorer
    src/main.cpp
    src/vmu_image.cpp
    src/icon_converter.cpp
)

target_include_directories(vmu-explorer PRIVATE
    ${PNG_INCLUDE_DIRS}
    src
)

target_link_libraries(vmu-explorer
    ${PNG_LIBRARIES}
)
