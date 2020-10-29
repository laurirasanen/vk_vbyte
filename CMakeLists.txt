cmake_minimum_required(VERSION 3.7.0 FATAL_ERROR)

project(vk_vbyte)

file(GLOB SOURCE *.c)
add_executable(vk_vbyte ${SOURCE})

include_directories(PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Vulkan REQUIRED FATAL_ERROR)
target_include_directories(vk_vbyte PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(vk_vbyte PUBLIC ${Vulkan_LIBRARIES})