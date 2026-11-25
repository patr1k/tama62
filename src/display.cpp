#include "display.hpp"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Layout, in LCD dot units: an icon strip above and below the 32x16 matrix,
// giving the 32x20 overall display
constexpr int icon_strip_height = 2;
constexpr int display_width = LcdFrame::width;
constexpr int display_height = LcdFrame::height + 2 * icon_strip_height;
constexpr int icons_per_row = LcdFrame::num_icons / 2;

// Button panel below the display, also in dot units. Buttons are spaced evenly
// across the width, with a small letter under each.
constexpr int panel_height = 9;
constexpr int button_diameter = 4;
constexpr int button_top = 1;     // from the top of the panel
constexpr int label_top = 6;      // from the top of the panel
constexpr int window_height = display_height + panel_height;

// LCD-ish colors (ARGB8888)
constexpr std::uint32_t color_background = 0xFFB4C2A0;  // pale grey-green glass
constexpr std::uint32_t color_dot_on     = 0xFF1E2418;  // near-black dot
constexpr std::uint32_t color_icon_off   = 0xFFA6B392;  // faint ghost of an unlit icon

// Device shell and buttons
constexpr std::uint32_t color_shell          = 0xFFE6E1D6;  // off-white plastic
constexpr std::uint32_t color_button         = 0xFF6B7080;  // slate grey
constexpr std::uint32_t color_button_pressed = 0xFF4A4E5A;
constexpr std::uint32_t color_button_shadow  = 0x40000000;  // soft drop shadow
constexpr std::uint32_t color_label          = 0xFF8A8578;

// Keyboard shortcuts for the buttons, left to right
constexpr std::array<SDL_Keycode, 3> button_keys = {SDLK_a, SDLK_s, SDLK_d};

// 3x5 glyphs for the button labels "A", "B", "C" (one row per entry, MSB = left)
constexpr std::array<std::array<std::uint8_t, 5>, 3> label_glyphs = {{
    {0b010, 0b101, 0b111, 0b101, 0b101},
    {0b110, 0b101, 0b110, 0b101, 0b110},
    {0b011, 0b100, 0b100, 0b100, 0b011},
}};

