#ifndef PRESENTATION_TUNING_PROFILE_H
#define PRESENTATION_TUNING_PROFILE_H

#include <string>
#include <unordered_map>

namespace battle {

struct PresentationTuningProfile {
    std::string profileId;
    int phaseIndex = 0;
    std::unordered_map<std::string, int> intParams;
    std::unordered_map<std::string, float> floatParams;
};

} // namespace battle

#endif // PRESENTATION_TUNING_PROFILE_H
