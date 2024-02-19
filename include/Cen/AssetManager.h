#ifndef CEN_ASSETMANAGER_H
#define CEN_ASSETMANAGER_H

#include <tsl/robin_map.h>
#include <filesystem>
#include <Cen/Model.h>
#include <Cen/Material.h>
#include <Canta/Device.h>

namespace cen {

    class Engine;
    class AssetManager;

    template <typename T, typename Manager = AssetManager>
    class Asset {
    public:

        Asset() = default;

        T& operator*();

        T* operator->();

        explicit operator bool() const noexcept {
            if (_index < 0 || !_manager)
                return false;
            return _manager->_metadata[_index].loaded;
        }

    private:
        friend Manager;

        Asset(Manager* manager, i32 index)
            : _manager(manager),
            _index(index)
        {}

        Manager* _manager = nullptr;
        i32 _index = 0;

    };

    class AssetManager {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            std::filesystem::path rootPath = {};
        };
        static auto create(CreateInfo info) -> AssetManager;

        AssetManager() = default;

        auto loadImage(const std::filesystem::path& path, canta::Format format) -> canta::ImageHandle;

        auto loadModel(const std::filesystem::path& path, Asset<Material> material = {}) -> Asset<Model>;

        auto loadMaterial(const std::filesystem::path& path) -> Asset<Material>;

        void uploadMaterials();

        auto materials() const -> std::span<const Material> { return _materials; }

    private:
        friend Asset<Model>;
        friend Asset<Material>;

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
