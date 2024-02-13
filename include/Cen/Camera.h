#ifndef CEN_CAMERA_H
#define CEN_CAMERA_H

#include <Ende/math/Quaternion.h>
#include <Ende/math/Mat.h>
#include <Ende/math/Frustum.h>

namespace cen {

    struct Frustum {
        ende::math::Vec4f planes[6];
        ende::math::Vec4f corners[8];
    };
    struct GPUCamera {
        ende::math::Mat4f projection;
        ende::math::Mat4f view;
        ende::math::Vec3f position;
        f32 near;
        f32 far;
        Frustum frustum;
    };

    class Camera {
    public:

        struct CreatePerspectiveInfo {
            ende::math::Vec3f position = { 0, 0, 0 };
            ende::math::Quaternion rotation = { 0, 0, 0, 1 };
            f32 fov = ende::math::rad(45);
            f32 width = 1;
            f32 height = 1;
            f32 near = 0.1;
            f32 far = 100;
        };
        static auto create(CreatePerspectiveInfo info) -> Camera;

        struct CreateOrthographicInfo {
            ende::math::Vec3f position = { 0, 0, 0 };
            ende::math::Quaternion rotation = { 0, 0, 0, 1 };
            f32 left = 0;
            f32 right = 0;
            f32 top = 0;
            f32 bottom = 0;
            f32 near = 0;
            f32 far = 0;
        };
        static auto create(CreateOrthographicInfo info) -> Camera;

        auto view() const -> ende::math::Mat4f;
        auto projection() const -> const ende::math::Mat4f& { return _projection; }
        auto viewProjection() const -> ende::math::Mat4f { return projection() * view(); }

        void setProjection(const ende::math::Mat4f& projection) { _projection = projection; }

        void setPosition(const ende::math::Vec3f& pos) { _position = pos; }
        auto position() const -> const ende::math::Vec3f& { return _position; }

        void setRotation(const ende::math::Quaternion& rot) { _rotation = rot; }
        auto rotation() const -> const ende::math::Quaternion& { return _rotation; }

        void setNear(f32 near);
        auto near() const -> f32 { return _near; }

        void setFar(f32 far);
        auto far() const -> f32 { return _far; }

        void setFov(f32 fov);
        auto fov() const -> f32 { return _fov; }

        auto gpuCamera() const -> GPUCamera;

        auto frustum() const -> const ende::math::Frustum& { return _frustum; }
        auto frustumCorners() const -> std::array<ende::math::Vec4f, 8>;
        void updateFrustum() { _frustum.update(viewProjection()); }

    private:

        Camera() = default;

        ende::math::Mat4f _projection = {};
        ende::math::Vec3f _position = {};
        ende::math::Quaternion _rotation = {};
        f32 _fov = 0;
        f32 _width = 0;
        f32 _height = 0;
        f32 _near = 0;
        f32 _far = 0;

        ende::math::Frustum _frustum = {};

    };

}

#endif //CEN_CAMERA_H
