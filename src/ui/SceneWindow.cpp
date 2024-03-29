#include <Cen/ui/SceneWindow.h>
#include <Cen/Scene.h>
#include <imgui.h>

bool renderTransform(cen::Scene::SceneNode* node) {
    auto position = node->transform.position();
    auto eulerAnglesRad = node->transform.rotation().unit().toEuler();
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
    if (node->index == scene->_primaryCamera)
        ImGui::Text("Is primary camera");
    else {
        if (ImGui::Button("Set as primary"))
            scene->setPrimaryCamera(node);
    }
    if (node->index == scene->_cullingCamera)
        ImGui::Text("Is culling camera");
    else {
        if (ImGui::Button("Set as culling"))
            scene->setCullingCamera(node);
    }
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

void nodeTypeLight(cen::Scene::SceneNode* node, cen::Scene* scene) {
    ImGui::Text("Light: %s", node->name.c_str());
    const char* modes[] = { "DIRECTIONAL", "POINT" };
    cen::Light::Type types[] = { cen::Light::Type::DIRECTIONAL, cen::Light::Type::POINT };
    i32 modeIndex = static_cast<i32>(scene->getLight(node).type());
    if (ImGui::Combo("Type", &modeIndex, modes, 2)) {
        scene->getLight(node).setType(types[modeIndex]);
    }

    auto colour = scene->getLight(node).colour();
    if (ImGui::ColorEdit3("Colour", &colour[0]))
        scene->getLight(node).setColour(colour);
    auto intensity = scene->getLight(node).intensity();
    if (ImGui::DragFloat("Intensity", &intensity, 0.1))
        scene->getLight(node).setIntensity(intensity);
    auto radius = scene->getLight(node).radius();
    if (ImGui::DragFloat("Radius", &radius, 0.1))
        scene->getLight(node).setRadius(radius);
}

void traverseSceneNode(cen::Scene::SceneNode* node, cen::Scene* scene, i32 selectedMesh, i32 prevSelected) {
    for (u32 childIndex = 0; childIndex < node->children.size(); childIndex++) {
        auto& child = node->children[childIndex];
        ImGui::PushID(std::hash<const void*>()(child.get()));
        switch (child->type) {
            case cen::Scene::NodeType::NONE:
            {
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeNone(child.get());
                    renderTransform(child.get());
                    traverseSceneNode(child.get(), scene, selectedMesh, prevSelected);
                    ImGui::TreePop();
                }
            }
                break;
            case cen::Scene::NodeType::MESH:
                if (child.get()->index == prevSelected && prevSelected != selectedMesh)
                    ImGui::SetNextItemOpen(false);
                if (child.get()->index == selectedMesh) {
                    ImGui::SetNextItemOpen(true);
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(12, 109, 17, 255));
                }
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeMesh(child.get(), scene);
                    renderTransform(child.get());
                    traverseSceneNode(child.get(), scene, selectedMesh, prevSelected);
                    ImGui::TreePop();
                }
                if (child.get()->index == selectedMesh) {
                    ImGui::SetScrollHereY();
                    ImGui::PopStyleColor();
                }
                break;
            case cen::Scene::NodeType::CAMERA:
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeCamera(child.get(), scene);
                    child->transform.setPosition(scene->getCamera(child.get()).position());
                    child->transform.setRotation(scene->getCamera(child.get()).rotation());
                    if (renderTransform(child.get())) {
                        scene->getCamera(child.get()).setPosition(child->transform.position());
                        scene->getCamera(child.get()).setRotation(child->transform.rotation());
                    }
                    traverseSceneNode(child.get(), scene, selectedMesh, prevSelected);
                    ImGui::TreePop();
                }
                break;
            case cen::Scene::NodeType::LIGHT:
                if (ImGui::TreeNode(child->name.c_str())) {
                    nodeTypeLight(child.get(), scene);
                    child->transform.setPosition(scene->getLight(child.get()).position());
                    child->transform.setRotation(scene->getLight(child.get()).rotation());
                    if (renderTransform(child.get())) {
                        scene->getLight(child.get()).setPosition(child->transform.position());
                        scene->getLight(child.get()).setRotation(child->transform.rotation());
                    }
                    traverseSceneNode(child.get(), scene, selectedMesh, prevSelected);
                    ImGui::TreePop();
                }
                break;
        }
        ImGui::PopID();
    }
}

void cen::ui::SceneWindow::render() {
    i32 prevSelected = selectedMesh;
    if (renderer->feedbackInfo().meshId != 0)
        selectedMesh = renderer->feedbackInfo().meshId;

    if (ImGui::Begin(name.c_str())) {
        traverseSceneNode(scene->_rootNode.get(), scene, selectedMesh, prevSelected);
    }
    ImGui::End();
}
