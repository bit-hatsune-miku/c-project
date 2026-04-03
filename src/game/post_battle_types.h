#pragma once

#include <string>
#include <vector>

namespace battle::postbattle {

struct CharacterAnalytics {
    std::string key;
    std::string title;
    int totalDamage = 0;
};

struct Summary {
    float totalActionValueConsumed = 0.0f;
    std::vector<CharacterAnalytics> characters;

    int totalDamage() const {
        int total = 0;
        for (const CharacterAnalytics& character : characters) {
            total += character.totalDamage;
        }
        return total;
    }
};

}  // namespace battle::postbattle
