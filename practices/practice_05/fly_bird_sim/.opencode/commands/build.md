---
description: Configure CMake and build the C SDL3 project
agent: build
---

Configure and compile the project using CMake and Ninja with compile commands generated for clangd:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```
$ARGUMENTS
