cmake_minimum_required(VERSION 3.31)

project("MeshViewer"
        VERSION 0.01
        DESCRIPTION "Vulkan Mesh Viewer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

find_package(Vulkan REQUIRED)
add_subdirectory(ThirdParty/SDL)

file(GLOB_RECURSE SOURCE LIST_DIRECTORIES false CONFIGURE_DEPENDS "Source/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCE})

target_include_directories(${PROJECT_NAME} PRIVATE SOURCE)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
)

target_compile_definitions(${PROJECT_NAME} PRIVATE MV_DEBUG)