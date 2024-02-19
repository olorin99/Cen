#ifndef CEN_MATERIAL_H
#define CEN_MATERIAL_H

#include <Ende/platform.h>
#include <string_view>
#include <span>
#include <Canta/Device.h>

namespace cen {

    class Engine;
    class Material;

    class MaterialInstance {
    public:

        MaterialInstance(MaterialInstance&& rhs) noexcept;

        auto operator=(MaterialInstance&& rhs) noexcept -> MaterialInstance&;

        auto material() const -> Material* { return _material; }
        auto offset() const -> u32 { return _offset; }
        auto index() const -> u32;

        auto setParameter(std::string_view name, std::span<const u8> data) -> bool;

        template<typename T>
        auto setParameter(std::string_view name, const T& data) -> bool {
            return setParameter(name, { reinterpret_cast<const u8*>(&data), sizeof(T) });
        }

        auto getParameter(std::string_view name, u32 size) -> void*;

        template <typename T>
        auto getParameter(std::string_view name) -> T {
            auto data = getParameter(name, sizeof(T));
            if (!data)
                return T();
            return *reinterpret_cast<T*>(data);
        }


        void setData(std::span<const u8> data, u32 offset = 0);

    private:
        friend Material;

        MaterialInstance() = default;

        Material* _material = nullptr;
        u32 _offset = 0;

    };

    class Material {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
            canta::PipelineHandle lit = {};
            canta::PipelineHandle unlit = {};
        };

        static auto create(CreateInfo info) -> Material;

        auto id() const -> u32 { return _id; }
        auto size() const -> u32 { return _materialSize; }
        auto buffer() const -> canta::BufferHandle { return _materialBuffer; }

        auto instance() -> MaterialInstance;

        enum class Variant {
            LIT,
            UNLIT,
            MAX
        };
        void setVariant(Variant variant, canta::PipelineHandle pipeline);
        auto getVariant(Variant variant) const -> canta::PipelineHandle;

        auto build() -> bool;

        void upload();

    private:

        friend MaterialInstance;

        static u32 s_materialId;

        Engine* _engine = nullptr;
        u32 _id = 0;
        u32 _materialSize = 0;

        std::array<canta::PipelineHandle, static_cast<u8>(Variant::MAX)> _variants = {};

        bool _dirty = true;
        std::vector<u8> _data = {};
        canta::BufferHandle _materialBuffer = {};

    };

}

#endif //CEN_MATERIAL_H
