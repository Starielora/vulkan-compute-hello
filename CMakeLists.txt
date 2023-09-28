cmake_minimum_required(VERSION 3.26)

project(vulkan-compute-hello)
    set(CMAKE_CXX_STANDARD 20)

    find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)
    add_executable(vulkan-compute-hello main.cpp)

    target_link_libraries(vulkan-compute-hello PRIVATE Vulkan::Vulkan Vulkan::shaderc_combined)
