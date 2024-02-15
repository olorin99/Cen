#include <Cen/Material.h>

u32 cen::Material::s_materialId = 0;

auto cen::Material::create(cen::Material::CreateInfo info) -> Material {
    Material material = {};

    material._engine = info.engine;
    material._id = s_materialId++;
    material.setVariant(Variant::LIT, info.lit);
    material.setVariant(Variant::UNLIT, info.unlit);

    material.build();
    material._materialBuffer = info.engine->device()->createBuffer({
        .size = material.size() * 10,
        .usage = canta::BufferUsage::STORAGE,
        .type = canta::MemoryType::STAGING,
        .persistentlyMapped = true,
        .name = std::format("Material:{}", material.id())
    });

    return material;
}

void cen::Material::setVariant(cen::Material::Variant variant, canta::PipelineHandle pipeline) {
    _variants[static_cast<u8>(variant)] = pipeline;
}

auto cen::Material::getVariant(cen::Material::Variant variant) -> canta::PipelineHandle {
    return _variants[static_cast<u8>(variant)];
}

// gets size of material type and checks all variants match
auto cen::Material::build() -> bool {
    u32 size = 0;
    for (auto& variant : _variants) {
        if (!variant)
            continue;

        u32 materialParametersSize = variant->interface().getType("materials").size;
        if (size == 0)
            size = materialParametersSize;
        if (size != materialParametersSize)
            return false;
    }
    _materialSize = size;
    return true;
}