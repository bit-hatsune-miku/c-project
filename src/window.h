#ifndef WINDOW_H
#define WINDOW_H

#include <SDL2/SDL.h>
#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();
    
    bool isOpen() const;
    void handleEvent(const SDL_Event& event);
    void pollEvents();
    void present();
    void clear(Uint8 r = 24, Uint8 g = 24, Uint8 b = 32, Uint8 a = 255);
    SDL_Renderer* getRenderer() const { return renderer; }
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    int width, height;
    bool open;
};

#endif
