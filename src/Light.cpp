#include <Cen/Light.h>

auto cen::Light::create(cen::Light::CreateInfo info) -> Light {
    Light light = {};

    light._type = info.type;
    light._position = info.position;
    light._rotation = info.rotation;
    light._colour = info.colour;
    light._intensity = info.intensity;
    light._radius = info.radius;

    return light;
}

void cen::Light::setType(cen::Light::Type type) {
    _type = type;
    _dirty = true;
}

void cen::Light::setPosition(const ende::math::Vec3f &pos) {
    _position = pos;
    _dirty = true;
}

void cen::Light::setRotation(const ende::math::Quaternion &rot) {
    _rotation = rot;
    _dirty = true;
}

void cen::Light::setColour(const ende::math::Vec3f &colour) {
    _colour = colour;
    _dirty = true;
}

void cen::Light::setIntensity(f32 intensity) {
    _intensity = intensity;
    _dirty = true;
}

void cen::Light::setRadius(f32 radius) {
    _radius = radius;
    _dirty = true;
}

auto cen::Light::gpuLight() const -> GPULight {
    return {
        .position = _type == Type::DIRECTIONAL ? _rotation.unit().front() : _position,
        .type = static_cast<u32>(_type),
        .colour = _colour,
        .intensity = _intensity,
        .radius = _radius
    };
}
