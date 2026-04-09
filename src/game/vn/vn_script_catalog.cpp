#include "vn_script_catalog.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>

#include <nlohmann/json.hpp>

#include "../../platform/path_resolution.h"

namespace vn {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

constexpr const char* kScriptCatalogRelativePath = "assets/vn/scripts/catalog.json";
constexpr const char* kCreditsDataRelativePath = "assets/vn/credits/credits.json";

struct ScriptCatalogState {
    std::vector<ScriptCatalogEntry> entries;
    std::unordered_map<std::string, std::size_t> byId;
    std::unordered_map<std::string, std::size_t> byLegacyAlias;
};

std::string stripJsonSuffix(std::string value) {
    if (value.size() >= 5 && value.substr(value.size() - 5) == ".json") {
        value.resize(value.size() - 5);
    }
    return value;
}

std::string scriptLookupToken(const std::string& scriptRef) {
    if (scriptRef.empty()) {
        return std::string();
    }

    const bool hasDirectorySeparators =
        scriptRef.find('/') != std::string::npos || scriptRef.find('\\') != std::string::npos;
    const std::string candidate = hasDirectorySeparators
        ? fs::path(scriptRef).filename().string()
        : scriptRef;
    return stripJsonSuffix(candidate);
}

const ScriptCatalogState& catalogState() {
    static const ScriptCatalogState state = []() {
        ScriptCatalogState loadedState;

        const std::string catalogPath = platform::path::resolvePath(kScriptCatalogRelativePath);
        std::ifstream file(catalogPath);
        if (!file.is_open()) {
            std::cerr << "[VN Script Catalog] Failed to open: " << catalogPath << "\n";
            return loadedState;
        }

        json root;
        try {
            file >> root;
        } catch (const json::exception& exception) {
            std::cerr << "[VN Script Catalog] JSON parse error: " << exception.what() << "\n";
            return loadedState;
        }

        const auto scriptsIt = root.find("scripts");
        if (scriptsIt == root.end() || !scriptsIt->is_array()) {
            std::cerr << "[VN Script Catalog] Missing scripts array in " << catalogPath << "\n";
            return loadedState;
        }

        loadedState.entries.reserve(scriptsIt->size());
        for (const auto& entryJson : *scriptsIt) {
            if (!entryJson.is_object()) {
                continue;
            }

            ScriptCatalogEntry entry;
            entry.scriptId = entryJson.value("id", "");
            entry.relativePath = entryJson.value("path", "");
            entry.category = entryJson.value("category", "");
            entry.legacyAliases = entryJson.value("legacyAliases", std::vector<std::string>{});
            if (entry.scriptId.empty() || entry.relativePath.empty()) {
                continue;
            }

            const std::size_t index = loadedState.entries.size();
            loadedState.entries.push_back(entry);
            loadedState.byId.emplace(entry.scriptId, index);
            for (const std::string& alias : entry.legacyAliases) {
                if (!alias.empty()) {
                    loadedState.byLegacyAlias.emplace(alias, index);
                }
            }
        }

        return loadedState;
    }();

    return state;
}

const ScriptCatalogEntry* findEntryByCanonicalId(const std::string& scriptRef) {
    const std::string token = scriptLookupToken(scriptRef);
    if (token.empty()) {
        return nullptr;
    }

    const auto& state = catalogState();
    const auto it = state.byId.find(token);
    if (it == state.byId.end()) {
        return nullptr;
    }
    return &state.entries[it->second];
}

const ScriptCatalogEntry* findEntryByLegacyAlias(const std::string& scriptRef) {
    const std::string token = scriptLookupToken(scriptRef);
    if (token.empty()) {
        return nullptr;
    }

    const auto& state = catalogState();
    const auto it = state.byLegacyAlias.find(token);
    if (it == state.byLegacyAlias.end()) {
        return nullptr;
    }
    return &state.entries[it->second];
}

std::string resolvedPathForEntry(const ScriptCatalogEntry* entry) {
    if (entry == nullptr || entry->relativePath.empty()) {
        return std::string();
    }
    return platform::path::resolvePath(entry->relativePath);
}

} // namespace

const std::vector<ScriptCatalogEntry>& scriptCatalog() {
    return catalogState().entries;
}

std::string canonicalScriptId(const std::string& scriptRef) {
    if (const ScriptCatalogEntry* entry = findEntryByCanonicalId(scriptRef)) {
        return entry->scriptId;
    }
    if (const ScriptCatalogEntry* entry = findEntryByLegacyAlias(scriptRef)) {
        return entry->scriptId;
    }
    return scriptLookupToken(scriptRef);
}

std::string canonicalScriptIdFromLegacyAlias(const std::string& legacyScriptRef) {
    if (const ScriptCatalogEntry* entry = findEntryByLegacyAlias(legacyScriptRef)) {
        return entry->scriptId;
    }
    if (const ScriptCatalogEntry* entry = findEntryByCanonicalId(legacyScriptRef)) {
        return entry->scriptId;
    }
    return scriptLookupToken(legacyScriptRef);
}

std::string resolveScriptPath(const std::string& scriptRef) {
    if (const ScriptCatalogEntry* entry = findEntryByCanonicalId(scriptRef)) {
        return resolvedPathForEntry(entry);
    }
    if (const ScriptCatalogEntry* entry = findEntryByLegacyAlias(scriptRef)) {
        return resolvedPathForEntry(entry);
    }
    return scriptRef.empty() ? std::string() : platform::path::resolvePath(scriptRef);
}

std::string resolveLegacyScriptPath(const std::string& legacyScriptRef) {
    if (const ScriptCatalogEntry* entry = findEntryByLegacyAlias(legacyScriptRef)) {
        return resolvedPathForEntry(entry);
    }
    if (const ScriptCatalogEntry* entry = findEntryByCanonicalId(legacyScriptRef)) {
        return resolvedPathForEntry(entry);
    }
    return legacyScriptRef.empty() ? std::string() : platform::path::resolvePath(legacyScriptRef);
}

std::string creditsDataPath() {
    return platform::path::resolvePath(kCreditsDataRelativePath);
}

} // namespace vn
