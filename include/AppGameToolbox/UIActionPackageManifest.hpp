#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace appgametoolbox {

// Portable JSON package contract. Resource paths are source-defined names, not filesystem paths.
struct UIActionPackageRoute {
    std::string id;
    std::string html;
    std::string css;
    bool preload = false;
};

struct UIActionPackageAsset {
    std::string id;
    std::string path;
};

struct UIActionPackageManifest {
    std::string packageId;
    std::string entryRoute;
    std::string localization;
    std::vector<UIActionPackageRoute> routes;
    std::vector<UIActionPackageAsset> assets;
};

// Parses and validates a manifest. Unknown JSON members are ignored for forward compatibility.
bool parseUIActionPackageManifest(std::string_view text, UIActionPackageManifest& manifest,
                                  std::string& error);

} // namespace appgametoolbox
