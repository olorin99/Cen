#ifndef CEN_SCENE_H
#define CEN_SCENE_H

#include <Ende/platform.h>
#include <vector>
#include <cen.glsl>
#include <Cen/Engine.h>
#include <Cen/Model.h>

namespace cen {

    class Scene {
    public:

        struct CreateInfo {
            Engine* engine = nullptr;
        };
        static auto create(CreateInfo info) -> Scene;

        void prepare();

        auto meshCount() const -> u32 { return _meshes.size(); }
        auto maxMeshlets() const -> u32 { return _maxMeshlets; }
        auto totalMeshlets() const -> u32 { return _totalMeshlets; }
        auto totalPrimtives() const -> u32 { return _totalPrimitives; }

        enum class NodeType {
            NONE = 0,
            MESH = 1,
            LIGHT = 2,
            CAMERA = 3,
        };

        struct SceneNode {
            virtual ~SceneNode() = default;

            NodeType type = NodeType::NONE;
            ende::math::Mat4f transform = ende::math::identity<4, f32>();
            std::string name = {};
            SceneNode* parent = nullptr;
            std::vector<std::unique_ptr<SceneNode>> children = {};
            i32 index = -1;
        };

        auto addMesh(const Mesh& mesh, const ende::math::Mat4f& transform, SceneNode* parent = nullptr) -> SceneNode*;


//    private:

        Engine* _engine = nullptr;

        std::unique_ptr<SceneNode> _rootNode = nullptr;

        std::vector<GPUMesh> _meshes = {};
        std::vector<ende::math::Mat4f> _transforms = {};

        canta::BufferHandle _meshBuffer[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _transformBuffer[canta::FRAMES_IN_FLIGHT] = {};

        u32 _maxMeshlets = 0;
        u32 _totalMeshlets = 0;
        u32 _totalPrimitives = 0;

    };

}

#endif //CEN_SCENE_H
