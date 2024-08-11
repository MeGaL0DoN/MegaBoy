cmake_minimum_required(VERSION 3.27)
project(imgui)

set(CMAKE_CXX_STANDARD 20)

add_library(imgui imconfig.h imgui.cpp imgui.h imgui_draw.cpp imgui_impl_glfw.cpp
            imgui_impl_glfw.h imgui_impl_opengl3.cpp imgui_impl_opengl3.h imgui_impl_opengl3_loader.h
            imgui_internal.h imgui_tables.cpp imgui_widgets.cpp imstb_rectpack.h imstb_textedit.h imstb_truetype.h
)

target_link_libraries(imgui glfw)

target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
