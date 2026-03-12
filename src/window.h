#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>
#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    bool isOpen() const;
    void close();
    void handleEvent(const SDL_Event& event);
    void pollEvents();
    void present();
    void clear(Uint8 r = 24, Uint8 g = 24, Uint8 b = 32, Uint8 a = 255);
    void setEscapeToQuitEnabled(bool enabled) { escapeToQuitEnabled = enabled; }
    SDL_Renderer* getRenderer() const { return renderer; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isFullscreen() const { return fullscreen; }
    bool setFullscreen(bool enabled);

private:
    void refreshSize();

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    int width = 0;
    int height = 0;
    int windowedWidth = 0;
    int windowedHeight = 0;
    bool open = false;
    bool fullscreen = false;
    bool escapeToQuitEnabled = true;
};

#endif
