cmake_minimum_required(VERSION 3.10)

project(blackray)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
        ext/glad/src/glad.c

        src/main.cc
        src/camera.cpp
        src/inputs.cpp
        src/texture.cpp
        src/scene.cpp
        src/objects.h
        src/models.cpp
        src/models.h
        src/objects.cpp
        src/tinygltf.cpp
        src/tinygltf.h
        src/animations.cpp
        src/animations.h
        src/timeline.cpp
        src/timeline.h
        src/stb.cpp
        src/stb.h
)

# Header files (optional, for IDEs)
set(HEADERS
        src/main.cc
        src/camera.cpp
        src/camera.h
        src/shaders.cpp
        src/shaders.h
        src/inputs.cpp
        src/inputs.h
        src/texture.cpp
        src/texture.h
        src/scene.cpp
        src/scene.h
        src/objects.h
        src/models.cpp
        src/models.h
)

include_directories(ext/glad/include)
include_directories(ext/stbi)


add_compile_options(-g -O3 -fopenmp)
add_link_options(-fopenmp )

add_executable(main ${SOURCES} ${HEADERS})

# Include current directory (for headers)
target_include_directories(main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(main glfw)
find_package(OpenGL REQUIRED)
target_link_libraries(main OpenGL::GL)
