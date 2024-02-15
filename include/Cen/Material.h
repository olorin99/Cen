#ifndef CEN_MATERIAL_H
#define CEN_MATERIAL_H

#include <Ende/platform.h>
#include <Cen/Engine.h>

namespace cen {

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



        enum class Variant {
            LIT,
            UNLIT,
            MAX
        };
        void setVariant(Variant variant, canta::PipelineHandle pipeline);
        auto getVariant(Variant variant) -> canta::PipelineHandle;

        auto build() -> bool;

    private:

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
