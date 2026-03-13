#include "window.h"
#include <SDL2/SDL_opengl.h>
#include <iostream>

Window::Window(const std::string& title, int windowWidth, int windowHeight)
    : width(windowWidth), height(windowHeight), windowedWidth(windowWidth), windowedHeight(windowHeight), open(true) {

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        open = false;
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth, windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );

    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        open = false;
        return;
    }
    
    if (!enableRenderer()) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        open = false;
        return;
    }

    refreshSize();
}

Window::~Window() {
    if (renderer) {
        SDL_DestroyRenderer(renderer);
    }
    if (glContext) {
        SDL_GL_DeleteContext(glContext);
    }
    if (window) {
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
}

bool Window::isOpen() const {
    return open;
}

void Window::close() {
    open = false;
}

void Window::handleEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            open = false;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                event.window.event == SDL_WINDOWEVENT_RESIZED) {
                refreshSize();
                if (!fullscreen) {
                    windowedWidth = width;
                    windowedHeight = height;
                }
            }
            break;
        case SDL_KEYDOWN:
            if (escapeToQuitEnabled && event.key.keysym.sym == SDLK_ESCAPE) {
                open = false;
            }
            break;
        default:
            break;
    }
}

void Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        handleEvent(event);
    }
}

void Window::present() {
    if (glContext != nullptr) {
        SDL_GL_SwapWindow(window);
    } else if (renderer != nullptr) {
        SDL_RenderPresent(renderer);
    }
}

void Window::clear(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (renderer == nullptr) {
        return;
    }
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_RenderClear(renderer);
}

bool Window::enableOpenGL() {
    if (window == nullptr) {
        return false;
    }

    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (glContext == nullptr) {
        glContext = SDL_GL_CreateContext(window);
        if (glContext == nullptr) {
            std::cerr << "GL context creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    refreshSize();
    return true;
}

bool Window::enableRenderer() {
    if (window == nullptr) {
        return false;
    }

    if (glContext != nullptr) {
        SDL_GL_MakeCurrent(window, nullptr);
        SDL_GL_DeleteContext(glContext);
        glContext = nullptr;
    }

    if (renderer == nullptr) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        }
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
    }

    if (renderer == nullptr) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }

    refreshSize();
    return true;
}

bool Window::setFullscreen(bool enabled) {
    if (window == nullptr) {
        return false;
    }
    if (fullscreen == enabled) {
        refreshSize();
        return true;
    }

    if (enabled) {
        windowedWidth = width;
        windowedHeight = height;
    }

    const Uint32 flags = enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    if (SDL_SetWindowFullscreen(window, flags) != 0) {
        std::cerr << "Failed to change fullscreen state: " << SDL_GetError() << std::endl;
        return false;
    }

    fullscreen = enabled;
    if (!enabled && windowedWidth > 0 && windowedHeight > 0) {
        SDL_SetWindowSize(window, windowedWidth, windowedHeight);
    }

    refreshSize();
    return true;
}

void Window::refreshSize() {
    if (renderer != nullptr) {
        if (SDL_GetRendererOutputSize(renderer, &width, &height) == 0) {
            return;
        }
    }

    if (window != nullptr) {
        SDL_GetWindowSize(window, &width, &height);
    }
}
