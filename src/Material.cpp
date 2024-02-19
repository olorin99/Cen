#include <Cen/Material.h>
#include <cstring>
#include <Cen/Engine.h>

cen::MaterialInstance::MaterialInstance(cen::MaterialInstance &&rhs) noexcept {
    std::swap(_material, rhs._material);
    std::swap(_offset, rhs._offset);
}

auto cen::MaterialInstance::operator=(cen::MaterialInstance &&rhs) noexcept -> MaterialInstance & {
    std::swap(_material, rhs._material);
    std::swap(_offset, rhs._offset);
    return *this;
}

auto cen::MaterialInstance::setParameter(std::string_view name, std::span<const u8> data) -> bool {
    auto materialType = _material->_variants[0]->interface().getType("materials");
    for (auto& memberName : materialType.members) {
        if (memberName == name) {
            auto member = _material->_variants[0]->interface().getType(memberName);
            if (member.size == 0)
                return false;

            setData(data, member.offset);
            return true;
        }
    }
    return false;
}

auto cen::MaterialInstance::getParameter(std::string_view name, u32 size) -> void * {
    auto materialType = _material->_variants[0]->interface().getType("materials");
    for (auto& memberName : materialType.members) {
        if (memberName == name) {
            auto member = _material->_variants[0]->interface().getType(memberName);
            if (member.size == 0)
                return nullptr;

            return _material->_data.data() + _offset + member.offset;
        }
    }
    return nullptr;
}

void cen::MaterialInstance::setData(std::span<const u8> data, u32 offset) {
    assert(offset + data.size() <= _material->_materialSize);
    std::memcpy(_material->_data.data() + _offset + offset, data.data(), data.size());
    _material->_dirty = true;
}

u32 cen::Material::s_materialId = 0;

auto cen::Material::create(cen::Material::CreateInfo info) -> Material {
    Material material = {};

    material._engine = info.engine;
    material._id = s_materialId++;
    material.setVariant(Variant::LIT, info.lit);
    material.setVariant(Variant::UNLIT, info.unlit);

    auto res = material.build();
    assert(res);
    material._materialBuffer = info.engine->device()->createBuffer({
        .size = material.size() * 10,
        .usage = canta::BufferUsage::STORAGE,
        .type = canta::MemoryType::STAGING,
        .persistentlyMapped = true,
        .name = std::format("Material:{}", material.id())
    });

    return material;
}

auto cen::Material::instance() -> MaterialInstance {
    u32 offset = _data.size();
    _data.resize(offset + _materialSize);
    MaterialInstance instance = {};
    instance._material = this;
    instance._offset = offset;
    return instance;
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
    if (_materialSize == 0)
        return false;
    return true;
}

void cen::Material::upload() {
    if (_dirty) {
        if (_data.size() > _materialBuffer->size()) {
            _materialBuffer = _engine->device()->createBuffer({
                .size = static_cast<u32>(_data.size())
            }, _materialBuffer);
        }
        _materialBuffer->data(_data);
        _dirty = false;
    }
}