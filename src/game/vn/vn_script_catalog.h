#ifndef VN_SCRIPT_CATALOG_H
#define VN_SCRIPT_CATALOG_H

#include <string>
#include <vector>

namespace vn {

struct ScriptCatalogEntry {
    std::string scriptId;
    std::string relativePath;
    std::string category;
    std::vector<std::string> legacyAliases;
};

const std::vector<ScriptCatalogEntry>& scriptCatalog();
std::string canonicalScriptId(const std::string& scriptRef);
std::string canonicalScriptIdFromLegacyAlias(const std::string& legacyScriptRef);
std::string resolveScriptPath(const std::string& scriptRef);
std::string resolveLegacyScriptPath(const std::string& legacyScriptRef);
std::string creditsDataPath();

} // namespace vn

#endif // VN_SCRIPT_CATALOG_H
