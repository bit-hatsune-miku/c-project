#include "credits_data.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iostream>

#include <nlohmann/json.hpp>

namespace game::credits {
namespace {

using json = nlohmann::json;

std::string normalizedToken(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string jsonStringValue(const json& object,
                            std::initializer_list<const char*> keys,
                            const std::string& fallback = std::string{}) {
    for (const char* key : keys) {
        const auto it = object.find(key);
        if (it != object.end() && it->is_string()) {
            return it->get<std::string>();
        }
    }

    return fallback;
}

const json* jsonObjectValue(const json& object, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
        const auto it = object.find(key);
        if (it != object.end() && it->is_object()) {
            return &(*it);
        }
    }

    return nullptr;
}

void appendStringArrayValues(const json& object,
                             std::initializer_list<const char*> keys,
                             std::vector<std::string>& outValues) {
    for (const char* key : keys) {
        const auto it = object.find(key);
        if (it == object.end() || !it->is_array()) {
            continue;
        }

        outValues.reserve(outValues.size() + it->size());
        for (const auto& entryJson : *it) {
            if (entryJson.is_string()) {
                outValues.push_back(entryJson.get<std::string>());
            }
        }
        return;
    }
}

std::string normalizeLayout(const std::string& requestedLayout, bool hasVisualContent) {
    const std::string layout = normalizedToken(requestedLayout);
    if (layout.empty()) {
        return hasVisualContent ? "split" : "center";
    }

    if (layout == "split" || layout == "feature" || layout == "visual" || layout == "image") {
        return "split";
    }

    return "center";
}

std::string normalizeTextAlign(const std::string& requestedAlign) {
    const std::string align = normalizedToken(requestedAlign);
    if (align == "left" || align == "start") {
        return "left";
    }

    return "center";
}

}  // namespace

bool loadCreditsData(const std::string& jsonPath, CreditsData& outData) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        std::cerr << "[Credits] Failed to open: " << jsonPath << "\n";
        return false;
    }

    json root;
    try {
        file >> root;
    } catch (const json::exception& exception) {
        std::cerr << "[Credits] JSON parse error: " << exception.what() << "\n";
        return false;
    }

    try {
        outData = CreditsData{};
        outData.kicker = root.value("kicker", "FINAL SIGNAL / STAFF ROLL");
        outData.title = root.value("title", "CREDITS");
        outData.subtitle = root.value("subtitle", "");
        outData.groupImage = root.value("groupImage", "");
        outData.groupPlaceholderTitle = root.value("groupPlaceholderTitle", "GROUP ILLUSTRATION PENDING");
        outData.groupPlaceholderCopy = root.value(
            "groupPlaceholderCopy",
            "Miku centered, full cast grouped around her. Slot reserved for Lyes' final art once the roster is complete.");
        outData.groupImageCaption = root.value("groupImageCaption", "");
        outData.returnPrompt = root.value("returnPrompt", "PRESS ANY KEY OR CLICK TO RETURN TO MAIN MENU");

        const auto blocksIt = root.find("blocks");
        if (blocksIt == root.end() || !blocksIt->is_array()) {
            std::cerr << "[Credits] Missing blocks array in " << jsonPath << "\n";
            return false;
        }

        outData.blocks.clear();
        outData.blocks.reserve(blocksIt->size());

        for (const auto& blockJson : *blocksIt) {
            CreditsBlock block;
            block.kicker = jsonStringValue(blockJson, {"kicker", "eyebrow", "label"}, "");
            block.heading = jsonStringValue(blockJson, {"heading", "title", "sectionTitle"}, "");
            block.textAlign = jsonStringValue(blockJson, {"textAlign", "copyAlign", "align"}, "");
            block.image = jsonStringValue(blockJson, {"image", "imagePath"}, "");
            block.imageLabel = jsonStringValue(blockJson, {"imageLabel", "visualLabel", "imageTitle"}, "");
            block.imageCaption = jsonStringValue(blockJson, {"imageCaption", "visualCaption", "caption"}, "");

            if (const json* copyJson = jsonObjectValue(blockJson, {"copy", "text"})) {
                if (block.heading.empty()) {
                    block.heading = jsonStringValue(*copyJson, {"heading", "title"}, "");
                }
                if (block.textAlign.empty()) {
                    block.textAlign = jsonStringValue(*copyJson, {"align", "textAlign"}, "");
                }
                appendStringArrayValues(*copyJson, {"entries", "names", "lines"}, block.entries);
            }

            if (const json* visualJson = jsonObjectValue(blockJson, {"visual"})) {
                if (block.image.empty()) {
                    block.image = jsonStringValue(*visualJson, {"image", "src", "path"}, "");
                }
                if (block.imageLabel.empty()) {
                    block.imageLabel = jsonStringValue(*visualJson, {"label", "title"}, "");
                }
                if (block.imageCaption.empty()) {
                    block.imageCaption = jsonStringValue(*visualJson, {"caption", "copy"}, "");
                }
            }

            if (block.entries.empty()) {
                appendStringArrayValues(blockJson, {"entries", "names", "credits"}, block.entries);
            }

            const bool hasVisualContent =
                !block.image.empty() || !block.imageLabel.empty() || !block.imageCaption.empty();
            block.layout = normalizeLayout(jsonStringValue(blockJson, {"layout", "style", "variant"}, ""),
                                           hasVisualContent);
            block.textAlign = normalizeTextAlign(block.textAlign);

            if (!block.heading.empty()) {
                outData.blocks.push_back(std::move(block));
            }
        }

        return !outData.blocks.empty();
    } catch (const json::exception& exception) {
        std::cerr << "[Credits] JSON structure error: " << exception.what() << "\n";
        return false;
    }
}

}  // namespace game::credits
