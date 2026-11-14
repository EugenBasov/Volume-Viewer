#include "IsoLayers.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <iostream>

std::string ISOList::uniqueName(const std::string& name) const
{
    if (iso_values.find(name) == iso_values.end())
        return name;
    for (int i = 1;; i++) {
        std::string candidate = name + " (" + std::to_string(i) + ")";
        if (iso_values.find(candidate) == iso_values.end())
            return candidate;
    }
}

void ISOList::addISO(std::string name, float value, glm::vec3 color)
{
    name = uniqueName(name);
    iso_values.emplace(name, value);
    iso_colors.emplace(name, color);
    is_focus.emplace(name, false);
    layers.push_back(name);
}

void ISOList::deleteISO(std::string name)
{
    if (iso_values.find(name) == iso_values.end()) {
        std::cerr << "There is no iso-value layer called " << name << std::endl;
        return;
    }
    iso_values.erase(name);
    iso_colors.erase(name);
    is_focus.erase(name);
    layers.erase(std::find(layers.begin(), layers.end(), name));
}

void ISOList::setName(std::string name, std::string newName)
{
    if (iso_values.find(name) == iso_values.end()) {
        std::cerr << "There is no iso-value layer called " << name << std::endl;
        return;
    }
    const float value = iso_values[name];
    const glm::vec3 color = iso_colors[name];
    const bool focus = is_focus[name];
    iso_values.erase(name);
    iso_colors.erase(name);
    is_focus.erase(name);

    newName = uniqueName(newName);
    iso_values.emplace(newName, value);
    iso_colors.emplace(newName, color);
    is_focus.emplace(newName, focus);
    *std::find(layers.begin(), layers.end(), name) = newName;
}

void ISOList::renderInterface(bool focusLayers)
{
    static char inputBuffer[128] = "";
    static int renamingIndex = -1;

    if (ImGui::Button("Add layer") && iso_values.size() < max_layers)
        addISO("New layer", 0.5f);

    ImGui::BeginChild("ItemList", ImVec2(300, 400), true);
    for (size_t i = 0; i < layers.size(); ++i) {
        std::snprintf(inputBuffer, sizeof(inputBuffer), "%s", layers[i].c_str());

        ImGui::PushID(static_cast<int>(i));

        // Clicking a layer name starts renaming, Enter or clicking elsewhere confirms it
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
        if (renamingIndex == static_cast<int>(i)) {
            if (ImGui::InputText("##Rename", inputBuffer, IM_ARRAYSIZE(inputBuffer),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                renamingIndex = -1;
            }
            if (!ImGui::IsItemActive() && !ImGui::IsItemHovered()) {
                renamingIndex = -1;
                if (inputBuffer[0] != '\0')
                    setName(layers[i], inputBuffer);
            }
        }
        else if (ImGui::Selectable(layers[i].c_str(), false, 0, ImVec2(ImGui::GetContentRegionAvail().x - 90, 0))) {
            renamingIndex = static_cast<int>(i);
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
        if (ImGui::Button("X")) {
            deleteISO(layers[i]);
        }
        else {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 90);
            ImGui::SliderFloat("##Layer", &iso_values[layers[i]], 0.f, 1.f);
            if (focusLayers) {
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
                ImGui::Checkbox("Focus?", &is_focus[layers[i]]);
            }
            ImGui::ColorEdit3("##Color", glm::value_ptr(iso_colors[layers[i]]));
        }

        ImGui::PopID();
    }
    ImGui::EndChild();
}
