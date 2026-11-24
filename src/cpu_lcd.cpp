// LCD Display Decoder
//
// The E0C6S46 LCD driver has 40 segment (SEG) lines and 16 common (COM) lines,
// so it can drive 640 dots. Each display-memory nibble holds 4 of those dots:
//
//   E00-E4F  ->  COM 0-7
//   E80-ECF  ->  COM 8-15

#include "cpu.hpp"

namespace {

// Turning (SEG, COM) into a matrix pixel or an icon depends on how the
// LCD is wired to the chip. This lookup table maps the segment positions
// to their true X/Y position in the pixel grid.
constexpr uint8_t seg_pos[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x20, 0x08, 
    0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x21, 0x22, 0x23, 
    0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, 0x24, 0x17, 
    0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x25, 0x26, 0x27,
};

// Place one lit LCD dot into the frame.
void map_dot(LcdFrame& frame, unsigned seg, unsigned com) {
    if (seg_pos[seg] < LcdFrame::width) {
        frame.screen[com] |= 1u << seg_pos[seg];
    } else {
        if (seg == 8 && com < 4) {
            frame.icons[com] = true;
        } else if (seg == 28 && com >= 12) {
            frame.icons[com - 8] = true;
        }
    }
}

static constexpr uint8_t LCD_ALL_OFF    = 0x8; 
static constexpr uint8_t LCD_ALL_ON     = 0x4;

} // namespace

void Cpu::lcd_frame(LcdFrame* frame) const {
    *frame = LcdFrame{};
    
    if (io(ALON)) {
        frame->screen.fill(0xFFFF);
        return;
    } else if (io(ALOFF)) {
        frame->screen.fill(0x0000);
        return;
    }

    frame->contrast = io(LC);

    for (unsigned H = 0; H < 16; H++) {
        if (!(H < 5 || (H >= 8 && H < 0xD))) continue;

        for (unsigned L = 0; L < 16; L++) {
            unsigned addr = (H << 4) | L;
            uint4_t dots = ram.pageE[H][L];

            unsigned seg = (addr & 0x7F) >> 1;
            unsigned com0 = ((addr & 0x80) >> 4) | ((addr & 0x1) << 2);

            for (unsigned bit = 0; bit < 4; bit++) {
                if (dots & (1u << bit)) {
                    map_dot(*frame, seg, com0 + bit);
                }
            }
        }
    }
}
