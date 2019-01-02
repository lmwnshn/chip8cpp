#include <array>
#include <chrono>
#include <iostream>
#include <thread>

#include "SDL2/SDL.h"

#include "include/chip8.h"

constexpr int EXIT_CODE_ERR = 1;
constexpr int EXIT_CODE_BAD_LOAD = 2;

constexpr const char *EMU_TITLE = "chip8";
constexpr int EMU_HEIGHT = 600;
constexpr int EMU_WIDTH = 600;
constexpr int DEFAULT_SLEEP_MICROSEC = 100;

/*
 * We accept the popular input mapping
 * Keyboard ==>  Chip8
 * 1 2 3 4  ==>  1 2 3 C
 * Q W E R  ==>  4 5 6 D
 * A S D F  ==>  7 8 9 E
 * Z X C V  ==>  A 0 B F
 */
#define KEYMAP_0 SDLK_x
#define KEYMAP_1 SDLK_1
#define KEYMAP_2 SDLK_2
#define KEYMAP_3 SDLK_3
#define KEYMAP_4 SDLK_q
#define KEYMAP_5 SDLK_w
#define KEYMAP_6 SDLK_e
#define KEYMAP_7 SDLK_a
#define KEYMAP_8 SDLK_s
#define KEYMAP_9 SDLK_d
#define KEYMAP_A SDLK_z
#define KEYMAP_B SDLK_c
#define KEYMAP_C SDLK_4
#define KEYMAP_D SDLK_r
#define KEYMAP_E SDLK_f
#define KEYMAP_F SDLK_v
#define KEYMAP_FASTER SDLK_5
#define KEYMAP_SLOWER SDLK_t

#define CASE_KEYMAP(keysym, func_call, sleep_var) \
  switch (keysym) { \
    case KEYMAP_0: { func_call(0); break; } \
    case KEYMAP_1: { func_call(1); break; } \
    case KEYMAP_2: { func_call(2); break; } \
    case KEYMAP_3: { func_call(3); break; } \
    case KEYMAP_4: { func_call(4); break; } \
    case KEYMAP_5: { func_call(5); break; } \
    case KEYMAP_6: { func_call(6); break; } \
    case KEYMAP_7: { func_call(7); break; } \
    case KEYMAP_8: { func_call(8); break; } \
    case KEYMAP_9: { func_call(9); break; } \
    case KEYMAP_A: { func_call(10); break; } \
    case KEYMAP_B: { func_call(11); break; } \
    case KEYMAP_C: { func_call(12); break; } \
    case KEYMAP_D: { func_call(13); break; } \
    case KEYMAP_E: { func_call(14); break; } \
    case KEYMAP_F: { func_call(15); break; } \
    case KEYMAP_FASTER: { sleep_var = sleep_var / 2 + 1; break; } \
    case KEYMAP_SLOWER: { sleep_var *= 2; break; } \
    default: { break; } \
  }

