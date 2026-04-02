#pragma once

#include <SDL2/SDL.h>

namespace platform::dpi {

inline void prepareSdlForHighDpiWindows() {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
#endif
}

inline Uint32 addHighDpiWindowFlag(Uint32 windowFlags) {
#ifdef _WIN32
    return windowFlags | SDL_WINDOW_ALLOW_HIGHDPI;
#else
    return windowFlags;
#endif
}

}  // namespace platform::dpi
