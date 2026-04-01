#pragma once

#include <string>
#include <vector>

namespace game::credits {

struct CreditsBlock {
    std::string kicker;
    std::string heading;
    std::string layout;
    std::string image;
    std::string imageLabel;
    std::string imageCaption;
    std::vector<std::string> entries;
};

struct CreditsData {
    std::string kicker;
    std::string title;
    std::string subtitle;
    std::string groupImage;
    std::string groupPlaceholderTitle;
    std::string groupPlaceholderCopy;
    std::string groupImageCaption;
    std::string returnPrompt;
    std::vector<CreditsBlock> blocks;
};

bool loadCreditsData(const std::string& jsonPath, CreditsData& outData);

}  // namespace game::credits
