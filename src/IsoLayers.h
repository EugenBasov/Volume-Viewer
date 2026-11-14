#pragma once

#include <glm/glm.hpp>

#include <map>
#include <string>
#include <vector>

// Editable list of iso-surface layers
class ISOList {
public:
    // Must match MAX_LAYERS in shaders/volume.frag
    static constexpr unsigned max_layers = 6;

    std::vector<std::string> layers;    // layer names in the order shown in the UI
    std::map<std::string, float> iso_values;
    std::map<std::string, glm::vec3> iso_colors;
    std::map<std::string, bool> is_focus;

    void addISO(std::string name, float value, glm::vec3 color = glm::vec3(1.f, 0.2f, 0.2f));
    void deleteISO(std::string name);
    void setName(std::string name, std::string newName);

    // ImGui widgets for adding, renaming and editing the layers
    void renderInterface(bool focusLayers = true);

private:
    // Appends " (1)", " (2)", ... if the name is already taken
    std::string uniqueName(const std::string& name) const;
};
