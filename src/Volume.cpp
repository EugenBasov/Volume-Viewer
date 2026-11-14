#include "Volume.h"

#include <glm/glm.hpp>
#include <lodepng.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <limits>

namespace {

// Highest density in the CT dataset, used for normalisation
constexpr float kMaxDensity = 3273.f;

} // namespace

bool loadVolume(const std::string& prefix, unsigned sliceCount, Volume& volume)
{
    std::vector<std::vector<unsigned char>> slices(sliceCount);
    unsigned width = 0;
    unsigned height = 0;
    for (unsigned i = 0; i < sliceCount; i++) {
        char suffix[16];
        std::snprintf(suffix, sizeof(suffix), "%03u.png", i + 1);
        const std::string filename = prefix + suffix;
        const unsigned error = lodepng::decode(slices[i], width, height, filename, LCT_GREY, 16);
        if (error) {
            std::cerr << "Cannot decode " << filename << ": " << lodepng_error_text(error) << std::endl;
            return false;
        }
    }

    volume.width = width;
    volume.height = height;
    volume.depth = sliceCount;
    const size_t sliceSize = size_t(width) * height;
    volume.data.resize(sliceSize * sliceCount);

    // lodepng returns the 16-bit samples in big-endian byte order
    for (unsigned z = 0; z < sliceCount; z++) {
        const std::vector<unsigned char>& slice = slices[z];
        for (size_t i = 0; i + 1 < slice.size(); i += 2) {
            const unsigned value = (slice[i] << 8) | slice[i + 1];
            volume.data[z * sliceSize + i / 2] = value / kMaxDensity;
        }
    }

    // Range of the gradient magnitude (central differences)
    auto at = [&](unsigned x, unsigned y, unsigned z) {
        return volume.data[z * sliceSize + size_t(y) * width + x];
    };
    float gradientMin = std::numeric_limits<float>::infinity();
    float gradientMax = 0.f;
    for (unsigned z = 1; z + 1 < sliceCount; z++) {
        for (unsigned y = 1; y + 1 < height; y++) {
            for (unsigned x = 1; x + 1 < width; x++) {
                const glm::vec3 next(at(x + 1, y, z), at(x, y + 1, z), at(x, y, z + 1));
                const glm::vec3 prev(at(x - 1, y, z), at(x, y - 1, z), at(x, y, z - 1));
                const float magnitude = glm::length(prev - next) / 2.f;
                gradientMin = std::min(gradientMin, magnitude);
                gradientMax = std::max(gradientMax, magnitude);
            }
        }
    }
    volume.gradientMin = gradientMin;
    volume.gradientMax = gradientMax;

    std::cout << "Loaded volume " << width << " x " << height << " x " << sliceCount << std::endl;
    return true;
}

GLuint createVolumeTexture(const Volume& volume)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, volume.width, volume.height, volume.depth, 0,
                 GL_RED, GL_FLOAT, volume.data.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
}
