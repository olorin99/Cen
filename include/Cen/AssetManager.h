#ifndef CEN_ASSETMANAGER_H
#define CEN_ASSETMANAGER_H

#include <tsl/robin_map.h>
#include <filesystem>
#include <Cen/Model.h>
#include <Cen/Material.h>
#include <Canta/Device.h>

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

        auto loadImage(const std::filesystem::path& path, canta::Format format) -> canta::ImageHandle;

        auto loadModel(const std::filesystem::path& path) -> Model*;

        auto loadMaterial(const std::filesystem::path& path) -> Material*;

    private:

        enum class AssetType {
            IMAGE,
            MODEL,
            MATERIAL,
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

        std::vector<canta::ImageHandle> _images = {};
        std::vector<Model> _models = {};
        std::vector<Material> _materials = {};

    };

}

#endif //CEN_ASSETMANAGER_H
