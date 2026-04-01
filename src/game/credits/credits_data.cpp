#include "credits_data.h"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace game::credits {
namespace {

using json = nlohmann::json;

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
            block.kicker = blockJson.value("kicker", "");
            block.heading = blockJson.value("heading", "");
            block.layout = blockJson.value("layout", "center");
            block.image = blockJson.value("image", "");
            block.imageLabel = blockJson.value("imageLabel", "");
            block.imageCaption = blockJson.value("imageCaption", "");

            const auto entriesIt = blockJson.find("entries");
            if (entriesIt != blockJson.end() && entriesIt->is_array()) {
                block.entries.reserve(entriesIt->size());
                for (const auto& entryJson : *entriesIt) {
                    if (entryJson.is_string()) {
                        block.entries.push_back(entryJson.get<std::string>());
                    }
                }
            }

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
