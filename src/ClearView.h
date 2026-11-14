#pragma once

#include "IsoLayers.h"

#include <glad/glad.h>

#include <string>
#include <vector>

// ClearView (Krüger et al. 2006): iso-surfaces with a focus lens
class ClearView {
public:
    explicit ClearView(GLuint program);

    void addISO(const std::string& name, float value, glm::vec3 color = glm::vec3(1.f, 0.2f, 0.2f));
    void renderImage(int width, int height);
    void renderInterface();

    static void setLensSize(float size) { lensSize = size; }
    static float getLensSize() { return lensSize; }

private:
    enum TEX { DEPTH, POSITION, NORMAL, CURVATURE, COLOR, TEXTURES_N };

    void genTextures();
    void renderPrep(const std::string& name, int layer);
    void renderComp();

    static float lensSize;

    GLuint program;
    GLuint framebuffer = 0;
    std::vector<GLuint> texs;   // one texture array per TEX, one array layer per iso-surface
    int width = 0;
    int height = 0;

    GLint target_loc;
    GLint isoVal_loc;
    GLint isoCol_loc;
    GLint iteration_loc;
    GLint posTex_loc;
    GLint normTex_loc;
    GLint lensSize_loc;
    GLint curvature_loc;
    GLint trans_strength_loc;

    ISOList iso_list;
    float curvature = 1.f;
    float transparency = 0.5f;
};
