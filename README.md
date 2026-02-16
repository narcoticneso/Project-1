# Ray Tracer – Project 1

## Overview

This project implements a Whitted-style recursive ray-tracer that supports:
  - Phong illumination modeling
  - Triangle-ray intersections
  - Smooth shading through per-vertex normal interpolation
  - Cube mapping as a user-selectable option

Scenes can be rendered from ray or JSON-formatted models once added to the project. This system can read both .bmp and .png files for texture maps and can write out images produced as .bmps.

## Compilation Instructions

This project uses **CMake**.

### Build Steps

From the project root directory to generate the executable build/ray:

```bash
mkdir build
cd build
cmake ..
make -j8
```

### Running the ray tracer:

```bash
./ray [options] input.ray output.bmp
```
