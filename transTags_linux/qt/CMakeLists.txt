cmake_minimum_required(VERSION 3.16)

project(transTags_linux VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11 xfixes xext)

add_executable(transTags_linux
    main.cpp
)

target_include_directories(transTags_linux PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(transTags_linux PRIVATE ${X11_CFLAGS_OTHER})
target_link_libraries(transTags_linux PRIVATE
    Qt6::Widgets
    ${X11_LIBRARIES}
)
