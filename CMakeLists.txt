cmake_minimum_required(VERSION 3.10)
project(predprey_sim VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(predprey_sim
    src/main.cpp
)

find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

target_link_libraries(predprey_sim
    sfml-graphics
    sfml-window
    sfml-system
)

target_include_directories(predprey_sim PRIVATE include)

configure_file(config.json config.json COPYONLY)
