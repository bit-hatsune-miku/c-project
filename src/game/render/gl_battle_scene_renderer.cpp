#define GL_GLEXT_PROTOTYPES

#include "gl_battle_scene_renderer.h"

#include <SDL2/SDL_opengl.h>

#ifdef BATTLE_ENABLE_IMAGE
#include <SDL2/SDL_image.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

#include "../../platform/path_resolution.h"
#include "../presentation/ari_boss_presentation.h"
#include "../presentation/lyoo_boss_presentation.h"
#include "../presentation/lyoo_plot_twist_presentation.h"
#include "../presentation/miku_diandong_presentation.h"

namespace battle::render {
namespace {

constexpr int kSuggestedBaseSpriteWidth = 140;
constexpr int kSuggestedBaseSpriteHeight = 260;
constexpr float kSpriteFrameTime = 0.15f;
constexpr float kFloorNearClipDepth = 1.0f;
constexpr float kClipEpsilon = 0.0001f;
constexpr float kPi = 3.14159265f;

constexpr float kLyooPlotIntroDurationSeconds = 2.40f;
constexpr float kLyooPlotTransitionDurationSeconds = 0.55f;
constexpr float kLyooPlotRingDurationSeconds = 2.35f;
constexpr float kLyooPlotSkyDurationSeconds = 1.70f;
constexpr float kLyooPlotApproachDurationSeconds = 2.00f;
constexpr float kLyooPlotMassiveStarDurationSeconds = 2.90f;
constexpr float kLyooPlotTransitionStartSeconds = kLyooPlotIntroDurationSeconds;
constexpr float kLyooPlotRingStartSeconds = kLyooPlotTransitionStartSeconds + kLyooPlotTransitionDurationSeconds;
constexpr float kLyooPlotSkyStartSeconds = kLyooPlotRingStartSeconds + kLyooPlotRingDurationSeconds;
constexpr float kLyooPlotApproachStartSeconds = kLyooPlotSkyStartSeconds + kLyooPlotSkyDurationSeconds;
constexpr float kLyooPlotMassiveStarStartSeconds = kLyooPlotApproachStartSeconds + kLyooPlotApproachDurationSeconds;
constexpr float kLyooPlotRingOrbitRadiusPx = 155.0f;
constexpr float kLyooPlotTransitionScreenCenterY = 0.49f;
constexpr float kLyooPlotRingScreenCenterX = 0.50f;
constexpr float kLyooPlotRingScreenCenterY = 0.45f;
constexpr float kLyooPlotApproachDarknessAlpha = 105.0f;
constexpr float kAriBossSpriteTargetHeightRatio = 0.40f;
constexpr float kAriBossSpriteMinHeightPixels = 220.0f;
constexpr float kAriBossSpriteMaxHeightPixels = 360.0f;
constexpr float kAriBossSpriteWorldAnchorZ = -105.0f;

struct FloorVertex {
    float worldX = 0.0f;
    float worldY = 0.0f;
    float depth = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
};

GlBattleSceneRenderer::TextureInfo makeTextureFromSurface(SDL_Surface* surface,
                                                          GLint filter) {
    GlBattleSceneRenderer::TextureInfo texture;
    if (surface == nullptr) {
        return texture;
    }

    SDL_Surface* convertedSurface = surface;
    if (surface->format->format != SDL_PIXELFORMAT_RGBA32) {
        convertedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(surface);
        if (convertedSurface == nullptr) {
            return texture;
        }
    }

    glGenTextures(1, &texture.id);
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA8,
                 convertedSurface->w,
                 convertedSurface->h,
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 convertedSurface->pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    texture.width = convertedSurface->w;
    texture.height = convertedSurface->h;
    SDL_FreeSurface(convertedSurface);
    return texture;
}

GlBattleSceneRenderer::TextureInfo loadTextureFromPath(const std::string& resolvedPath) {
    if (resolvedPath.empty() || !std::filesystem::exists(resolvedPath)) {
        return {};
    }

#ifdef BATTLE_ENABLE_IMAGE
    SDL_Surface* surface = IMG_Load(resolvedPath.c_str());
    return makeTextureFromSurface(surface, GL_NEAREST);
#else
    (void)resolvedPath;
    return {};
#endif
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint status = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_TRUE) {
        return shader;
    }

    glDeleteShader(shader);
    return 0;
}

float lerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

float easeOutCubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - clamped;
    return 1.0f - inv * inv * inv;
}

float easeInCubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return clamped * clamped * clamped;
}

float easeInQuint(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    return clamped * clamped * clamped * clamped * clamped;
}

float easeInOutCubic(float t) {
    const float clamped = std::clamp(t, 0.0f, 1.0f);
    if (clamped < 0.5f) {
        return 4.0f * clamped * clamped * clamped;
    }

    const float f = -2.0f * clamped + 2.0f;
    return 1.0f - (f * f * f) * 0.5f;
}

FloorVertex interpolateToNearPlane(const FloorVertex& from, const FloorVertex& to) {
    const float depthDelta = to.depth - from.depth;
    float t = 0.0f;
    if (std::fabs(depthDelta) > kClipEpsilon) {
        t = (kFloorNearClipDepth - from.depth) / depthDelta;
    }
    t = std::clamp(t, 0.0f, 1.0f);

    return FloorVertex{
        from.worldX + (to.worldX - from.worldX) * t,
        from.worldY + (to.worldY - from.worldY) * t,
        kFloorNearClipDepth,
        from.u + (to.u - from.u) * t,
        from.v + (to.v - from.v) * t
    };
}

int clipFloorQuadToNearPlane(const std::array<FloorVertex, 4>& input, std::array<FloorVertex, 6>& output) {
    std::array<FloorVertex, 6> working{};
    for (size_t i = 0; i < input.size(); ++i) {
        working[i] = input[i];
    }

    int workingCount = static_cast<int>(input.size());
    int outCount = 0;
    for (int i = 0; i < workingCount; ++i) {
        const FloorVertex& current = working[static_cast<size_t>(i)];
        const FloorVertex& next = working[static_cast<size_t>((i + 1) % workingCount)];
        const bool currentInside = current.depth > kFloorNearClipDepth;
        const bool nextInside = next.depth > kFloorNearClipDepth;

        if (currentInside && nextInside) {
            output[static_cast<size_t>(outCount++)] = next;
            continue;
        }
        if (currentInside && !nextInside) {
            output[static_cast<size_t>(outCount++)] = interpolateToNearPlane(current, next);
            continue;
        }
        if (!currentInside && nextInside) {
            output[static_cast<size_t>(outCount++)] = interpolateToNearPlane(current, next);
            output[static_cast<size_t>(outCount++)] = next;
        }
    }
    return outCount;
}

