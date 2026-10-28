---
name: sdl3-c
description: Use when writing, debugging, building, or refactoring C code using SDL3 (Simple DirectMedia Layer 3) or CMake for SDL3.
---

# SDL3 C Development Skill

This skill guides development for modern C applications using SDL3 (Simple DirectMedia Layer 3).

## Key Differences in SDL3 compared to SDL2

1. **Initialization & Shutdown**:
   - `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS)`
   - `SDL_Quit()`
   - Always check return status: most return boolean `bool` (`true` on success, `false` on failure).
   - In SDL3, error details are fetched with `SDL_GetError()`.

2. **Window and Renderer**:
   - Creating window: `SDL_Window *window = SDL_CreateWindow("Title", width, height, SDL_WINDOW_RESIZABLE);`
     *(Note: position parameters like `SDL_WINDOWPOS_CENTERED` were removed in SDL3; title and size are direct arguments)*.
   - Creating renderer: `SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);`
   - Window & Renderer combined shortcut: `SDL_CreateWindowAndRenderer("Title", width, height, SDL_WINDOW_RESIZABLE, &window, &renderer);`

3. **Color and Drawing**:
   - Color values in 2D render functions frequently use float coordinates (`SDL_FRect`) or `SDL_SetRenderDrawColor(renderer, r, g, b, a)` (0-255).
   - `SDL_RenderClear(renderer)`
   - `SDL_RenderFillRect(renderer, const SDL_FRect *rect)`
   - `SDL_RenderPresent(renderer)`

4. **Event Loop**:
   - Use `SDL_PollEvent(&event)`.
   - Event types: `SDL_EVENT_QUIT`, `SDL_EVENT_KEY_DOWN`, etc.
   - For keyboard events: `event.key.key` has keysym `SDLK_ESCAPE`, etc.

5. **Cleanup**:
   - `SDL_DestroyRenderer(renderer);`
   - `SDL_DestroyWindow(window);`
   - `SDL_Quit();`

## Modern CMake Configuration for SDL3

In `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(my_sdl3_project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL3 REQUIRED CONFIG)

add_executable(my_sdl3_project src/main.c)
target_link_libraries(my_sdl3_project PRIVATE SDL3::SDL3)
```

## Available Project Tools
- Use the local MCP server `sdl3` tools:
  - `sdl3_search_symbol`: look up SDL3 function signatures, types, or enums.
  - `sdl3_get_definition`: inspect exact header signatures and doc comments.
  - `sdl3_list_headers`: list available headers in the installed SDL3 dev package.
