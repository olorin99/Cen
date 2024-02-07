#ifndef CEN_ASSETMANAGER_H
#define CEN_ASSETMANAGER_H

#include <tsl/robin_map.h>
#include <filesystem>
#include <Cen/Model.h>

namespace cen {

    class Engine;

    class AssetManager {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            std::filesystem::path rootPath = {};
        };
        static auto create(CreateInfo info) -> AssetManager;

        AssetManager() = default;

        auto loadModel(const std::filesystem::path& path) -> Model*;

    private:

        enum class AssetType {
            IMAGE,
            MODEL,
        };

        auto getAssetIndex(u32 hash) -> i32;
        auto registerAsset(u32 hash, const std::filesystem::path& path, const std::string& name, AssetType type) -> i32;

        Engine* _engine = nullptr;

        std::filesystem::path _rootPath = {};
        std::vector<std::filesystem::path> _searchPaths = {};

        tsl::robin_map<u32, i32> _assetMap = {};

        struct AssetMetadata {
            u32 hash = 0;
            std::string name = {};
            std::filesystem::path path = {};
            bool loaded = false;
            AssetType type = AssetType::MODEL;
            i32 index = 0;
        };
        std::vector<AssetMetadata> _metadata = {};

        std::vector<Model> _models = {};

    };

}

#endif //CEN_ASSETMANAGER_H
