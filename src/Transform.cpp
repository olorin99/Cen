#include <Cen/Transform.h>

auto cen::Transform::create(cen::Transform::CreateInfo info) -> Transform {
    Transform transform = {};

    transform._position = info.position;
    transform._rotation = info.rotation;
    transform._scale = info.scale;

    return transform;
}

cen::Transform::Transform(const cen::Transform &rhs)
    : _position(rhs._position),
    _rotation(rhs._rotation),
    _scale(rhs._scale)
{}

auto cen::Transform::operator=(const cen::Transform &rhs) -> Transform & {
    _position = rhs._position;
    _rotation = rhs._rotation;
    _scale = rhs._scale;
    return *this;
}

cen::Transform::Transform(cen::Transform &&rhs) noexcept {
    std::swap(_position, rhs._position);
    std::swap(_rotation, rhs._rotation);
    std::swap(_scale, rhs._scale);
}

auto cen::Transform::operator=(cen::Transform &&rhs) noexcept -> Transform & {
    std::swap(_position, rhs._position);
    std::swap(_rotation, rhs._rotation);
    std::swap(_scale, rhs._scale);
    return *this;
}

auto cen::Transform::local() const -> ende::math::Mat4f {
    auto translation = ende::math::translation<4, f32>(_position);
    auto rotation = _rotation.unit().toMat();
    auto scale = ende::math::scale<4, f32>(_scale);
    return translation * rotation * scale;
}

void cen::Transform::setPosition(const ende::math::Vec3f &position) {
    _position = position;
    setDirty(true);
}

void cen::Transform::setRotation(const ende::math::Quaternion &rotation) {
    _rotation = rotation;
    setDirty(true);
}

void cen::Transform::rotate(const ende::math::Quaternion &rotation) {
    _rotation = (_rotation * rotation).unit();
    setDirty(true);
}

void cen::Transform::rotate(const ende::math::Vec3f &axis, f32 angle) {
    rotate(ende::math::Quaternion(axis, angle));
}

void cen::Transform::setScale(const ende::math::Vec3f &scale) {
    _scale = scale;
    setDirty(true);
}

void cen::Transform::multScale(const ende::math::Vec3f &scale) {
    _scale = _scale * scale;
    setDirty(true);
}