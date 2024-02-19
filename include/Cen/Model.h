#ifndef CEN_MODEL_H
#define CEN_MODEL_H

#include <Ende/platform.h>
#include <Ende/math/Vec.h>
#include <Ende/math/Mat.h>
#include <vector>
#include <string>
#include <Cen/Material.h>

namespace cen {

    struct Mesh {
        u32 meshletOffset = 0;
        u32 meshletCount = 0;
        ende::math::Vec4f min;
        ende::math::Vec4f max;
        MaterialInstance* materialInstance = nullptr;
    };

    class Model {
    public:

        struct Node {
            std::vector<u32> meshes = {};
            std::vector<u32> children = {};
            ende::math::Mat4f transform = {};
            std::string name = {};
        };

//    private:

        std::vector<Node> nodes = {};
        std::vector<Mesh> meshes = {};
        //TODO: add material support
        std::vector<MaterialInstance> materials = {};
        std::vector<canta::ImageHandle> images = {};

    };

}

#endif //CEN_MODEL_H
