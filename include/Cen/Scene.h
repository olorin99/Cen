#ifndef CEN_SCENE_H
#define CEN_SCENE_H

#include <Ende/platform.h>
#include <vector>
#include <cen.glsl>
#include <Cen/Engine.h>
#include <Cen/Model.h>
#include <Cen/Transform.h>
#include <Cen/Camera.h>

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
            Transform transform = {};
            ende::math::Mat4f worldTransform = {};
            std::string name = {};
            SceneNode* parent = nullptr;
            std::vector<std::unique_ptr<SceneNode>> children = {};
            i32 index = -1;
        };

        auto addMesh(std::string_view name, const Mesh& mesh, const Transform& transform, SceneNode* parent = nullptr) -> SceneNode*;
        auto getMesh(SceneNode* node) -> GPUMesh&;

        auto addCamera(std::string_view name, const Camera& camera, const Transform& transform, SceneNode* parent = nullptr) -> SceneNode*;
        auto getCamera(SceneNode* node) -> Camera&;
        auto getCamera(i32 index) -> Camera&;
        auto primaryCamera() -> Camera& { return getCamera(_primaryCamera); }
        auto cullingCamera() -> Camera& { return getCamera(_cullingCamera); }
        void setPrimaryCamera(SceneNode* node);
        void setCullingCamera(SceneNode* node);

//    private:

        Engine* _engine = nullptr;

        std::unique_ptr<SceneNode> _rootNode = nullptr;

        std::vector<GPUMesh> _meshes = {};
        std::vector<ende::math::Mat4f> _worldTransforms = {};
        std::vector<GPUCamera> _gpuCameras = {};

        std::vector<Camera> _cameras = {};
        i32 _primaryCamera = -1;
        i32 _cullingCamera = -1;

        canta::BufferHandle _meshBuffer[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _transformBuffer[canta::FRAMES_IN_FLIGHT] = {};
        canta::BufferHandle _cameraBuffer[canta::FRAMES_IN_FLIGHT] = {};

        u32 _maxMeshlets = 0;
        u32 _totalMeshlets = 0;
        u32 _totalPrimitives = 0;

    };

}

#endif //CEN_SCENE_H
