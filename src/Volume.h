#pragma once

#include <glad/glad.h>

#include <string>
#include <vector>

// Scalar volume loaded from a stack of 16-bit greyscale PNG slices.
struct Volume {
    unsigned width = 0;
    unsigned height = 0;
    unsigned depth = 0;
    std::vector<float> data;    // densities normalised to [0, 1], slice after slice

    // Range of the gradient magnitude, used to normalise it in the shader
    float gradientMin = 0.f;
    float gradientMax = 0.f;
};

// Loads the slices "<prefix>001.png" ... "<prefix><sliceCount>.png".
bool loadVolume(const std::string& prefix, unsigned sliceCount, Volume& volume);

// Uploads the volume into a single-channel float 3D texture.
GLuint createVolumeTexture(const Volume& volume);
