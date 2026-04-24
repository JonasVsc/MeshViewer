cmake_minimum_required(VERSION 3.31)

project("MeshViewer"
        VERSION 0.01
        DESCRIPTION "Vulkan Mesh Viewer"
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(ThirdParty/SDL)

find_package(Vulkan REQUIRED)
find_program(SLANGC_EXECUTABLE slangc REQUIRED)

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

file(GLOB_RECURSE SHADER_SOURCES "Assets/Shaders/*.slang")

set(SHADER_INTERMEDIATE_DIR "${CMAKE_CURRENT_BINARY_DIR}/Shaders")
set(COMPILED_SHADERS "")

foreach(SHADER_SRC IN LISTS SHADER_SOURCES)
    get_filename_component(SHADER_NAME "${SHADER_SRC}" NAME_WE)
    set(SHADER_OUT "${SHADER_INTERMEDIATE_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
        OUTPUT "${SHADER_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_INTERMEDIATE_DIR}"
        COMMAND ${SLANGC_EXECUTABLE} "${SHADER_SRC}" -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o "${SHADER_OUT}"
        MAIN_DEPENDENCY "${SHADER_SRC}"
        COMMENT "Compilando shader Slang: ${SHADER_NAME}.slang"
        VERBATIM
    )

    list(APPEND COMPILED_SHADERS "${SHADER_OUT}")
endforeach()

add_custom_target(CompileShaders DEPENDS ${COMPILED_SHADERS})

add_dependencies(${PROJECT_NAME} CompileShaders)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        "${SHADER_INTERMEDIATE_DIR}" 
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Movendo shaders (.spv) compilados para o diretório do executável..."
    VERBATIM
)