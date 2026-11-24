#include "display.hpp"

#include <SDL.h>

#include <array>
#include <stdexcept>
#include <string>

namespace {

// Layout, in LCD dot units: an icon strip above and below the 32x16 matrix,
// giving the 32x20 overall display
constexpr int icon_strip_height = 2;
constexpr int display_width = LcdFrame::width;
constexpr int display_height = LcdFrame::height + 2 * icon_strip_height;
constexpr int icons_per_row = LcdFrame::num_icons / 2;

// LCD-ish colors (ARGB8888)
constexpr std::uint32_t color_background = 0xFFB4C2A0;  // pale grey-green glass
constexpr std::uint32_t color_dot_on     = 0xFF1E2418;  // near-black dot
constexpr std::uint32_t color_icon_off   = 0xFFA6B392;  // faint ghost of an unlit icon

void set_draw_color(SDL_Renderer* r, std::uint32_t argb) {
    SDL_SetRenderDrawColor(r, (argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

[[noreturn]] void throw_sdl_error(const char* what) {
    throw std::runtime_error(std::string(what) + ": " + SDL_GetError());
}

} // namespace

void Display::SdlDeleter::operator()(SDL_Window* w) const { SDL_DestroyWindow(w); }
void Display::SdlDeleter::operator()(SDL_Renderer* r) const { SDL_DestroyRenderer(r); }
void Display::SdlDeleter::operator()(SDL_Texture* t) const { SDL_DestroyTexture(t); }

Display::Display(int scale) : scale_(scale) {
    // Leave Ctrl-C to our own SIGINT handler in main.cpp
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        throw_sdl_error("SDL_Init failed");
    }

    window_.reset(SDL_CreateWindow("tama62",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   display_width * scale_, display_height * scale_,
                                   0));
    if (!window_) {
        SDL_Quit();
        throw_sdl_error("SDL_CreateWindow failed");
    }

    // No vsync: main.cpp paces frames itself, and vsync would block the tick loop
    renderer_.reset(SDL_CreateRenderer(window_.get(), -1, 0));
    if (!renderer_) {
        window_.reset();
        SDL_Quit();
        throw_sdl_error("SDL_CreateRenderer failed");
    }

    texture_.reset(SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING,
                                     LcdFrame::width, LcdFrame::height));
    if (!texture_) {
        renderer_.reset();
        window_.reset();
        SDL_Quit();
        throw_sdl_error("SDL_CreateTexture failed");
    }
}

Display::~Display() {
    texture_.reset();
    renderer_.reset();
    window_.reset();
    SDL_Quit();
}

void Display::present(const LcdFrame& frame) {
    SDL_Renderer* r = renderer_.get();

    // Dot matrix -> texture, one texel per dot
    std::array<std::uint32_t, LcdFrame::width * LcdFrame::height> pixels;
    for (int y = 0; y < LcdFrame::height; y++) {
        for (int x = 0; x < LcdFrame::width; x++) {
            pixels[y * LcdFrame::width + x] = frame.pixel(x, y) ? color_dot_on : color_background;
        }
    }
    SDL_UpdateTexture(texture_.get(), nullptr, pixels.data(), LcdFrame::width * sizeof(std::uint32_t));

    set_draw_color(r, color_background);
    SDL_RenderClear(r);

    SDL_Rect matrix = {0, icon_strip_height * scale_, LcdFrame::width * scale_, LcdFrame::height * scale_};
    SDL_RenderCopy(r, texture_.get(), nullptr, &matrix);

    // Icons: placeholder squares, 4 evenly spaced in each strip
    int slot_width = display_width * scale_ / icons_per_row;
    int icon_size = icon_strip_height * scale_ * 3 / 4;
    for (int i = 0; i < LcdFrame::num_icons; i++) {
        int column = i % icons_per_row;
        int strip_top = (i < icons_per_row) ? 0 : (icon_strip_height + LcdFrame::height) * scale_;

        SDL_Rect icon = {
            column * slot_width + (slot_width - icon_size) / 2,
            strip_top + (icon_strip_height * scale_ - icon_size) / 2,
            icon_size,
            icon_size,
        };
        set_draw_color(r, frame.icons[i] ? color_dot_on : color_icon_off);
        SDL_RenderFillRect(r, &icon);
    }

    SDL_RenderPresent(r);
}

bool Display::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
    }
    return true;
}
