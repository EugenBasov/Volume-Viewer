#include "ICPVR.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <algorithm>
#include <utility>
#include <vector>

ICPVR::ICPVR(GLuint program)
    : program(program)
{
    lowerThreshold_loc = glGetUniformLocation(program, "lowerThreshold");
    kt_loc = glGetUniformLocation(program, "kt");
    ks_loc = glGetUniformLocation(program, "ks");
    layers_count_loc = glGetUniformLocation(program, "layers_count");
}

void ICPVR::addISO(const std::string& name, float value, glm::vec3 color)
{
    iso_list.addISO(name, value, color);
}

void ICPVR::renderImage(int width, int height)
{
    glUseProgram(program);
    glViewport(0, 0, width, height);
    glUniform1f(lowerThreshold_loc, lowerThreshold);
    glUniform1f(kt_loc, kt);
    glUniform1f(ks_loc, ks);

    // The colour transfer function in the shader expects the layers sorted by iso-value
    std::vector<std::pair<float, glm::vec3>> layers;
    for (const auto& iso : iso_list.iso_values)
        layers.emplace_back(iso.second, iso_list.iso_colors[iso.first]);
    std::sort(layers.begin(), layers.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (size_t i = 0; i < layers.size(); i++) {
        const std::string index = "[" + std::to_string(i) + "]";
        glUniform1f(glGetUniformLocation(program, ("iso_v" + index).c_str()), layers[i].first);
        glUniform3fv(glGetUniformLocation(program, ("iso_c" + index).c_str()), 1, glm::value_ptr(layers[i].second));
    }
    glUniform1i(layers_count_loc, static_cast<GLint>(layers.size()));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void ICPVR::renderInterface()
{
    ImGui::Text("Lower threshold");
    ImGui::SliderFloat("##Lower threshold", &lowerThreshold, 0.f, 1.f);
    ImGui::Text("Interior visibility");
    ImGui::SliderFloat("##Interior visibility", &kt, 0.f, 10.f);
    ImGui::Text("Sharpness");
    ImGui::SliderFloat("##Sharpness", &ks, 0.f, 1.f);
    ImGui::NewLine();
    iso_list.renderInterface(false);
}
