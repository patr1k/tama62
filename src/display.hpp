#pragma once

#include <memory>

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

    // Handle pending window events. Returns false once the window is closed.
    // (Keyboard -> button input will go here later.)
    bool poll_events();

private:
    // Deleters let unique_ptr free SDL objects; defined in display.cpp
    struct SdlDeleter {
        void operator()(SDL_Window* w) const;
        void operator()(SDL_Renderer* r) const;
        void operator()(SDL_Texture* t) const;
    };

    int scale_;
    std::unique_ptr<SDL_Window, SdlDeleter> window_;
    std::unique_ptr<SDL_Renderer, SdlDeleter> renderer_;
    std::unique_ptr<SDL_Texture, SdlDeleter> texture_;  // 32x16, one texel per dot
};
