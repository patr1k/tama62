// Input ports K00-K03 (F40H) and K10-K13 (F42H)
//
// The pins have pull-ups, so an idle pin reads 1 and a pressed button
// (shorting the pin to ground) reads 0.
//
// Interrupts: each port group has a "matched" state, and IK0 / IK1 is set when
// the group goes from matched to not matched. Only pins enabled in the mask
// register (F14H / F15H) take part, so with every pin masked the group always
// counts as matched and no flag can be set.
//   K00-K03: matched when every enabled pin equals its input relation bit
//            (DFK, F41H). DFK = 1 -> falling edge, DFK = 0 -> rising edge.
//   K10-K13: matched when every enabled pin is 1 (falling edge only).
// Changing a mask or DFK bit while a pin is active can also set the flag; the
// manual warns about this (6.1.5), and the same rule reproduces it here.
//
// See section 6.1 of the E0C6S46 Technical Manual.

#include "cpu.hpp"

namespace {

constexpr IoReg IK0 {0x0, 0x4, 0x1};
constexpr IoReg IK1 {0x0, 0x5, 0x1};

} // namespace

void Cpu::set_input_port(int port, uint4_t value) {
    ram.pageF[0x4][port == 0 ? 0x0 : 0x2] = value & 0xF;
    input_update();
}

void Cpu::input_update() {
    const uint4_t k0 = ram.pageF[0x4][0x0];
    const uint4_t dfk0 = ram.pageF[0x4][0x1];
    const uint4_t k1 = ram.pageF[0x4][0x2];
    const uint4_t eik0 = ram.pageF[0x1][0x4];
    const uint4_t eik1 = ram.pageF[0x1][0x5];

    bool k0_matched = ((k0 ^ dfk0) & eik0) == 0;
    bool k1_matched = ((k1 ^ 0xF) & eik1) == 0;

    if (input.k0_matched && !k0_matched) io_write(IK0, true);
    if (input.k1_matched && !k1_matched) io_write(IK1, true);

    input.k0_matched = k0_matched;
    input.k1_matched = k1_matched;
}