void set_draw_color(SDL_Renderer* r, std::uint32_t argb) {
    SDL_SetRenderDrawColor(r, (argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF, (argb >> 24) & 0xFF);
}

// Tint a texture; alpha is ignored so the PNG's own transparency is kept
void set_texture_color(SDL_Texture* t, std::uint32_t argb) {
    SDL_SetTextureColorMod(t, (argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
}

// White disc on a transparent background, edge anti-aliased by 4x4 supersampling
std::vector<std::uint32_t> disc_pixels(int size) {
    std::vector<std::uint32_t> pixels(size * size);
    const float r = size / 2.0f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int inside = 0;
            for (int sy = 0; sy < 4; sy++) {
                for (int sx = 0; sx < 4; sx++) {
                    float dx = x + (sx + 0.5f) / 4 - r;
                    float dy = y + (sy + 0.5f) / 4 - r;
                    inside += dx * dx + dy * dy <= r * r;
                }
            }
            std::uint32_t alpha = inside * 255 / 16;
            pixels[y * size + x] = (alpha << 24) | 0x00FFFFFF;
        }
    }
    return pixels;
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

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        throw_sdl_error("IMG_Init failed");
    }

    window_.reset(SDL_CreateWindow("tama62",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   display_width * scale_, window_height * scale_,
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

    for (uint8_t i = 0; i < 8; i++) {
        std::string filename = "icons/" + std::to_string(i) + ".png";
        icons_[i].reset(IMG_LoadTexture(renderer_.get(), filename.c_str()));
        if (!icons_[i]) {
            // The destructor won't run if we throw, so clean up here
            std::string err = std::string("IMG_LoadTexture failed for ") + filename + ": " + IMG_GetError();
            for (auto& icon : icons_) {
                icon.reset();
            }
            texture_.reset();
            renderer_.reset();
            window_.reset();
            IMG_Quit();
            SDL_Quit();
            throw std::runtime_error(err);
        }
    }

    const int disc_size = button_diameter * scale_;
    disc_.reset(SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STATIC, disc_size, disc_size));
    if (!disc_) {
        std::string err = std::string("SDL_CreateTexture failed: ") + SDL_GetError();
        for (auto& icon : icons_) {
            icon.reset();
        }
        texture_.reset();
        renderer_.reset();
        window_.reset();
        IMG_Quit();
        SDL_Quit();
        throw std::runtime_error(err);
    }
    std::vector<std::uint32_t> disc = disc_pixels(disc_size);
    SDL_UpdateTexture(disc_.get(), nullptr, disc.data(), disc_size * sizeof(std::uint32_t));
    SDL_SetTextureBlendMode(disc_.get(), SDL_BLENDMODE_BLEND);
}

Display::~Display() {
    // Textures must go before the renderer that owns them
    disc_.reset();
    for (auto& icon : icons_) {
        icon.reset();
    }
    texture_.reset();
    renderer_.reset();
    window_.reset();
    IMG_Quit();
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

    int slot_width = display_width * scale_ / icons_per_row;
    int icon_size = icon_strip_height * scale_ * 3 / 4;
    SDL_Rect iconRect = {
        0,
        0,
        icon_size,
        icon_size,
    };
    for (int i = 0; i < LcdFrame::num_icons; i++) {
        auto icon = icons_[i].get();
        int column = i % icons_per_row;
        int strip_top = (i < icons_per_row) ? 0 : (icon_strip_height + LcdFrame::height) * scale_;

        iconRect.x = column * slot_width + (slot_width - icon_size) / 2;
        iconRect.y = strip_top + (icon_strip_height * scale_ - icon_size) / 2;
        SDL_SetTextureBlendMode(icon, SDL_BLENDMODE_BLEND);
        set_texture_color(icon, frame.icons[i] ? color_dot_on : color_icon_off);
        SDL_RenderCopy(r, icon, nullptr, &iconRect);
    }

    draw_buttons();

    SDL_RenderPresent(r);
}

namespace {

// Centre of button `i` in window pixels
SDL_Point button_center(int i, int scale) {
    int spacing = display_width * scale / (Display::num_buttons + 1);
    return {spacing * (i + 1), (display_height + button_top) * scale + button_diameter * scale / 2};
}

} // namespace

void Display::draw_buttons() {
    SDL_Renderer* r = renderer_.get();
    SDL_Texture* disc = disc_.get();

    SDL_Rect panel = {0, display_height * scale_, display_width * scale_, panel_height * scale_};
    set_draw_color(r, color_shell);
    SDL_RenderFillRect(r, &panel);

    const int size = button_diameter * scale_;
    const int depth = std::max(2, scale_ / 5);  // how far a button sinks when pressed
    const std::array<bool, num_buttons> held = buttons();

    for (int i = 0; i < num_buttons; i++) {
        SDL_Point c = button_center(i, scale_);
        SDL_Rect rect = {c.x - size / 2, c.y - size / 2, size, size};

        // Shadow stays put; the button drops onto it when pressed
        SDL_Rect shadow = {rect.x, rect.y + depth, size, size};
        set_texture_color(disc, color_button_shadow);
        SDL_SetTextureAlphaMod(disc, (color_button_shadow >> 24) & 0xFF);
        SDL_RenderCopy(r, disc, nullptr, &shadow);
        SDL_SetTextureAlphaMod(disc, 0xFF);

        if (held[i]) {
            rect.y += depth;
        }
        set_texture_color(disc, held[i] ? color_button_pressed : color_button);
        SDL_RenderCopy(r, disc, nullptr, &rect);

        // Label, drawn from 3x5 glyph cells
        const int cell = std::max(1, scale_ / 4);
        const int label_x = c.x - 3 * cell / 2;
        const int label_y = (display_height + label_top) * scale_ + (scale_ * 2 - 5 * cell) / 2;
        set_draw_color(r, color_label);
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                if (label_glyphs[i][row] & (0b100 >> col)) {
                    SDL_Rect px = {label_x + col * cell, label_y + row * cell, cell, cell};
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
    }
}

int Display::button_at(int x, int y) const {
    const int radius = button_diameter * scale_ / 2;
    for (int i = 0; i < num_buttons; i++) {
        SDL_Point c = button_center(i, scale_);
        int dx = x - c.x;
        int dy = y - c.y;
        if (dx * dx + dy * dy <= radius * radius) {
            return i;
        }
    }
    return -1;
}

std::array<bool, Display::num_buttons> Display::buttons() const {
    std::array<bool, num_buttons> held = key_held_;
    if (mouse_button_ >= 0) {
        held[mouse_button_] = true;
    }
    return held;
}

bool Display::poll_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_button_ = button_at(event.button.x, event.button.y);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    mouse_button_ = -1;
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                for (int i = 0; i < num_buttons; i++) {
                    if (event.key.keysym.sym == button_keys[i]) {
                        key_held_[i] = event.type == SDL_KEYDOWN;
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                // Key-up events go elsewhere while unfocused, so let go of everything
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    key_held_.fill(false);
                    mouse_button_ = -1;
                }
                break;
        }
    }
    return true;
}
