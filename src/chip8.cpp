#include <fstream>
#include <random>
#include <iostream>

#include "include/chip8.h"

namespace chip8 {

Chip8::Chip8() {
  Reset();
}

Chip8::~Chip8() = default;

void Chip8::Reset() {
  keys_.reset();
  std::fill(gfx_.begin(), gfx_.end(), 0);
  std::fill(stack_.begin(), stack_.end(), 0);
  std::fill(mem_.begin(), mem_.end(), 0);
  std::copy(FONTSET.begin(), FONTSET.end(), mem_.begin());
  std::fill(V_.begin(), V_.end(), 0);
  I_ = 0;
  sp_ = 0;
  pc_ = ROM_LOCATION;
  opcode_ = 0;
  delay_timer_ = 0;
  sound_timer_ = 0;
  should_redraw_ = true;

  std::random_device rd;
  rng_eng_ = std::default_random_engine(rd());
  rng_ = std::uniform_int_distribution<uint8_t>(0x0, 0xFF);
}

bool Chip8::Load(const std::string &file_path) {
  Reset();

  std::ifstream input;
  input.open(file_path, std::ios::binary);
  if (input.fail()) {
    return false;
  }

  auto input_it = std::istreambuf_iterator<char>(input);
  auto eos = std::istreambuf_iterator<char>();
  std::vector<uint8_t> rom(input_it, eos);
  if (rom.size() > (MEMORY_LIMIT - ROM_LOCATION)) {
    return false;
  }

  std::copy(rom.begin(), rom.end(), mem_.begin() + ROM_LOCATION);
  input.close();
  return true;
}

bool Chip8::ShouldRedraw() {
  return should_redraw_;
}

void Chip8::Redraw(std::array<uint32_t, SCREEN_WIDTH * SCREEN_HEIGHT> &buf) {
  std::transform(gfx_.begin(), gfx_.end(), buf.begin(),
                 [](uint8_t pixel) -> uint32_t { return (0x00FFFFFFu * pixel) | 0xFF000000u; });
  should_redraw_ = false;
}

void Chip8::KeyDown(int key_index) {
  keys_[key_index] = true;
}

void Chip8::KeyUp(int key_index) {
  keys_[key_index] = false;
}

void Chip8::Step() {
  opcode_ = mem_[pc_] << 8u | mem_[pc_ + 1];  // two bytes

  auto instruction = static_cast<uint16_t>(opcode_ & 0xF000u);
  auto &VX = V_[(opcode_ & 0x0F00u) >> 8u];
  auto &VY = V_[(opcode_ & 0x00F0u) >> 4u];
  auto &VF = V_[0xFu];
  auto NNN = static_cast<uint16_t>(opcode_ & 0x0FFFu);
  auto NN = static_cast<uint8_t>(opcode_ & 0x00FFu);
  auto N = static_cast<uint8_t>(opcode_ & 0x000Fu);

  switch (instruction) {
    case 0x0000: {
      switch (NN) {
        // 00E0 Clear the screen
        case 0xE0: {
          std::fill(gfx_.begin(), gfx_.end(), 0);
          should_redraw_ = true;
          pc_ += 2;
          break;
        }
          // 00EE Return from a subroutine
        case 0xEE: {
          pc_ = stack_[--sp_];
          pc_ += 2;
          break;
        }
          // 0NNN [ignored] Execute machine language subroutine at address NNN
        default: {
          assert(false);
          break;
        }
      }
      break;
    }

    case 0x1000: {
      // 1NNN Jump to address NNN
      pc_ = NNN;
      break;
    }

    case 0x2000: {
      // 2NNN Execute subroutine starting at address NNN
      stack_[sp_++] = pc_;
      pc_ = NNN;
      break;
    }

    case 0x3000: {
      // 3XNN Skip the following instruction if the value of register VX equals NN
      if (VX == NN) {
        pc_ += 2;
      }
      pc_ += 2;
      break;
    }

    case 0x4000: {
      // 4XNN Skip the following instruction if the value of register VX is not equal to NN
      if (VX != NN) {
        pc_ += 2;
      }
      pc_ += 2;
      break;
    }

    case 0x5000: {
      // 5XY0 Skip the following instruction if the value of register VX is equal to the value of register VY
      if (VX == VY) {
        pc_ += 2;
      }
      pc_ += 2;
      break;
    }

    case 0x6000: {
      // 6XNN Store number NN in register VX
      VX = NN;
      pc_ += 2;
      break;
    }

    case 0x7000: {
      // 7XNN Add the value NN to register VX
      VX += NN;
      pc_ += 2;
      break;
    }

    case 0x8000: {
      switch (N) {
        // 8XY0 Store the value of register VY in register VX
        case 0x0: {
          VX = VY;
          pc_ += 2;
          break;
        }
          // 8XY1 Set VX to VX OR VY
        case 0x1: {
          VX |= VY;
          pc_ += 2;
          break;
        }
          // 8XY2 Set VX to VX AND VY
        case 0x2: {
          VX &= VY;
          pc_ += 2;
          break;
        }
          // 8XY3 Set VX to VX XOR VY
        case 0x3: {
          VX ^= VY;
          pc_ += 2;
          break;
        }
          // 8XY4 Add the value of register VY to register VX
          //      Set VF to 01 if a carry occurs
          //      Set VF to 00 if a carry does not occur
        case 0x4: {
          if (VY > (0xFF - VX)) { VF = 1; }
          else { VF = 0; }
          VX = VX + VY;
          pc_ += 2;
          break;
        }
          // 8XY5 Subtract the value of register VY from register VX
          //      Set VF to 00 if a borrow occurs
          //      Set VF to 01 if a borrow does not occur
        case 0x5: {
          if (VY > VX) { VF = 0; }
          else { VF = 1; }
          VX = VX - VY;
          pc_ += 2;
          break;
        }
          // 8XY6 Store the value of register VY shifted right one bit in register VX
          //      Set register VF to the least significant bit prior to the shift
        case 0x6: {
          /*
            VF = static_cast<uint8_t>(VY & 0x1u);
            VY >>= 1;
            VX = VY;
          */
          // Most ROMs use the buggy Cowgod version below
          VF = static_cast<uint8_t>(VX & 0x1u);
          VX >>= 1;
          pc_ += 2;
          break;
        }
          // 8XY7 Set register VX to the value of VY minus VX
          //      Set VF to 00 if a borrow occurs
          //      Set VF to 01 if a borrow does not occur
        case 0x7: {
          if (VX > VY) { VF = 0; }
          else { VF = 1; }
          VX = VY - VX;
          pc_ += 2;
          break;
        }
          // 8XYE Store the value of register VY shifted left one bit in register VX
          //      Set register VF to the most significant bit prior to the shift
        case 0xE: {
          /*
            VF = static_cast<uint8_t>(VY >> 0x7u);
            VY <<= 1;
            VX = VY;
          */
          // Most ROMs use the buggy Cowgod version below
          VF = static_cast<uint8_t>(VX >> 0x7u);
          VX <<= 1;
          pc_ += 2;
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
      break;
    }
    case 0x9000: {
      // 9XY0 Skip the following instruction if the value of register VX is not equal to the value of register VY
      if (VX != VY) {
        pc_ += 2;
      }
      pc_ += 2;
      break;
    }
    case 0xA000: {
      // ANNN Store memory address NNN in register I
      I_ = NNN;
      pc_ += 2;
      break;
    }
    case 0xB000: {
      // BNNN Jump to address NNN + V0
      pc_ = NNN + V_[0];
      break;
    }
    case 0xC000: {
      // CXNN Set VX to a random number with a mask of NN
      VX = rng_(rng_eng_) & NN;
      pc_ += 2;
      break;
    }
    case 0xD000: {
      // DXYN Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
      //      Set VF to 01 if any set pixels are changed to unset, and 00 otherwise
      VF = 0;
      uint8_t pixel;

      for (auto yl = 0; yl < N; yl++) {
        pixel = mem_[I_ + yl];
        for (uint8_t xl = 0; xl < 8; xl++) {
          if ((pixel & (0x80u >> xl)) != 0) {
            auto &buf_pix = gfx_[VX + xl + ((VY + yl) * 64)];
            if (buf_pix == 1) {
              VF = 1;
            }
            buf_pix ^= 1;
          }
        }
      }

      should_redraw_ = true;
      pc_ += 2;
      break;
    }
    case 0xE000: {
      switch (NN) {
        // EX9E Skip the following instruction if the key corresponding to the hex value currently stored
        //      in register VX is pressed
        case 0x9E: {
          if (keys_[VX]) {
            pc_ += 2;
          }
          pc_ += 2;
          break;
        }
          // EXA1 Skip the following instruction if the key corresponding to the hex value currently stored
          //      in register VX is not pressed
        case 0xA1: {
          if (!keys_[VX]) {
            pc_ += 2;
          }
          pc_ += 2;
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
      break;
    }
    case 0xF000: {
      switch (NN) {
        // FX07 Store the current value of the delay timer in register VX
        case 0x07: {
          VX = delay_timer_;
          pc_ += 2;
          break;
        }
          // FX0A Wait for a keypress and store the result in register VX
        case 0x0A: {
          if (keys_.any()) {
            for (uint8_t i = 0; i < keys_.size(); i++) {
              if (keys_[i]) {
                VX = i;
                pc_ += 2;
                break;
              }
            }
          }
          break;
        }
          // FX15 Set the delay timer to the value of register VX
        case 0x15: {
          delay_timer_ = VX;
          pc_ += 2;
          break;
        }
          // FX18 Set the sound timer to the value of register VX
        case 0x18: {
          sound_timer_ = VX;
          pc_ += 2;
          break;
        }
          // FX1E Add the value stored in register VX to register I
        case 0x1E: {
          if (I_ + VX > 0xFFF) { VF = 1; }
          else { VF = 0; }
          I_ += VX;
          pc_ += 2;
          break;
        }
          // FX29 Set I to the memory address of the sprite data corresponding to the hexadecimal digit
          //      stored in register VX
        case 0x29: {
          I_ = static_cast<uint16_t>(VX * 5);  // 4x5 font
          pc_ += 2;
          break;
        }
          // FX33 Store the binary-coded decimal equivalent of the value stored in register VX
          //      at addresses I, I+1, and I+2
        case 0x33: {
          mem_[I_] = static_cast<uint8_t>(VX / 100);
          mem_[I_ + 1] = static_cast<uint8_t>((VX / 10) % 10);
          mem_[I_ + 2] = static_cast<uint8_t>((VX % 100) % 10);
          pc_ += 2;
          break;
        }
          // FX55 Store the values of registers V0 to VX inclusive in memory starting at address I
          //      I is set to I + X + 1 after operation
        case 0x55: {
          int xval = (opcode_ & 0x0F00) >> 8;
          std::copy_n(V_.begin(), xval + 1, mem_.begin() + I_);
          I_ = static_cast<uint16_t>(I_ + xval + 1);
          pc_ += 2;
          break;
        }
          // FX65 Fill registers V0 to VX inclusive with the values stored in memory starting at address I
          //      I is set to I + X + 1 after operation
        case 0x65: {
          int xval = (opcode_ & 0x0F00) >> 8;
          std::copy_n(mem_.begin() + I_, xval + 1, V_.begin());
          I_ = static_cast<uint16_t>(I_ + xval + 1);
          pc_ += 2;
          break;
        }
        default: {
          assert(false);
          break;
        }
      }
      break;
    }
    default: {
      assert(false);
      break;
    }
  }

  if (delay_timer_ > 0) {
    delay_timer_--;
  }

  if (sound_timer_ > 0) {
    std::cout << '\a';  // xd
    std::cout.flush();
    sound_timer_--;
  }
}

} // namespace chip8
