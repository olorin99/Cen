#ifndef CEN_TRANSFORM_H
#define CEN_TRANSFORM_H

#include <Ende/math/Mat.h>
#include <Ende/math/Quaternion.h>

namespace cen {

    class Transform {
    public:

        struct CreateInfo {
            ende::math::Vec3f position = { 0, 0, 0 };
            ende::math::Quaternion rotation = { 0, 0, 0, 1 };
            ende::math::Vec3f scale = { 1, 1, 1 };
        };
        static auto create(CreateInfo info) -> Transform;

        Transform() = default;

        Transform(const Transform& rhs);
        auto operator=(const Transform& rhs) -> Transform&;

        Transform(Transform&& rhs) noexcept;
        auto operator=(Transform&& rhs) noexcept -> Transform&;

        auto local() const -> ende::math::Mat4f;

        auto position() const -> const ende::math::Vec3f& { return _position; }
        void setPosition(const ende::math::Vec3f& position);
        void addPos(const ende::math::Vec3f& offset);

        auto rotation() const -> const ende::math::Quaternion& { return _rotation; }
        void setRotation(const ende::math::Quaternion& rotation);
        void rotate(const ende::math::Quaternion& rotation);
        void rotate(const ende::math::Vec3f& axis, f32 angle);

        auto scale() const -> const ende::math::Vec3f& { return _scale; }
        void setScale(const ende::math::Vec3f& scale);
        void multScale(const ende::math::Vec3f& scale);

        auto dirty() const -> bool { return _dirty; }
        void setDirty(bool dirty) { _dirty = dirty; }

    private:

        ende::math::Vec3f _position = { 0, 0, 0 };
        ende::math::Quaternion _rotation = { 0, 0, 0, 1 };
        ende::math::Vec3f _scale = { 1, 1, 1 };
        bool _dirty = true;

    };

}

#endif //CEN_TRANSFORM_H
