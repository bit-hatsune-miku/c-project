#ifndef EASING_H
#define EASING_H

namespace battle::easing {

float clamp01(float t);
float lerp(float a, float b, float t);
float easeInCubic(float t);
float easeInQuint(float t);
float easeOutQuint(float t);
float easeOutCubic(float t);
float easeOutBounce(float t);
float easeOutBack(float t);

} // namespace battle::easing

#endif // EASING_H
