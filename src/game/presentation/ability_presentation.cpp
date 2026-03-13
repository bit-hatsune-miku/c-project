#include "ability_presentation.h"
#include <unordered_map>

namespace battle {

PresentationRegistry& PresentationRegistry::instance() {
    static PresentationRegistry registry;
    return registry;
}

void PresentationRegistry::registerPresentation(const std::string& id, PresentationFactory factory) {
    factories_[id] = factory;
}

std::unique_ptr<AbilityPresentation> PresentationRegistry::create(
    const std::string& id,
    float casterWorldX, float casterWorldY, float casterWorldZ,
    float targetWorldX, float targetWorldY, float targetWorldZ
) {
    auto it = factories_.find(id);
    if (it != factories_.end()) {
        return it->second(casterWorldX, casterWorldY, casterWorldZ,
                         targetWorldX, targetWorldY, targetWorldZ);
    }
    return nullptr;
}

} // namespace battle
