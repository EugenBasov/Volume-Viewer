#pragma once

#include "IsoLayers.h"

#include <glad/glad.h>

#include <string>

// Illustrative context-preserving volume rendering (Bruckner et al. 2005)
class ICPVR {
public:
    explicit ICPVR(GLuint program);

    void addISO(const std::string& name, float value, glm::vec3 color = glm::vec3(1.f, 0.2f, 0.2f));
    void renderImage(int width, int height);
    void renderInterface();

    float lowerThreshold = 0.223f;  // samples with a lower density are skipped
    float kt = 6.f;                 // interior visibility
    float ks = 0.6f;                // sharpness

private:
    GLuint program;
    ISOList iso_list;

    GLint lowerThreshold_loc;
    GLint kt_loc;
    GLint ks_loc;
    GLint layers_count_loc;
};
