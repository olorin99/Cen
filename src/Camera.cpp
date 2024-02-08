#include "Cen/Camera.h"

auto cen::Camera::create(cen::Camera::CreatePerspectiveInfo info) -> Camera {
    Camera camera = {};

    camera._projection = ende::math::perspective(info.fov, info.width / info.height, info.near, info.far);
    camera._position = info.position;
    camera._rotation = info.rotation;
    camera._fov = info.fov;
    camera._width = info.width;
    camera._height = info.height;
    camera._near = info.near;
    camera._far = info.far;

    return camera;
}

auto cen::Camera::create(cen::Camera::CreateOrthographicInfo info) -> Camera {
    Camera camera = {};

    camera._projection = ende::math::orthographic(info.left, info.right, info.bottom, info.top, info.near, info.far);
    camera._position = info.position;
    camera._rotation = info.rotation;
    camera._near = info.near;
    camera._far = info.far;

    return camera;
}

auto cen::Camera::view() const -> ende::math::Mat4f {
    auto translation = ende::math::translation<4, f32>(_position);
    auto rotation = _rotation.inverse().toMat();
    return rotation * translation;
}

void cen::Camera::setNear(f32 near) {
    _near = near;
    _projection = ende::math::perspective(_fov, _width / _height, _near, _far);
}

void cen::Camera::setFar(f32 far) {
    _far = far;
    _projection = ende::math::perspective(_fov, _width / _height, _near, _far);
}

void cen::Camera::setFov(f32 fov) {
    _fov = fov;
    _projection = ende::math::perspective(_fov, _width / _height, _near, _far);
}

auto cen::Camera::gpuCamera() const -> GPUCamera {
    auto planes = _frustum.planes();
    auto corners = frustumCorners();
    cen::Frustum frustum = {};
    for (u32 i = 0; i < 6; i++)
        frustum.planes[i] = planes[i];
    for (u32 i = 0; i < 8; i++)
        frustum.corners[i] = corners[i];

    return {
        .projection = projection(),
        .view = view(),
        .position = _position,
        .near = _near,
        .far = _far,
        .frustum = frustum
    };
}

auto cen::Camera::frustumCorners() const -> std::array<ende::math::Vec4f, 8> {
    const auto inverseViewProjection = viewProjection().inverse();
    std::array<ende::math::Vec4f, 8> corners = {};
    u32 index = 0;
    for (unsigned int x = 0; x < 2; ++x) {
        for (unsigned int y = 0; y < 2; ++y) {
            for (unsigned int z = 0; z < 2; ++z) {
                const ende::math::Vec4f pt = inverseViewProjection.transform(ende::math::Vec4f{
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f});

                corners[index++] = pt / pt.w();
            }
        }
    }
    return corners;
}