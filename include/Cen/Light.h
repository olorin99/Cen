#ifndef CEN_LIGHT_H
#define CEN_LIGHT_H

#include <Ende/math/Vec.h>
#include <Ende/math/Quaternion.h>
#include <cen.glsl>

namespace cen {

    class Light {
    public:

        enum Type {
            DIRECTIONAL = 0,
            POINT = 1,
        };

        struct CreateInfo {
            ende::math::Vec3f position = { 0, 0, 0 };
            ende::math::Quaternion rotation = { 0, 0, 0, 1 };
            ende::math::Vec3f colour = { 1, 1, 1 };
            f32 intensity = 1;
            f32 radius = 1;
        };
        static auto create(CreateInfo info) -> Light;

        Light() = default;

        auto type() const -> Type { return _type; }
        void setType(Type type);

        auto position() const -> const ende::math::Vec3f& { return _position; }
        void setPosition(const ende::math::Vec3f& pos);

        auto rotation() const -> const ende::math::Quaternion& { return _rotation; }
        void setRotation(const ende::math::Quaternion& rot);

        auto colour() const -> const ende::math::Vec3f& { return _colour; }
        void setColour(const ende::math::Vec3f& colour);

        auto intensity() const -> f32 { return _intensity; }
        void setIntensity(f32 intensity);

        auto radius() const -> f32 { return _radius; }
        void setRadius(f32 radius);

        auto gpuLight() const -> GPULight;

    private:

        Type _type = Type::DIRECTIONAL;
        ende::math::Vec3f _position = {};
        ende::math::Quaternion _rotation = {};
        ende::math::Vec3f _colour = { 1, 1, 1 };
        f32 _intensity = 1;
        f32 _radius = 1;
        bool _dirty = true;

    };

}

#endif //CEN_LIGHT_H
