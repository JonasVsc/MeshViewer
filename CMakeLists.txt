cmake_minimum_required(VERSION 3.31)

project("MeshViewer"
        VERSION 0.01
        DESCRIPTION "Vulkan Mesh Viewer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(ThirdParty/SDL)

find_package(Vulkan REQUIRED)

file(GLOB_RECURSE SOURCE LIST_DIRECTORIES false CONFIGURE_DEPENDS "Source/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCE})

target_include_directories(${PROJECT_NAME} PRIVATE SOURCE)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
)

target_compile_definitions(${PROJECT_NAME} 
    PUBLIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
)