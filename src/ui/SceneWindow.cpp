#include <Cen/ui/SceneWindow.h>
#include <Cen/Scene.h>
#include <imgui.h>

bool renderTransform(cen::Scene::SceneNode* node) {
    auto position = node->transform.position();
    auto eulerAnglesRad = node->transform.rotation().toEuler();
    auto eulerAnglesDeg = ende::math::Vec3f{
            static_cast<f32>(ende::math::deg(eulerAnglesRad.x())),
            static_cast<f32>(ende::math::deg(eulerAnglesRad.y())),
            static_cast<f32>(ende::math::deg(eulerAnglesRad.z()))
    };
    auto scale = node->transform.scale();
    bool changed = false;

    if (ImGui::DragFloat3("Position", &position[0], 0.1)) {
        node->transform.setPosition(position);
        changed = true;
    }

    if (ImGui::DragFloat3("Rotation", &eulerAnglesDeg[0], 0.1)) {
        ende::math::Quaternion rotation(ende::math::rad(eulerAnglesDeg.x()), ende::math::rad(eulerAnglesDeg.y()), ende::math::rad(eulerAnglesDeg.z()));
        node->transform.setRotation(rotation);
        changed = true;
    }

    if (ImGui::DragFloat3("Scale", &scale[0], 0.1)) {
        node->transform.setScale(scale);
        changed = true;
    }

    return changed;
}

void nodeTypeNone(cen::Scene::SceneNode* node) {
    ImGui::Text(node->name.c_str());
}

void nodeTypeMesh(cen::Scene::SceneNode* node, cen::Scene* scene) {
    ImGui::Text("Mesh: %s", node->name.c_str());
    auto mesh = scene->getMesh(node);
    ImGui::Text("Meshlet Offset: %d", mesh.meshletOffset);
    ImGui::Text("Meshlet Count: %d", mesh.meshletCount);
    ImGui::Text("Min: (%f, %f, %f)", mesh.min.x(), mesh.min.y(), mesh.min.z());
    ImGui::Text("Max: (%f, %f, %f)", mesh.max.x(), mesh.max.y(), mesh.max.z());
}

void nodeTypeCamera(cen::Scene::SceneNode* node, cen::Scene* scene) {
    ImGui::Text("Camera: %s", node->name.c_str());
    if (ImGui::Button("Set as primary"))
        scene->setPrimaryCamera(node);
    if (ImGui::Button("Set as culling"))
        scene->setCullingCamera(node);
    auto near = scene->getCamera(node).near();
    if (ImGui::DragFloat("Near", &near, 0.1))
        scene->getCamera(node).setNear(near);
    auto far = scene->getCamera(node).far();
    if (ImGui::DragFloat("Far", &far, 0.1))
        scene->getCamera(node).setFar(far);
    f32 fov = ende::math::deg(scene->getCamera(node).fov());
    if (ImGui::SliderFloat("FOV", &fov, 1, 180))
        scene->getCamera(node).setFov(ende::math::rad(fov));
}

void traverseSceneNode(cen::Scene::SceneNode* node, cen::Scene* scene) {
    for (u32 childIndex = 0; childIndex < node->children.size(); childIndex++) {
        auto& child = node->children[childIndex];
        ImGui::PushID(std::hash<const void*>()(child.get()));
        switch (child->type) {
            case cen::Scene::NodeType::NONE:
            {
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeNone(child.get());
                    renderTransform(child.get());
                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
            }
                break;
            case cen::Scene::NodeType::MESH:
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeMesh(child.get(), scene);
                    renderTransform(child.get());
                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
                break;
            case cen::Scene::NodeType::CAMERA:
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeCamera(child.get(), scene);
                    child->transform.setPosition(scene->getCamera(child.get()).position());
                    child->transform.setRotation(scene->getCamera(child.get()).rotation(a));
                    if (renderTransform(child.get())) {
                        scene->getCamera(child.get()).setPosition(child->transform.position());
                        scene->getCamera(child.get()).setRotation(child->transform.rotation());
                    }
                    traverseSceneNode(child.get(), scene);
                    ImGui::TreePop();
                }
                break;
        }
        ImGui::PopID();
    }
}

void cen::ui::SceneWindow::render() {
    if (ImGui::Begin(name.c_str())) {
        traverseSceneNode(scene->_rootNode.get(), scene);
    }
    ImGui::End();
}