std::string makeLyooBossFrameKey(int index) {
    return "lyooBoss:frame:" + std::to_string(index);
}

std::string makeLyooBossFramePath(int index) {
    char frameName[160];
    std::snprintf(frameName, sizeof(frameName), "assets/combat/presentations/lyooBoss/frame%04d.png", index);
    return platform::path::resolvePath(frameName);
}

std::string makeLyooPlotFrameKey(int index) {
    return "lyooPlot:frame:" + std::to_string(index);
}

std::string makeLyooPlotFramePath(int index) {
    char frameName[160];
    std::snprintf(frameName, sizeof(frameName), "assets/combat/presentations/lyooPlotTwist/frame%04d.png", index);
    return platform::path::resolvePath(frameName);
}

std::string makeAriBossFrameKey(int index) {
    return "ariBoss:frame:" + std::to_string(index);
}

std::string makeAriBossFramePath(int index) {
    char frameName[192];
    std::snprintf(frameName,
                  sizeof(frameName),
                  "assets/combat/presentations/ariBoss/frame_%02d_delay-0.04s.png",
                  index);
    return platform::path::resolvePath(frameName);
}

} // namespace

GlBattleSceneRenderer::~GlBattleSceneRenderer() {
    destroy();
}

bool GlBattleSceneRenderer::initialize() {
    static const char* kVertexShader = R"(
        #version 330 core
        layout (location = 0) in vec2 in_position;
        layout (location = 1) in vec2 in_uv;
        layout (location = 2) in vec4 in_color;
        uniform vec2 viewport_size;
        out vec2 frag_uv;
        out vec4 frag_color;
        void main() {
            vec2 ndc = vec2(
                (in_position.x / viewport_size.x) * 2.0 - 1.0,
                1.0 - (in_position.y / viewport_size.y) * 2.0
            );
            frag_uv = in_uv;
            frag_color = in_color;
            gl_Position = vec4(ndc, 0.0, 1.0);
        }
    )";

    static const char* kFragmentShader = R"(
        #version 330 core
        in vec2 frag_uv;
        in vec4 frag_color;
        uniform sampler2D scene_texture;
        out vec4 out_color;
        void main() {
            out_color = texture(scene_texture, frag_uv) * frag_color;
        }
    )";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, kVertexShader);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    program_ = glCreateProgram();
    glAttachShader(program_, vertexShader);
    glAttachShader(program_, fragmentShader);
    glLinkProgram(program_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linkStatus = GL_FALSE;
    glGetProgramiv(program_, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        destroy();
        return false;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, r)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    viewportLocation_ = glGetUniformLocation(program_, "viewport_size");
    samplerLocation_ = glGetUniformLocation(program_, "scene_texture");
    whiteTexture_ = ensureSolidWhiteTexture();
    floorTexture_ = ensureGeneratedFloorTexture();
    return program_ != 0 && vao_ != 0 && vbo_ != 0 && whiteTexture_.id != 0 && floorTexture_.id != 0;
}

void GlBattleSceneRenderer::destroy() {
    for (auto& [_, texture] : worldTextures_) {
        if (texture.id != 0) {
            glDeleteTextures(1, &texture.id);
        }
    }
    worldTextures_.clear();

    for (auto& [_, texture] : specialTextures_) {
        if (texture.id != 0) {
            glDeleteTextures(1, &texture.id);
        }
    }
    specialTextures_.clear();

    if (floorTexture_.id != 0) {
        glDeleteTextures(1, &floorTexture_.id);
        floorTexture_ = {};
    }
    if (whiteTexture_.id != 0) {
        glDeleteTextures(1, &whiteTexture_.id);
        whiteTexture_ = {};
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
    viewportLocation_ = -1;
    samplerLocation_ = -1;
}

bool GlBattleSceneRenderer::ensureWorldAssets(const std::vector<std::string>& assetNames) {
    bool ok = true;
    for (const std::string& assetName : assetNames) {
        const TextureInfo texture = ensureWorldTexture(assetName);
        if (texture.id == 0) {
            ok = false;
        }
    }

    (void)ensureSpecialTexture("miku:diandong", platform::path::resolvePath("assets/combat/presentations/miku/diandong.png"));
    (void)ensureSpecialTexture("lyooPlot:star", platform::path::resolvePath("assets/combat/presentations/lyooPlotTwist/star.png"));
    for (int i = 0; i <= 13; ++i) {
        (void)ensureSpecialTexture(makeLyooBossFrameKey(i), makeLyooBossFramePath(i));
        if (i <= 10) {
            (void)ensureSpecialTexture(makeLyooPlotFrameKey(i), makeLyooPlotFramePath(i));
        }
    }
    return ok;
}

GlBattleSceneRenderer::TextureInfo GlBattleSceneRenderer::ensureWorldTexture(const std::string& assetName) {
    if (const auto it = worldTextures_.find(assetName); it != worldTextures_.end()) {
        return it->second;
    }

    const std::string path = platform::path::resolvePath("assets/combat/sprites/" + assetName + ".png");
    TextureInfo texture = loadTextureFromPath(path);
    worldTextures_[assetName] = texture;
    return texture;
}

GlBattleSceneRenderer::TextureInfo GlBattleSceneRenderer::ensureSpecialTexture(const std::string& key,
                                                                               const std::string& resolvedPath) {
    if (const auto it = specialTextures_.find(key); it != specialTextures_.end()) {
        return it->second;
    }

    TextureInfo texture = loadTextureFromPath(resolvedPath);
    specialTextures_[key] = texture;
    return texture;
}

GlBattleSceneRenderer::TextureInfo GlBattleSceneRenderer::ensureGeneratedFloorTexture() {
    if (floorTexture_.id != 0) {
        return floorTexture_;
    }

    constexpr int texSize = 64;
    constexpr int cell = 16;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, texSize, texSize, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return {};
    }

    const Uint32 c0 = SDL_MapRGBA(surface->format, 46, 49, 60, 255);
    const Uint32 c1 = SDL_MapRGBA(surface->format, 52, 56, 69, 255);

    SDL_Rect rect{0, 0, cell, cell};
    for (int y = 0; y < texSize; y += cell) {
        for (int x = 0; x < texSize; x += cell) {
            rect.x = x;
            rect.y = y;
            const bool alt = ((x / cell) + (y / cell)) % 2 == 0;
            SDL_FillRect(surface, &rect, alt ? c0 : c1);
        }
    }

    floorTexture_ = makeTextureFromSurface(surface, GL_LINEAR);
    return floorTexture_;
}

