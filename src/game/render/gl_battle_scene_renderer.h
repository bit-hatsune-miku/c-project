#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "../battle_session_core.h"

namespace battle::render {

class GlBattleSceneRenderer {
public:
    struct TextureInfo {
        GLuint id = 0;
        int width = 0;
        int height = 0;
    };

    ~GlBattleSceneRenderer();

    bool initialize();
    void destroy();

    bool ensureWorldAssets(const std::vector<std::string>& assetNames);
    void renderWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                     int screenWidth,
                     int screenHeight);

    bool rendersPresentationNatively(const battle::AbilityPresentation* presentation) const;
    bool renderNativeBelowWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                                int screenWidth,
                                int screenHeight);
    bool renderNativeMidWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                              int screenWidth,
                              int screenHeight);
    bool renderNativeAboveHud(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                              int screenWidth,
                              int screenHeight);
    void renderFullscreenFade(int screenWidth,
                              int screenHeight,
                              SDL_Color color);

private:
    struct Vertex {
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    TextureInfo ensureWorldTexture(const std::string& assetName);
    TextureInfo ensureSpecialTexture(const std::string& key, const std::string& resolvedPath);
    TextureInfo ensureGeneratedFloorTexture();
    TextureInfo ensureSolidWhiteTexture();

    void drawTexturedTriangles(GLuint textureId,
                               const std::vector<Vertex>& vertices,
                               int screenWidth,
                               int screenHeight);
    void appendQuad(std::vector<Vertex>& vertices,
                    float x0,
                    float y0,
                    float x1,
                    float y1,
                    float u0,
                    float v0,
                    float u1,
                    float v1,
                    SDL_Color color) const;
    void appendRotatedQuad(std::vector<Vertex>& vertices,
                           float centerX,
                           float centerY,
                           float width,
                           float height,
                           float u0,
                           float v0,
                           float u1,
                           float v1,
                           SDL_Color color,
                           float angleDegrees) const;
    void appendOutlineRect(std::vector<Vertex>& vertices,
                           float x,
                           float y,
                           float width,
                           float height,
                           float thickness,
                           SDL_Color color) const;
    void appendFilledRect(std::vector<Vertex>& vertices,
                          float x,
                          float y,
                          float width,
                          float height,
                          SDL_Color color) const;

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLint viewportLocation_ = -1;
    GLint samplerLocation_ = -1;

    TextureInfo whiteTexture_{};
    TextureInfo floorTexture_{};
    std::unordered_map<std::string, TextureInfo> worldTextures_;
    std::unordered_map<std::string, TextureInfo> specialTextures_;
};

} // namespace battle::render
