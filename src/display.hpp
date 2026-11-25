#pragma once

#include <memory>
#include <array>

#include "cpu.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

class Display {
public:
    // `scale` is the size of one LCD dot in screen pixels.
    // Throws std::runtime_error if SDL or the window can't be initialised.
    explicit Display(int scale = 16);
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    // Draw a frame and show it
    void present(const LcdFrame& frame);

    // Handle pending window events, including button presses. Returns false
    // once the window is closed.
    bool poll_events();

    // The three device buttons, left to right (A, B, C). Each can be held with
    // the mouse or the keyboard (A / S / D keys). true = held down.
    static constexpr int num_buttons = 3;
    std::array<bool, num_buttons> buttons() const;

private:
    // Deleters let unique_ptr free SDL objects; defined in display.cpp
    struct SdlDeleter {
        void operator()(SDL_Window* w) const;
        void operator()(SDL_Renderer* r) const;
        void operator()(SDL_Texture* t) const;
    };

    int scale_;

    using TexturePtr = std::unique_ptr<SDL_Texture, SdlDeleter>;

    std::unique_ptr<SDL_Window, SdlDeleter> window_;
    std::unique_ptr<SDL_Renderer, SdlDeleter> renderer_;
    TexturePtr texture_;  // 32x16, one texel per dot

    std::array<TexturePtr, 8> icons_{}; // 64x64, PNG w/ transparency
    TexturePtr disc_;                    // anti-aliased white disc, tinted per button

    int mouse_button_ = -1;  // button held by the left mouse button, or -1
    std::array<bool, num_buttons> key_held_{};

    void draw_buttons();
    int button_at(int x, int y) const;  // button under a window position, or -1
};