GlBattleSceneRenderer::TextureInfo GlBattleSceneRenderer::ensureSolidWhiteTexture() {
    if (whiteTexture_.id != 0) {
        return whiteTexture_;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    if (surface == nullptr) {
        return {};
    }

    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 255, 255, 255, 255));
    whiteTexture_ = makeTextureFromSurface(surface, GL_NEAREST);
    return whiteTexture_;
}

void GlBattleSceneRenderer::drawTexturedTriangles(GLuint textureId,
                                                  const std::vector<Vertex>& vertices,
                                                  int screenWidth,
                                                  int screenHeight) {
    if (textureId == 0 || vertices.empty() || program_ == 0 || vao_ == 0 || vbo_ == 0) {
        return;
    }

    glUseProgram(program_);
    glUniform2f(viewportLocation_, static_cast<float>(screenWidth), static_cast<float>(screenHeight));
    glUniform1i(samplerLocation_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

void GlBattleSceneRenderer::renderFullscreenFade(int screenWidth,
                                                 int screenHeight,
                                                 SDL_Color color) {
    if (program_ == 0) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::vector<Vertex> vertices;
    vertices.reserve(6);
    appendFilledRect(vertices,
                     0.0f,
                     0.0f,
                     static_cast<float>(screenWidth),
                     static_cast<float>(screenHeight),
                     color);
    drawTexturedTriangles(ensureSolidWhiteTexture().id, vertices, screenWidth, screenHeight);
}

void GlBattleSceneRenderer::appendQuad(std::vector<Vertex>& vertices,
                                       float x0,
                                       float y0,
                                       float x1,
                                       float y1,
                                       float u0,
                                       float v0,
                                       float u1,
                                       float v1,
                                       SDL_Color color) const {
    const float r = static_cast<float>(color.r) / 255.0f;
    const float g = static_cast<float>(color.g) / 255.0f;
    const float b = static_cast<float>(color.b) / 255.0f;
    const float a = static_cast<float>(color.a) / 255.0f;
    vertices.push_back(Vertex{x0, y0, u0, v0, r, g, b, a});
    vertices.push_back(Vertex{x1, y0, u1, v0, r, g, b, a});
    vertices.push_back(Vertex{x1, y1, u1, v1, r, g, b, a});
    vertices.push_back(Vertex{x0, y0, u0, v0, r, g, b, a});
    vertices.push_back(Vertex{x1, y1, u1, v1, r, g, b, a});
    vertices.push_back(Vertex{x0, y1, u0, v1, r, g, b, a});
}

void GlBattleSceneRenderer::appendRotatedQuad(std::vector<Vertex>& vertices,
                                              float centerX,
                                              float centerY,
                                              float width,
                                              float height,
                                              float u0,
                                              float v0,
                                              float u1,
                                              float v1,
                                              SDL_Color color,
                                              float angleDegrees) const {
    const float radians = angleDegrees * (kPi / 180.0f);
    const float cosA = std::cos(radians);
    const float sinA = std::sin(radians);
    const float halfW = width * 0.5f;
    const float halfH = height * 0.5f;
    const std::array<SDL_FPoint, 4> local = {{
        {-halfW, -halfH},
        { halfW, -halfH},
        { halfW,  halfH},
        {-halfW,  halfH}
    }};
    std::array<SDL_FPoint, 4> transformed{};
    for (size_t i = 0; i < local.size(); ++i) {
        transformed[i].x = centerX + local[i].x * cosA - local[i].y * sinA;
        transformed[i].y = centerY + local[i].x * sinA + local[i].y * cosA;
    }

    const float r = static_cast<float>(color.r) / 255.0f;
    const float g = static_cast<float>(color.g) / 255.0f;
    const float b = static_cast<float>(color.b) / 255.0f;
    const float a = static_cast<float>(color.a) / 255.0f;
    vertices.push_back(Vertex{transformed[0].x, transformed[0].y, u0, v0, r, g, b, a});
    vertices.push_back(Vertex{transformed[1].x, transformed[1].y, u1, v0, r, g, b, a});
    vertices.push_back(Vertex{transformed[2].x, transformed[2].y, u1, v1, r, g, b, a});
    vertices.push_back(Vertex{transformed[0].x, transformed[0].y, u0, v0, r, g, b, a});
    vertices.push_back(Vertex{transformed[2].x, transformed[2].y, u1, v1, r, g, b, a});
    vertices.push_back(Vertex{transformed[3].x, transformed[3].y, u0, v1, r, g, b, a});
}

void GlBattleSceneRenderer::appendOutlineRect(std::vector<Vertex>& vertices,
                                              float x,
                                              float y,
                                              float width,
                                              float height,
                                              float thickness,
                                              SDL_Color color) const {
    appendFilledRect(vertices, x, y, width, thickness, color);
    appendFilledRect(vertices, x, y + height - thickness, width, thickness, color);
    appendFilledRect(vertices, x, y + thickness, thickness, height - thickness * 2.0f, color);
    appendFilledRect(vertices, x + width - thickness, y + thickness, thickness, height - thickness * 2.0f, color);
}

void GlBattleSceneRenderer::appendFilledRect(std::vector<Vertex>& vertices,
                                             float x,
                                             float y,
                                             float width,
                                             float height,
                                             SDL_Color color) const {
    appendQuad(vertices, x, y, x + width, y + height, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

void GlBattleSceneRenderer::renderWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                                        int screenWidth,
                                        int screenHeight) {
    if (program_ == 0) {
        return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (snapshot.renderFloor) {
        const float floorZ = 0.0f;
        const float centerX = -300.0f;
        const float centerY = 510.0f;
        const float tileSize = 200.0f;
        const int tilesX = 14;
        const int tilesY = 16;
        const float startX = centerX - (tilesX * tileSize * 0.5f);
        const float startY = centerY - (tilesY * tileSize * 0.30f);

        std::vector<Vertex> floorVertices;
        floorVertices.reserve(static_cast<size_t>(tilesX * tilesY * 6));
        for (int ty = 0; ty < tilesY; ++ty) {
            for (int tx = 0; tx < tilesX; ++tx) {
                const float x0 = startX + tx * tileSize;
                const float y0 = startY + ty * tileSize;
                const float x1 = x0 + tileSize;
                const float y1 = y0 + tileSize;

                const float d00 = snapshot.camera.getDepth(x0, y0, floorZ);
                const float d10 = snapshot.camera.getDepth(x1, y0, floorZ);
                const float d11 = snapshot.camera.getDepth(x1, y1, floorZ);
                const float d01 = snapshot.camera.getDepth(x0, y1, floorZ);
                if (d00 <= kFloorNearClipDepth && d10 <= kFloorNearClipDepth &&
                    d11 <= kFloorNearClipDepth && d01 <= kFloorNearClipDepth) {
                    continue;
                }

                const std::array<FloorVertex, 4> tile = {{
                    FloorVertex{x0, y0, d00, 0.0f, 0.0f},
                    FloorVertex{x1, y0, d10, 1.0f, 0.0f},
                    FloorVertex{x1, y1, d11, 1.0f, 1.0f},
                    FloorVertex{x0, y1, d01, 0.0f, 1.0f}
                }};
                std::array<FloorVertex, 6> clippedTile{};
                const int clippedCount = clipFloorQuadToNearPlane(tile, clippedTile);
                if (clippedCount < 3) {
                    continue;
                }

                std::array<SDL_FPoint, 6> projected{};
                float minX = 0.0f;
                float maxX = 0.0f;
                float minY = 0.0f;
                float maxY = 0.0f;
                for (int i = 0; i < clippedCount; ++i) {
                    projected[static_cast<size_t>(i)] =
                        snapshot.camera.worldToScreen(clippedTile[static_cast<size_t>(i)].worldX,
                                                      clippedTile[static_cast<size_t>(i)].worldY,
                                                      floorZ);
                    if (i == 0) {
                        minX = maxX = projected[static_cast<size_t>(i)].x;
                        minY = maxY = projected[static_cast<size_t>(i)].y;
                    } else {
                        minX = std::min(minX, projected[static_cast<size_t>(i)].x);
                        maxX = std::max(maxX, projected[static_cast<size_t>(i)].x);
                        minY = std::min(minY, projected[static_cast<size_t>(i)].y);
                        maxY = std::max(maxY, projected[static_cast<size_t>(i)].y);
                    }
                }
                if (maxX < -200.0f || minX > screenWidth + 200.0f || maxY < -200.0f || minY > screenHeight + 200.0f) {
                    continue;
                }

                for (int i = 1; i + 1 < clippedCount; ++i) {
                    const FloorVertex& a = clippedTile[0];
                    const FloorVertex& b = clippedTile[static_cast<size_t>(i)];
                    const FloorVertex& c = clippedTile[static_cast<size_t>(i + 1)];
                    const SDL_FPoint pa = projected[0];
                    const SDL_FPoint pb = projected[static_cast<size_t>(i)];
                    const SDL_FPoint pc = projected[static_cast<size_t>(i + 1)];
                    floorVertices.push_back(Vertex{pa.x, pa.y, a.u, a.v, 1.0f, 1.0f, 1.0f, 1.0f});
                    floorVertices.push_back(Vertex{pb.x, pb.y, b.u, b.v, 1.0f, 1.0f, 1.0f, 1.0f});
                    floorVertices.push_back(Vertex{pc.x, pc.y, c.u, c.v, 1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
        }
        drawTexturedTriangles(ensureGeneratedFloorTexture().id, floorVertices, screenWidth, screenHeight);
    }

    struct DrawCall {
        enum class Kind {
            Entity,
            AriBossPresentation
        };

        Kind kind = Kind::Entity;
        const battle::render::SceneEntity* entity = nullptr;
        size_t index = 0;
        float depth = 0.0f;
        SDL_FPoint screen{};
        battle::AriBossPresentation::NativeRenderState ariBossState{};
    };

    std::vector<DrawCall> drawList;
    drawList.reserve(snapshot.entities.size() + 1);
    for (size_t i = 0; i < snapshot.entities.size(); ++i) {
        const battle::render::SceneEntity& entity = snapshot.entities[i];
        if (!entity.visible || entity.spriteAlpha <= 0.001f) {
            continue;
        }
        drawList.push_back(DrawCall{
            DrawCall::Kind::Entity,
            &entity,
            i,
            snapshot.camera.getDepth(entity.worldX, entity.worldY, entity.worldZ),
            snapshot.camera.worldToScreen(entity.worldX, entity.worldY, entity.worldZ),
            {}
        });
    }

    if (const auto* ariBoss = dynamic_cast<const battle::AriBossPresentation*>(snapshot.activePresentation)) {
        const auto state = ariBoss->buildNativeRenderState();
        if (state.active) {
            const float ariWorldZ = state.casterZ + kAriBossSpriteWorldAnchorZ;
            drawList.push_back(DrawCall{
                DrawCall::Kind::AriBossPresentation,
                nullptr,
                snapshot.entities.size(),
                snapshot.camera.getDepth(state.casterX, state.casterY, ariWorldZ),
                snapshot.camera.worldToScreen(state.casterX, state.casterY, ariWorldZ),
                state
            });
        }
    }

    std::sort(drawList.begin(), drawList.end(), [](const DrawCall& lhs, const DrawCall& rhs) {
        return lhs.depth > rhs.depth;
    });

    for (const DrawCall& drawCall : drawList) {
        if (drawCall.kind == DrawCall::Kind::AriBossPresentation) {
            if (drawCall.depth <= 0.0f || drawCall.screen.x <= -500000.0f || drawCall.screen.y <= -500000.0f) {
                continue;
            }

            const auto& state = drawCall.ariBossState;
            const TextureInfo frame = ensureSpecialTexture(makeAriBossFrameKey(state.frameIndex),
                                                           makeAriBossFramePath(state.frameIndex));
            const float aspectRatio = static_cast<float>(std::max(1, frame.width != 0 ? frame.width : state.frameWidth)) /
                                      static_cast<float>(std::max(1, frame.height != 0 ? frame.height : state.frameHeight));
            const float drawHeight = std::clamp(static_cast<float>(screenHeight) * kAriBossSpriteTargetHeightRatio,
                                                kAriBossSpriteMinHeightPixels,
                                                kAriBossSpriteMaxHeightPixels);
            const float drawWidth = drawHeight * aspectRatio;

            std::vector<Vertex> vertices;
            vertices.reserve(6);
            appendQuad(vertices,
                       drawCall.screen.x - drawWidth * 0.5f,
                       drawCall.screen.y - drawHeight * 0.5f,
                       drawCall.screen.x + drawWidth * 0.5f,
                       drawCall.screen.y + drawHeight * 0.5f,
                       0.0f,
                       0.0f,
                       1.0f,
                       1.0f,
                       SDL_Color{255, 255, 255, 255});
            drawTexturedTriangles(frame.id != 0 ? frame.id : ensureSolidWhiteTexture().id,
                                  vertices,
                                  screenWidth,
                                  screenHeight);
            continue;
        }

        const battle::render::SceneEntity& entity = *drawCall.entity;
        const float scale = snapshot.camera.getPerspectiveScale(entity.worldX, entity.worldY, entity.worldZ);
        if (scale <= 0.0f) {
            continue;
        }
        const bool isFocused = static_cast<int>(drawCall.index) == snapshot.focusedEntityIndex;
        const float focusScale = isFocused ? 1.13f : 1.0f;
        const float shakeOffsetX = drawCall.index < snapshot.shakeOffsetsX.size()
            ? snapshot.shakeOffsetsX[drawCall.index]
            : 0.0f;
        const float alpha = std::clamp(entity.spriteAlpha, 0.0f, 1.0f);
        const float drawWidth = static_cast<float>(std::max(8, static_cast<int>(
            kSuggestedBaseSpriteWidth * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f))));
        const float drawHeight = static_cast<float>(std::max(8, static_cast<int>(
            kSuggestedBaseSpriteHeight * scale * focusScale * (entity.isBoss ? 1.28f : 1.0f))));
        const float x = drawCall.screen.x + shakeOffsetX - drawWidth * 0.5f;
        const float y = drawCall.screen.y + entity.spriteOffsetYPx - drawHeight;

        const TextureInfo texture = ensureWorldTexture(entity.assetName);
        if (texture.id != 0) {
            const int frameCount = texture.width / kSuggestedBaseSpriteWidth;
            const bool isAnimated = frameCount > 1 && (texture.width % kSuggestedBaseSpriteWidth == 0);
            float u0 = 0.0f;
            float u1 = 1.0f;
            if (isAnimated) {
                const int currentFrame = static_cast<int>(snapshot.frameAccumulator / kSpriteFrameTime) % frameCount;
                u0 = static_cast<float>(currentFrame * kSuggestedBaseSpriteWidth) / static_cast<float>(texture.width);
                u1 = static_cast<float>((currentFrame + 1) * kSuggestedBaseSpriteWidth) / static_cast<float>(texture.width);
            }
            std::vector<Vertex> spriteVertices;
            spriteVertices.reserve(6);
            appendQuad(spriteVertices,
                       x,
                       y,
                       x + drawWidth,
                       y + drawHeight,
                       u0,
                       0.0f,
                       u1,
                       1.0f,
                       SDL_Color{255, 255, 255, static_cast<Uint8>(alpha * 255.0f)});
            drawTexturedTriangles(texture.id, spriteVertices, screenWidth, screenHeight);
        } else {
            std::vector<Vertex> fallback;
            fallback.reserve(12);
            appendFilledRect(fallback, x, y, drawWidth, drawHeight,
                             SDL_Color{entity.fallbackColor.r, entity.fallbackColor.g, entity.fallbackColor.b,
                                       static_cast<Uint8>(alpha * 255.0f)});
            appendOutlineRect(fallback, x, y, drawWidth, drawHeight, 2.0f,
                              SDL_Color{16, 16, 20, static_cast<Uint8>(alpha * 255.0f)});
            drawTexturedTriangles(ensureSolidWhiteTexture().id, fallback, screenWidth, screenHeight);
        }

        if (isFocused) {
            std::vector<Vertex> focusVertices;
            focusVertices.reserve(24);
            appendOutlineRect(focusVertices,
                              x - 6.0f,
                              y - 6.0f,
                              drawWidth + 12.0f,
                              drawHeight + 12.0f,
                              2.0f,
                              SDL_Color{250, 230, 96, static_cast<Uint8>(alpha * 255.0f)});
            drawTexturedTriangles(ensureSolidWhiteTexture().id, focusVertices, screenWidth, screenHeight);
        }
    }
}

bool GlBattleSceneRenderer::rendersPresentationNatively(const battle::AbilityPresentation* presentation) const {
    return dynamic_cast<const battle::AriBossPresentation*>(presentation) != nullptr ||
           dynamic_cast<const battle::MikuDiandongPresentation*>(presentation) != nullptr ||
           dynamic_cast<const battle::LyooBossPresentation*>(presentation) != nullptr ||
           dynamic_cast<const battle::LyooPlotTwistPresentation*>(presentation) != nullptr;
}

bool GlBattleSceneRenderer::renderNativeBelowWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                                                   int screenWidth,
                                                   int screenHeight) {
    (void)snapshot;
    (void)screenWidth;
    (void)screenHeight;
    return false;
}

bool GlBattleSceneRenderer::renderNativeMidWorld(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                                                 int screenWidth,
                                                 int screenHeight) {
    if (dynamic_cast<const battle::AriBossPresentation*>(snapshot.activePresentation) != nullptr) {
        return true;
    }

    if (const auto* lyooBoss = dynamic_cast<const battle::LyooBossPresentation*>(snapshot.activePresentation)) {
        const auto state = lyooBoss->buildNativeState();
        if (state.phase == battle::LyooBossPresentation::NativePhase::IntroFrames ||
            state.phase == battle::LyooBossPresentation::NativePhase::AccelLoop ||
            state.phase == battle::LyooBossPresentation::NativePhase::HoldFrame13Ui) {
            const TextureInfo frame = ensureSpecialTexture(makeLyooBossFrameKey(state.uiFrameIndex),
                                                           makeLyooBossFramePath(state.uiFrameIndex));
            std::vector<Vertex> vertices;
            vertices.reserve(6);
            appendQuad(vertices, 0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight),
                       0.0f, 0.0f, 1.0f, 1.0f,
                       SDL_Color{255, 255, 255, 255});
            drawTexturedTriangles(frame.id != 0 ? frame.id : ensureSolidWhiteTexture().id, vertices, screenWidth, screenHeight);
            return true;
        }

        if (state.phase == battle::LyooBossPresentation::NativePhase::ZoomOut ||
            state.phase == battle::LyooBossPresentation::NativePhase::Complete) {
            const TextureInfo frame13 = ensureSpecialTexture(makeLyooBossFrameKey(13), makeLyooBossFramePath(13));
            float aspect = 1.0f;
            if (frame13.id != 0 && frame13.height > 0) {
                aspect = static_cast<float>(std::max(1, frame13.width)) / static_cast<float>(std::max(1, frame13.height));
            }

            if (snapshot.camera.getDepth(state.casterX, state.casterY, state.casterZ) > 1.0f) {
                const SDL_FPoint center = snapshot.camera.worldToScreen(state.casterX, state.casterY, state.casterZ);
                const float scale = snapshot.camera.getPerspectiveScale(state.casterX, state.casterY, state.casterZ);
                const float drawHeight = std::max(24.0f, state.worldSpriteHeightUnits * scale);
                const float drawWidth = drawHeight * aspect;
                std::vector<Vertex> bossVertices;
                bossVertices.reserve(6);
                appendQuad(bossVertices,
                           center.x - drawWidth * 0.5f,
                           center.y - drawHeight,
                           center.x + drawWidth * 0.5f,
                           center.y,
                           0.0f,
                           0.0f,
                           1.0f,
                           1.0f,
                           SDL_Color{255, 255, 255, 255});
                drawTexturedTriangles(frame13.id != 0 ? frame13.id : ensureSolidWhiteTexture().id,
                                      bossVertices,
                                      screenWidth,
                                      screenHeight);
            }

            std::vector<Vertex> pulseVertices;
            pulseVertices.reserve(state.pulses.size() * 12);
            for (const auto& pulse : state.pulses) {
                if (!pulse.active) {
                    continue;
                }

                const float t = std::clamp(pulse.elapsed / std::max(0.0001f, pulse.duration), 0.0f, 1.0f);
                const float eased = easeOutCubic(t);
                const float x = lerpF(pulse.startX, pulse.targetX, eased);
                const float y = lerpF(pulse.startY, pulse.targetY, eased);
                const float arc = std::sin(t * kPi) * 130.0f;
                const float z = lerpF(pulse.startZ, pulse.targetZ, eased) - arc;
                if (snapshot.camera.getDepth(x, y, z) <= 1.0f) {
                    continue;
                }

                const SDL_FPoint screen = snapshot.camera.worldToScreen(x, y, z);
                const float size = std::max(12.0f, snapshot.camera.getPerspectiveScale(x, y, z) * 28.0f);
                const float alpha = (t > 0.88f) ? (1.0f - (t - 0.88f) / 0.12f) : 1.0f;
                appendFilledRect(pulseVertices,
                                 screen.x - size * 0.7f,
                                 screen.y - size * 0.7f,
                                 size * 1.4f,
                                 size * 1.4f,
                                 SDL_Color{255, 70, 70, static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 90.0f)});
                appendFilledRect(pulseVertices,
                                 screen.x - size * 0.28f,
                                 screen.y - size * 0.28f,
                                 size * 0.56f,
                                 size * 0.56f,
                                 SDL_Color{255, 35, 35, static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)});
                appendOutlineRect(pulseVertices,
                                  screen.x - size * 0.32f,
                                  screen.y - size * 0.32f,
                                  size * 0.64f,
                                  size * 0.64f,
                                  std::max(2.0f, size * 0.08f),
                                  SDL_Color{255, 240, 240, static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)});
            }
            drawTexturedTriangles(ensureSolidWhiteTexture().id, pulseVertices, screenWidth, screenHeight);
            return true;
        }
    }

    if (const auto* lyooPlot = dynamic_cast<const battle::LyooPlotTwistPresentation*>(snapshot.activePresentation)) {
        const auto state = lyooPlot->buildNativeState();
        const TextureInfo starTexture = ensureSpecialTexture("lyooPlot:star",
                                                             platform::path::resolvePath("assets/combat/presentations/lyooPlotTwist/star.png"));
        auto drawStar = [&](std::vector<Vertex>& vertices,
                            float centerX,
                            float centerY,
                            float size,
                            float angleDegrees,
                            Uint8 alpha) {
            if (starTexture.id != 0) {
                appendRotatedQuad(vertices,
                                  centerX,
                                  centerY,
                                  size,
                                  size,
                                  0.0f,
                                  0.0f,
                                  1.0f,
                                  1.0f,
                                  SDL_Color{255, 255, 255, alpha},
                                  angleDegrees);
                return;
            }

            appendFilledRect(vertices,
                             centerX - size * 0.08f,
                             centerY - size * 0.5f,
                             size * 0.16f,
                             size,
                             SDL_Color{255, 255, 255, alpha});
            appendFilledRect(vertices,
                             centerX - size * 0.5f,
                             centerY - size * 0.08f,
                             size,
                             size * 0.16f,
                             SDL_Color{255, 255, 255, alpha});
        };

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::IntroFrames) {
            const TextureInfo frame = ensureSpecialTexture(makeLyooPlotFrameKey(std::clamp(state.introFrameIndex, 0, 9)),
                                                           makeLyooPlotFramePath(std::clamp(state.introFrameIndex, 0, 9)));
            std::vector<Vertex> vertices;
            vertices.reserve(6);
            appendQuad(vertices, 0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight),
                       0.0f, 0.0f, 1.0f, 1.0f, SDL_Color{255, 255, 255, 255});
            drawTexturedTriangles(frame.id != 0 ? frame.id : ensureSolidWhiteTexture().id, vertices, screenWidth, screenHeight);
            return true;
        }

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::TransitionToVoid) {
            const float localT = std::clamp((state.elapsedTime - kLyooPlotTransitionStartSeconds) /
                                            kLyooPlotTransitionDurationSeconds, 0.0f, 1.0f);
            const float blackAlpha = lerpF(0.0f, 255.0f, easeInCubic(localT));
            const TextureInfo frame = ensureSpecialTexture(makeLyooPlotFrameKey(localT < 0.28f ? 9 : 10),
                                                           makeLyooPlotFramePath(localT < 0.28f ? 9 : 10));
            const float zoomT = localT < 0.28f ? 0.0f : std::clamp((localT - 0.28f) / 0.72f, 0.0f, 1.0f);
            const float sizeT = std::pow(std::max(0.0f, 1.0f - zoomT), 3.4f);
            const float height = std::max(1.0f, static_cast<float>(screenHeight) * 0.76f * sizeT);
            const float width = height * (320.0f / 170.0f);

            std::vector<Vertex> blackVertices;
            blackVertices.reserve(12);
            appendFilledRect(blackVertices, 0.0f, 0.0f,
                             static_cast<float>(screenWidth), static_cast<float>(screenHeight),
                             SDL_Color{0, 0, 0, static_cast<Uint8>(blackAlpha)});
            appendQuad(blackVertices,
                       static_cast<float>(screenWidth) * 0.5f - width * 0.5f,
                       static_cast<float>(screenHeight) * kLyooPlotTransitionScreenCenterY - height * 0.5f,
                       static_cast<float>(screenWidth) * 0.5f + width * 0.5f,
                       static_cast<float>(screenHeight) * kLyooPlotTransitionScreenCenterY + height * 0.5f,
                       0.0f,
                       0.0f,
                       1.0f,
                       1.0f,
                       SDL_Color{255, 255, 255, static_cast<Uint8>(255.0f * (1.0f - zoomT * 0.2f))});
            drawTexturedTriangles(frame.id != 0 ? frame.id : ensureSolidWhiteTexture().id, blackVertices, screenWidth, screenHeight);
            return true;
        }

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::RingStars ||
            state.phase == battle::LyooPlotTwistPresentation::NativePhase::SkyStars ||
            state.phase == battle::LyooPlotTwistPresentation::NativePhase::ApproachAllies) {
            std::vector<Vertex> vertices;
            vertices.reserve(512);

            if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::RingStars ||
                state.phase == battle::LyooPlotTwistPresentation::NativePhase::SkyStars) {
                const float ringPhaseT = std::clamp((state.elapsedTime - kLyooPlotRingStartSeconds) /
                                                   kLyooPlotRingDurationSeconds, 0.0f, 1.0f);
                const float centerX = static_cast<float>(screenWidth) * kLyooPlotRingScreenCenterX;
                const float centerY = lerpF(
                    static_cast<float>(screenHeight) * kLyooPlotRingScreenCenterY,
                    static_cast<float>(screenHeight) * 1.10f,
                    easeInOutCubic(std::clamp((ringPhaseT - 0.34f) / 0.50f, 0.0f, 1.0f))
                );
                const float phaseTime = state.elapsedTime - kLyooPlotRingStartSeconds;
                const float baseRadius = kLyooPlotRingOrbitRadiusPx *
                    lerpF(0.95f, 1.04f, std::sin(phaseTime * 1.7f) * 0.5f + 0.5f);
                for (const auto& star : state.ringStars) {
                    const float localTime = phaseTime - star.spawnTime;
                    if (localTime < 0.0f || localTime > star.lifetime) {
                        continue;
                    }
                    const float progress = std::clamp(localTime / std::max(0.001f, star.lifetime), 0.0f, 1.0f);
                    const float scale = lyooPlot->computeNativeStarScale(progress);
                    if (scale <= 0.001f) {
                        continue;
                    }
                    const float angleRadians = star.orbitAngleDegrees * (kPi / 180.0f);
                    const float starX = centerX + std::cos(angleRadians) * baseRadius;
                    const float starY = centerY + std::sin(angleRadians) * baseRadius * 0.72f;
                    const float size = std::max(1.0f, star.baseSize * scale);
                    drawStar(vertices, starX, starY, size, localTime * star.rotationSpeed,
                             static_cast<Uint8>(255.0f * std::min(1.0f, scale)));
                }
            }

            if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::SkyStars ||
                state.phase == battle::LyooPlotTwistPresentation::NativePhase::ApproachAllies) {
                const float phaseTime = state.elapsedTime - kLyooPlotSkyStartSeconds;
                for (const auto& star : state.skyStars) {
                    const float localTime = phaseTime - star.spawnTime;
                    if (localTime < 0.0f || localTime > star.lifetime) {
                        continue;
                    }
                    const float progress = std::clamp(localTime / std::max(0.001f, star.lifetime), 0.0f, 1.0f);
                    const float scale = lyooPlot->computeNativeStarScale(progress);
                    if (scale <= 0.001f) {
                        continue;
                    }
                    const float size = std::max(1.0f, star.baseSize * scale);
                    drawStar(vertices,
                             star.normalizedX * static_cast<float>(screenWidth),
                             star.normalizedY * static_cast<float>(screenHeight),
                             size,
                             localTime * star.rotationSpeed,
                             static_cast<Uint8>(255.0f * std::min(1.0f, scale)));
                }
            }

            drawTexturedTriangles(starTexture.id != 0 ? starTexture.id : ensureSolidWhiteTexture().id,
                                  vertices,
                                  screenWidth,
                                  screenHeight);

            if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::ApproachAllies) {
                std::vector<Vertex> overlay;
                overlay.reserve(6);
                const float localT = std::clamp((state.elapsedTime - kLyooPlotApproachStartSeconds) /
                                                kLyooPlotApproachDurationSeconds, 0.0f, 1.0f);
                appendFilledRect(overlay,
                                 0.0f,
                                 0.0f,
                                 static_cast<float>(screenWidth),
                                 static_cast<float>(screenHeight),
                                 SDL_Color{0, 0, 0, static_cast<Uint8>(std::lround(lerpF(kLyooPlotApproachDarknessAlpha, 25.0f, localT)))});
                drawTexturedTriangles(ensureSolidWhiteTexture().id, overlay, screenWidth, screenHeight);
            }

            return true;
        }

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::FinalShake) {
            return true;
        }
    }

    return false;
}