int main(int argc, char *argv[]) {

  if (argc != 2) {
    std::cout << "Usage: chip8cpp <ROM>" << std::endl;
    return EXIT_CODE_ERR;
  }

  std::cout << "Keyboard \t==> Chip8" << std::endl;
  std::cout << static_cast<char>(KEYMAP_1) << " "
            << static_cast<char>(KEYMAP_2) << " "
            << static_cast<char>(KEYMAP_3) << " "
            << static_cast<char>(KEYMAP_C) << " \t==> 1 2 3 C " << std::endl;
  std::cout << static_cast<char>(KEYMAP_4) << " "
            << static_cast<char>(KEYMAP_5) << " "
            << static_cast<char>(KEYMAP_6) << " "
            << static_cast<char>(KEYMAP_D) << " \t==> 4 5 6 D " << std::endl;
  std::cout << static_cast<char>(KEYMAP_7) << " "
            << static_cast<char>(KEYMAP_8) << " "
            << static_cast<char>(KEYMAP_9) << " "
            << static_cast<char>(KEYMAP_E) << " \t==> 7 8 9 E " << std::endl;
  std::cout << static_cast<char>(KEYMAP_A) << " "
            << static_cast<char>(KEYMAP_0) << " "
            << static_cast<char>(KEYMAP_B) << " "
            << static_cast<char>(KEYMAP_F) << " \t==> A 0 B F " << std::endl;
  std::cout << "Emulation speed controls"
            << ": faster [" << static_cast<char>(KEYMAP_FASTER)
            << "] slower [" << static_cast<char>(KEYMAP_SLOWER)
            << "]" << std::endl;


  // I _could_ refactor this, but I doubt it will change or be swapped out

  // initialize SDL

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cout << "[ERR/SDL_Init] " << SDL_GetError() << std::endl;
    return EXIT_CODE_ERR;
  }

  SDL_Window *window = SDL_CreateWindow(
      EMU_TITLE,              // create a window named EMU_TITLE
      SDL_WINDOWPOS_CENTERED, // centered about x
      SDL_WINDOWPOS_CENTERED, // centered about y
      EMU_WIDTH,              // width EMU_WIDTH
      EMU_HEIGHT,             // height EMU_HEIGHT
      SDL_WINDOW_SHOWN        // and show it
  );
  if (window == nullptr) {
    std::cout << "[ERR/SDL_CreateWindow] " << SDL_GetError() << std::endl;
    SDL_Quit();
    return EXIT_CODE_ERR;
  }

  SDL_Renderer *renderer = SDL_CreateRenderer(
      window,                                              // make a 2d renderer for the given window
      -1,                                                  // with any driver which satisfies spec
      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC // and request hardware acceleration and vsync
  );
  if (renderer == nullptr) {
    std::cout << "[ERR/SDL_Renderer] " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_CODE_ERR;
  }

  SDL_RenderSetLogicalSize(renderer, EMU_WIDTH, EMU_HEIGHT);

  SDL_Texture *texture = SDL_CreateTexture(
      renderer,                     // create a texture for the renderer
      SDL_PIXELFORMAT_ARGB8888,     // with this arbitrarily picked format
      SDL_TEXTUREACCESS_STREAMING,  // texture will change frequently
      chip8::SCREEN_WIDTH,          // width SCREEN_WIDTH
      chip8::SCREEN_HEIGHT          // height SCREEN_HEIGHT
  );
  if (texture == nullptr) {
    std::cout << "[ERR/SDL_CreateTexture] " << SDL_GetError() << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_CODE_ERR;
  }

  // game loop

  std::array<uint32_t, chip8::SCREEN_WIDTH * chip8::SCREEN_HEIGHT> texture_buf{};
  chip8::Chip8 chip8;
  SDL_Event sdl_event;
  bool is_running = true;

  if (!chip8.Load(argv[1])) {
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_CODE_BAD_LOAD;
  }

  int sleep_duration = DEFAULT_SLEEP_MICROSEC;

  while (is_running) {
    chip8.Step();
    while (SDL_PollEvent(&sdl_event) == 1) {
      switch (sdl_event.type) {
        case SDL_QUIT:is_running = false;
          break;
        case SDL_KEYDOWN:CASE_KEYMAP(sdl_event.key.keysym.sym, chip8.KeyDown, sleep_duration)
          break;
        case SDL_KEYUP:CASE_KEYMAP(sdl_event.key.keysym.sym, chip8.KeyUp, sleep_duration)
          break;
        default:break;
      }
    }
    if (chip8.ShouldRedraw()) {
      chip8.Redraw(texture_buf);
      SDL_UpdateTexture(texture, nullptr, &texture_buf, chip8::SCREEN_WIDTH * sizeof(uint32_t));
      SDL_RenderClear(renderer);
      SDL_RenderCopy(renderer, texture, nullptr, nullptr);
      SDL_RenderPresent(renderer);
    }
    std::this_thread::sleep_for(std::chrono::microseconds(sleep_duration));
  }

  return 0;
}
