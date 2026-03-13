#include "easing.h"

#include <algorithm>

namespace battle::easing {

float clamp01(float t) {
    return std::clamp(t, 0.0f, 1.0f);
}

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float easeOutCubic(float t) {
    const float x = 1.0f - clamp01(t);
    return 1.0f - x * x * x;
}

float easeOutBounce(float t) {
    const float x = clamp01(t);
    constexpr float n1 = 7.5625f;
    constexpr float d1 = 2.75f;

    if (x < 1.0f / d1) {
        return n1 * x * x;
    }
    if (x < 2.0f / d1) {
        const float y = x - 1.5f / d1;
        return n1 * y * y + 0.75f;
    }
    if (x < 2.5f / d1) {
        const float y = x - 2.25f / d1;
        return n1 * y * y + 0.9375f;
    }

    const float y = x - 2.625f / d1;
    return n1 * y * y + 0.984375f;
}

float easeOutBack(float t) {
    const float x = clamp01(t);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float p = x - 1.0f;
    return 1.0f + c3 * p * p * p + c1 * p * p;
}

} // namespace battle::easing
