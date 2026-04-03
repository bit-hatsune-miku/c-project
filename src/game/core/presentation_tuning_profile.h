#ifndef PRESENTATION_TUNING_PROFILE_H
#define PRESENTATION_TUNING_PROFILE_H

#include <string>
#include <unordered_map>

/**
 * @brief Holds configurable presentation tuning parameters for a battle profile.
 *
 * Stores the profile identifier, a zero-based phase index, and named maps of
 * integer and floating-point tuning parameters used to adjust presentation
 * behavior.
 *
 * @var profileId Identifier for the tuning profile.
 * @var phaseIndex Phase index (zero-based) for which these parameters apply.
 * @var intParams Map of integer tuning parameters keyed by parameter name.
 * @var floatParams Map of floating-point tuning parameters keyed by parameter name.
 */
namespace battle {

struct PresentationTuningProfile {
    std::string profileId;
    int phaseIndex = 0;
    std::unordered_map<std::string, int> intParams;
    std::unordered_map<std::string, float> floatParams;
};

} // namespace battle

#endif // PRESENTATION_TUNING_PROFILE_H
