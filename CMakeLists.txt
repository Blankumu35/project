cmake_minimum_required(VERSION 3.0)
project(project)

find_package(OpenGL REQUIRED)
set (CMAKE_CXX_STANDARD 11)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_subdirectory(external)

include_directories(
	external/glfw-3.1.2/include/
	external/glm-0.9.7.1/
	external/glad-opengl-3.3/include/
	external/tinygltf-2.9.3/
	project/
)

add_executable(lab4_skeleton
	project/lab4_skeleton.cpp
	project/render/shader.cpp
)
target_link_libraries(lab4_skeleton
	${OPENGL_LIBRARY}
	glfw
	glad
)

add_executable(lab4_character
	project/lab4_character.cpp
	project/render/shader.cpp
)
target_link_libraries(lab4_character
	${OPENGL_LIBRARY}
	glfw
	glad
)

add_executable(final_project
    project/final_project.cpp
    project/particle.cpp        
    project/render/shader.cpp
    project/terrain/ChunkedTerrain.cpp
    project/character/Bot.cpp
)


target_link_libraries(final_project
    ${OPENGL_LIBRARY}
    glfw
    glad
)