# sunset

A small C++/OpenGL 3D renderer. It loads glTF models (converted from GameCube BMD assets), plays back skeletal animations, and renders them with shadow mapping, a sky pass, and a lens flare effect. Includes a free-fly camera and support for scripted cutscenes (STB format).

## Features

- glTF model loading with skeletal (skinned) and static mesh support
- Shadow mapping
- Sky rendering with a lens flare pass
- Cutscene playback and animation remapping (STB format)
- Free-fly camera with mouse look and WASD movement

## Requirements

- CMake 3.10+
- A C++23 compiler
- GLFW 3.3+
- OpenGL
- GLM

glad, tinygltf, stb_image, and json.hpp are bundled under `ext/`.

## Build

```
mkdir build
cd build
cmake ..
make
```

## Run

Run the resulting binary from the repository root, so the relative paths to `shaders/` and `res/` resolve correctly:

```
./build/main
```

## Project structure

```
src/       renderer source (camera, scene, models, animations, shaders, textures, input)
shaders/   GLSL shaders (mesh, shadow, sky, lens flare)
res/       models, textures, and cutscene data
ext/       bundled third-party headers (glad, tinygltf, stb_image, json)
```

## Notes

Model and texture assets under `res/` come from external sources and are not original work. This project is a rendering exercise, not a released application, and has no installer or packaged build.
