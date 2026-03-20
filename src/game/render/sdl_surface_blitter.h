#pragma once

#include <SDL2/SDL.h>

namespace battle::render {

class SdlSurfaceBlitter {
public:
    explicit SdlSurfaceBlitter(SDL_Renderer* renderer = nullptr)
        : renderer_(renderer) {}

    ~SdlSurfaceBlitter() {
        destroy();
    }

    void setRenderer(SDL_Renderer* renderer) {
        if (renderer_ == renderer) {
            return;
        }
        destroy();
        renderer_ = renderer;
    }

    void destroy() {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        textureWidth_ = 0;
        textureHeight_ = 0;
    }

    bool uploadSurface(SDL_Surface* surface) {
        if (renderer_ == nullptr || surface == nullptr) {
            return false;
        }

        if (!ensureTextureSize(surface->w, surface->h)) {
            return false;
        }

        if (SDL_UpdateTexture(texture_, nullptr, surface->pixels, surface->pitch) != 0) {
            return false;
        }

        return true;
    }

    void draw() const {
        if (renderer_ == nullptr || texture_ == nullptr) {
            return;
        }

        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    }

private:
    bool ensureTextureSize(int width, int height) {
        if (texture_ != nullptr && textureWidth_ == width && textureHeight_ == height) {
            return true;
        }

        destroy();

        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, width, height);
        if (texture_ == nullptr) {
            return false;
        }

        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
        textureWidth_ = width;
        textureHeight_ = height;
        return true;
    }

    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
};

} // namespace battle::render
