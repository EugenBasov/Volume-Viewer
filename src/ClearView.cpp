#include "ClearView.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include <iostream>

float ClearView::lensSize = 0.25f;

namespace {

void detachColorAttachments()
{
    for (GLenum attachment : { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 })
        glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, 0, 0, 0);
}

bool framebufferComplete(const std::string& layerName, const char* pass)
{
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
        return true;
    std::cerr << "Framebuffer error on iso-value \"" << layerName << "\" (" << pass
              << "), status 0x" << std::hex << status << std::dec << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
}

} // namespace

ClearView::ClearView(GLuint program)
    : program(program)
{
    glGenFramebuffers(1, &framebuffer);
    target_loc = glGetUniformLocation(program, "target");
    isoVal_loc = glGetUniformLocation(program, "iso_val");
    isoCol_loc = glGetUniformLocation(program, "iso_col");
    iteration_loc = glGetUniformLocation(program, "iteration");
    posTex_loc = glGetUniformLocation(program, "positionTex");
    normTex_loc = glGetUniformLocation(program, "normalTex");
    lensSize_loc = glGetUniformLocation(program, "lensSize");
    curvature_loc = glGetUniformLocation(program, "curvature");
    trans_strength_loc = glGetUniformLocation(program, "trans_strength");
}

void ClearView::addISO(const std::string& name, float value, glm::vec3 color)
{
    iso_list.addISO(name, value, color);
}

// (Re)creates the offscreen texture arrays for the current window size
void ClearView::genTextures()
{
    if (!texs.empty())
        glDeleteTextures(static_cast<GLsizei>(texs.size()), texs.data());
    texs.assign(TEXTURES_N, 0);
    glGenTextures(TEXTURES_N, texs.data());

    for (int i = 0; i < TEXTURES_N; i++) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, texs[i]);
        if (i == DEPTH || i == CURVATURE)
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_R32F, width, height, ISOList::max_layers, 0, GL_RED, GL_FLOAT, nullptr);
        else
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA16F, width, height, ISOList::max_layers, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
}

// Ray-casts one iso-surface into layer `layer` of the offscreen textures (passes 0-2)
void ClearView::renderPrep(const std::string& name, int layer)
{
    glUseProgram(program);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
    glUniform1f(isoVal_loc, iso_list.iso_values[name]);
    glUniform3fv(isoCol_loc, 1, glm::value_ptr(iso_list.iso_colors[name]));
    glUniform1i(posTex_loc, 1);
    glUniform1i(normTex_loc, 2);
    glUniform1i(iteration_loc, layer);
    const GLenum drawBuffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);

    // Pass 0: position and depth of the first hit
    glUniform1i(target_loc, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texs[DEPTH], 0, layer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texs[POSITION], 0, layer);
    if (!framebufferComplete(name, "depth"))
        return;
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    detachColorAttachments();

    // Pass 1: normal and shaded colour
    glUniform1i(target_loc, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texs[NORMAL], 0, layer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texs[COLOR], 0, layer);
    if (!framebufferComplete(name, "normal"))
        return;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[POSITION]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    detachColorAttachments();

    // Pass 2: curvature estimated from the normals
    glUniform1i(target_loc, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texs[CURVATURE], 0, layer);
    if (!framebufferComplete(name, "curvature"))
        return;
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[NORMAL]);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
    detachColorAttachments();

    glUniform1i(target_loc, -1);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Pass 3: composites all layers into the final image
void ClearView::renderComp()
{
    int i = 0;
    for (const auto& iso : iso_list.iso_values) {
        const std::string index = "[" + std::to_string(i) + "]";
        glUniform1f(glGetUniformLocation(program, ("iso_v" + index).c_str()), iso.second);
        glUniform1i(glGetUniformLocation(program, ("is_focus" + index).c_str()), iso_list.is_focus[iso.first]);
        i++;
    }

    glUniform1i(glGetUniformLocation(program, "depthT"), 1);
    glUniform1i(glGetUniformLocation(program, "posT"), 2);
    glUniform1i(glGetUniformLocation(program, "curvT"), 3);
    glUniform1i(glGetUniformLocation(program, "colorT"), 4);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[DEPTH]);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[POSITION]);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[CURVATURE]);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texs[COLOR]);

    glUniform1i(target_loc, 3);
    glUniform1f(lensSize_loc, lensSize);
    glUniform1f(curvature_loc, curvature);
    glUniform1f(trans_strength_loc, transparency);
    glUniform1i(glGetUniformLocation(program, "layers_count"), static_cast<GLint>(iso_list.iso_values.size()));
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
}

void ClearView::renderImage(int newWidth, int newHeight)
{
    if (newWidth <= 0 || newHeight <= 0)   // minimised window
        return;
    if (newWidth != width || newHeight != height) {
        width = newWidth;
        height = newHeight;
        genTextures();
    }

    int layer = 0;
    for (const auto& iso : iso_list.iso_values)
        renderPrep(iso.first, layer++);
    renderComp();
}

void ClearView::renderInterface()
{
    ImGui::Text("Curvature input");
    ImGui::SliderFloat("##Curvature input", &curvature, 0.f, 10.f);
    ImGui::Text("Transparency strength");
    ImGui::SliderFloat("##Transparency strength", &transparency, 0.f, 1.f);
    ImGui::NewLine();
    iso_list.renderInterface();
}
