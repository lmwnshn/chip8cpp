#pragma once
#include <array>
#include <bitset>
#include <cstdint>
#include <random>

namespace chip8 {

constexpr size_t SCREEN_WIDTH = 64;         ///< chip8 screen width
constexpr size_t SCREEN_HEIGHT = 32;        ///< chip8 screen height
constexpr size_t NUM_KEYS = 16;             ///< chip8 keypad size
constexpr size_t NUM_REGISTERS = 16;        ///< chip8 num registers

constexpr size_t MEMORY_LIMIT = 4096;       ///< chip8 typical memory
constexpr size_t STACK_LIMIT = 16;          ///< chip8 typical stack size
constexpr uint16_t ROM_LOCATION = 0x200;

/* Built-in chip8 font utilities. */
constexpr std::array<uint8_t, 80> FONTSET{
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

/**
 * \brief chip8 interpreter.
 *
 * Reference: http://mattmik.com/files/chip8/mastering/chip8.html
 */
class Chip8 {
 public:

  Chip8();
  ~Chip8();

  /**
   * Reset all chip8 internal state.
   */
  void Reset();

  /**
   * Loads the ROM at file_path into the chip8.
   * @param file_path path to ROM to be loaded
   * @return true if loaded successfully, false otherwise
   */
  bool Load(const std::string &file_path);

  void Step();

  /**
   * @return true if we have an update for the screen
   */
  bool ShouldRedraw();

  /**
   * Redraws the current chip8 screen into buf.
   * @param buf destination screen
   */
  void Redraw(std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> &buf);

  /**
   * Press the key at the given index.
   * @param key_index index of key pressed [0, 15]
   */
  void KeyDown(int key_index);

  /**
   * Release the key at the given index.
   * @param key_index index of key released [0, 15]
   */
  void KeyUp(int key_index);

 private:
  std::bitset<NUM_KEYS> keys_;                                ///< keypad

  std::array<uint8_t, SCREEN_WIDTH * SCREEN_HEIGHT> gfx_;     ///< graphics buffer
  std::array<uint16_t, STACK_LIMIT> stack_;                   ///< stack

  std::array<uint8_t, MEMORY_LIMIT> mem_;                     ///< memory
  std::array<uint8_t, NUM_REGISTERS> V_;                      ///< data registers

  uint16_t I_;                                                ///< address register
  uint16_t sp_;                                               ///< stack pointer
  uint16_t pc_;                                               ///< program counter
  uint16_t opcode_;                                           ///< current opcode

  uint8_t delay_timer_;                                       ///< delay timer
  uint8_t sound_timer_;                                       ///< sound timer

  std::default_random_engine rng_eng_;                        ///< rng engine
  std::uniform_int_distribution<uint8_t> rng_;                ///< random number generator

  bool should_redraw_;                                        ///< whether the chip8 should redraw
};

}  // namespace chip8