cmake_minimum_required(VERSION 3.31)
project(VulkanDrawingApp)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(volk CONFIG REQUIRED)
find_package(SDL3 REQUIRED CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(slang CONFIG REQUIRED)

set(SOURCES src/main.cpp)

add_executable(VulkanDrawingApp ${SOURCES})

target_link_libraries(VulkanDrawingApp PRIVATE
        volk::volk
        SDL3::SDL3
        GPUOpen::VulkanMemoryAllocator
        glm::glm
        tinyobjloader::tinyobjloader
        slang::slang)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets")

add_compile_options(-fno-exceptions)


