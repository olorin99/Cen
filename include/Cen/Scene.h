#ifndef CEN_SCENE_H
#define CEN_SCENE_H

#include <Ende/platform.h>
#include <vector>
#include <cen.glsl>
#include <Cen/Engine.h>

namespace cen {

    class Scene {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
        };
        static auto create(CreateInfo info) -> Scene;

        void prepare();

        void addMesh(const GPUMesh& mesh, const ende::math::Mat4f& transform);


        auto meshCount() const -> u32 { return _meshes.size(); }
        auto maxMeshlets() const -> u32 { return _maxMeshlets; }

//    private:

        Engine* _engine = nullptr;

        std::vector<GPUMesh> _meshes = {};
        std::vector<ende::math::Mat4f> _transforms = {};

        canta::BufferHandle _meshBuffer[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _transformBuffer[canta::FRAMES_IN_FLIGHT] = {};

        u32 _maxMeshlets = 0;

    };

}

#endif //CEN_SCENE_H