bool GlBattleSceneRenderer::renderNativeAboveHud(const battle::BattleSessionCore::BattleFrameSnapshot& snapshot,
                                                 int screenWidth,
                                                 int screenHeight) {
    if (const auto* miku = dynamic_cast<const battle::MikuDiandongPresentation*>(snapshot.activePresentation)) {
        const auto state = miku->buildNativeState();
        if (state.phase == battle::MikuDiandongPresentation::NativePhase::Complete) {
            return false;
        }

        const TextureInfo diandong = ensureSpecialTexture("miku:diandong",
            platform::path::resolvePath("assets/combat/presentations/miku/diandong.png"));
        auto findBossEntity = [&]() -> const battle::render::SceneEntity* {
            for (const auto& entity : snapshot.entities) {
                if (entity.isBoss) {
                    return &entity;
                }
            }
            return nullptr;
        };

        auto drawWorldSprite = [&](const battle::Camera3D& camera,
                                   const TextureInfo& texture,
                                   float worldX,
                                   float worldY,
                                   float worldZ,
                                   float baseHeight,
                                   float widthScale,
                                   SDL_Color color) {
            if (camera.getDepth(worldX, worldY, worldZ) <= 1.0f) {
                return;
            }
            const SDL_FPoint screen = camera.worldToScreen(worldX, worldY, worldZ);
            const float perspectiveScale = camera.getPerspectiveScale(worldX, worldY, worldZ);
            const float height = std::max(4.0f, baseHeight * perspectiveScale);
            float width = height * widthScale;
            if (texture.id != 0 && texture.height > 0) {
                width = std::max(4.0f, height * (static_cast<float>(texture.width) / static_cast<float>(texture.height)) * widthScale);
            }
            std::vector<Vertex> vertices;
            vertices.reserve(6);
            appendQuad(vertices,
                       screen.x - width * 0.5f,
                       screen.y - height,
                       screen.x + width * 0.5f,
                       screen.y,
                       0.0f,
                       0.0f,
                       1.0f,
                       1.0f,
                       color);
            drawTexturedTriangles(texture.id != 0 ? texture.id : ensureSolidWhiteTexture().id, vertices, screenWidth, screenHeight);
        };

        if (state.phase == battle::MikuDiandongPresentation::NativePhase::ImpactFreeze) {
            std::vector<Vertex> overlay;
            overlay.reserve(6);
            appendFilledRect(overlay, 0.0f, 0.0f, static_cast<float>(screenWidth), static_cast<float>(screenHeight),
                             SDL_Color{255, 255, 255, 255});
            drawTexturedTriangles(ensureSolidWhiteTexture().id, overlay, screenWidth, screenHeight);
            drawWorldSprite(snapshot.camera, diandong,
                            state.mikuWorldX, state.mikuWorldY, state.mikuWorldZ,
                            230.0f * 1.05f, 1.4f, SDL_Color{0, 0, 0, 255});
            if (const auto* bossEntity = findBossEntity()) {
                drawWorldSprite(snapshot.camera,
                                ensureWorldTexture(bossEntity->assetName),
                                state.targetWorldX,
                                state.targetWorldY,
                                state.targetWorldZ,
                                320.0f,
                                0.65f,
                                SDL_Color{0, 0, 0, 255});
            }
            return true;
        }

        drawWorldSprite(snapshot.camera, diandong,
                        state.mikuWorldX, state.mikuWorldY, state.mikuWorldZ,
                        230.0f, 1.4f, SDL_Color{255, 255, 255, 255});
        return true;
    }

    if (const auto* lyooPlot = dynamic_cast<const battle::LyooPlotTwistPresentation*>(snapshot.activePresentation)) {
        const auto state = lyooPlot->buildNativeState();
        const TextureInfo starTexture = ensureSpecialTexture("lyooPlot:star",
                                                             platform::path::resolvePath("assets/combat/presentations/lyooPlotTwist/star.png"));
        auto drawStar = [&](std::vector<Vertex>& vertices,
                            float centerX,
                            float centerY,
                            float size,
                            float angleDegrees,
                            Uint8 alpha) {
            appendRotatedQuad(vertices,
                              centerX,
                              centerY,
                              size,
                              size,
                              0.0f,
                              0.0f,
                              1.0f,
                              1.0f,
                              SDL_Color{255, 255, 255, alpha},
                              angleDegrees);
        };

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::MassiveStar) {
            std::vector<Vertex> vertices;
            vertices.reserve(1024);
            const float phaseTime = state.elapsedTime - kLyooPlotMassiveStarStartSeconds;
            for (const auto& star : state.scatterStars) {
                const float localTime = phaseTime - star.spawnTime;
                if (localTime < 0.0f || localTime > star.lifetime) {
                    continue;
                }

                const float progress = std::clamp(localTime / std::max(0.001f, star.lifetime), 0.0f, 1.0f);
                const float moveT = easeInOutCubic(progress);
                const float x = lerpF(star.normalizedX, star.endNormalizedX, moveT) * static_cast<float>(screenWidth);
                const float y = lerpF(star.normalizedY, star.endNormalizedY, moveT) * static_cast<float>(screenHeight);
                const float sizeGrowth = lerpF(0.05f, 1.95f, easeOutCubic(progress));
                const float size = std::max(1.0f, star.baseSize * sizeGrowth);
                const float fadeOut = progress < 0.82f ? 1.0f : (1.0f - (progress - 0.82f) / 0.18f);
                drawStar(vertices, x, y, size, localTime * star.rotationSpeed,
                         static_cast<Uint8>(220.0f * std::clamp(fadeOut, 0.0f, 1.0f)));
            }

            const float localT = std::clamp((state.elapsedTime - kLyooPlotMassiveStarStartSeconds) /
                                            kLyooPlotMassiveStarDurationSeconds, 0.0f, 1.0f);
            const float giantStartT = std::clamp((localT - 0.40f) / 0.60f, 0.0f, 1.0f);
            if (giantStartT > 0.0f) {
                const float size = lerpF(1.0f, static_cast<float>(std::max(screenWidth, screenHeight)) * 3.3f,
                                         easeInQuint(giantStartT));
                const float alphaT = giantStartT < 0.74f ? easeInCubic(giantStartT / 0.74f) : 1.0f;
                drawStar(vertices,
                         static_cast<float>(screenWidth) * 0.5f,
                         static_cast<float>(screenHeight) * 0.5f,
                         size,
                         -28.0f + giantStartT * 260.0f,
                         static_cast<Uint8>(lerpF(24.0f, 255.0f, alphaT)));
            }

            drawTexturedTriangles(starTexture.id != 0 ? starTexture.id : ensureSolidWhiteTexture().id,
                                  vertices,
                                  screenWidth,
                                  screenHeight);
            return true;
        }

        if (state.phase == battle::LyooPlotTwistPresentation::NativePhase::FullBlackout) {
            std::vector<Vertex> vertices;
            vertices.reserve(6);
            appendFilledRect(vertices, 0.0f, 0.0f,
                             static_cast<float>(screenWidth), static_cast<float>(screenHeight),
                             SDL_Color{0, 0, 0, 255});
            drawTexturedTriangles(ensureSolidWhiteTexture().id, vertices, screenWidth, screenHeight);
            return true;
        }
    }

    return false;
}

} // namespace battle::render
