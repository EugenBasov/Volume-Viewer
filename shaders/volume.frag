#version 330 core

// Volume ray casting, one ray per pixel. technique: false = ICPVR, true = ClearView
// ClearView passes (target): 0 first hit, 1 shading, 2 curvature, 3 composition

in vec2 uv;
layout(location = 0) out vec4 outColor0;
layout(location = 1) out vec3 outColor1;
layout(location = 2) out float outColor2;

const int MAX_LAYERS = 6;

// Volume
uniform sampler3D volumeTexture;
uniform vec3 cellSize;
uniform float gradientMagnitude_min;
uniform float gradientMagnitude_max;

// Camera
uniform vec2 resolution;
uniform float aspectRatio;
uniform vec3 cameraPos;
uniform vec3 cameraRight;
uniform float cameraRad;
uniform mat3 cameraDir;

uniform bool technique;

// Iso-surface layers
uniform int layers_count;
uniform float iso_v[MAX_LAYERS];
uniform bool is_focus[MAX_LAYERS];
uniform vec3 iso_c[MAX_LAYERS];

// ICPVR
uniform float lowerThreshold;
uniform float kt;   // interior visibility
uniform float ks;   // sharpness

// ClearView: preparation passes (one iso-surface layer at a time)
uniform int target;
uniform int iteration;  // index of the rendered layer
uniform float iso_val;
uniform vec3 iso_col;
uniform sampler2DArray positionTex;
uniform sampler2DArray normalTex;
uniform float curvature;

// ClearView: composition pass
uniform sampler2DArray depthT;
uniform sampler2DArray posT;
uniform sampler2DArray curvT;
uniform sampler2DArray colorT;
uniform vec2 cursorPos;
uniform float lensSize;
uniform float trans_strength;

// The volume occupies a unit cube centred at the origin
const vec3 volumeMin = vec3(-0.5);
const vec3 volumeMax = vec3(0.5);

float max3 (vec3 v) {
  return max (max (v.x, v.y), v.z);
}

float min3 (vec3 v) {
  return min (min (v.x, v.y), v.z);
}

float modfl(float a, float m){
    return a - floor(a / m) * m;
}

vec3 modfvl(vec3 a, vec3 m){
    vec3 res;
    res.x = modfl(a.x, m.x);
    res.y = modfl(a.y, m.y);
    res.z = modfl(a.z, m.z);
    return res;
}

// Opacity transfer function
float alpha_tf(float samp){
    return samp;
}

// Colour transfer function (layers are sorted by iso-value)
vec3 color_tf(float samp){
    if(samp < iso_v[0])
        return iso_c[0];
    for(int i = 1; i < layers_count; i++){
        if(samp <= iso_v[i])
            return iso_c[i];
    }
    return iso_c[layers_count-1];
}

// Gradient by central differences (points from dense to less dense material)
vec3 grad(vec3 pos){
    float samp_x = texture(volumeTexture, pos+vec3(cellSize.x,0,0)).r;
    float samp_y = texture(volumeTexture, pos+vec3(0,cellSize.y,0)).r;
    float samp_z = texture(volumeTexture, pos+vec3(0,0,cellSize.z)).r;
    float samp_xn = texture(volumeTexture, pos-vec3(cellSize.x,0,0)).r;
    float samp_yn = texture(volumeTexture, pos-vec3(0,cellSize.y,0)).r;
    float samp_zn = texture(volumeTexture, pos-vec3(0,0,cellSize.z)).r;
    return ((vec3(samp_xn,samp_yn,samp_zn)) - (vec3(samp_x,samp_y,samp_z))) / 2.0;
}

vec3 grad_n(vec3 pos){
    return normalize(grad(pos));
}

// Gradient magnitude normalised to [0, 1] over the whole volume
float grad_mag_n(vec3 pos){
    float grad_mag = length(grad(pos));
    return (grad_mag - gradientMagnitude_min) / (gradientMagnitude_max - gradientMagnitude_min);
}

// Simple headlight shading
float shading_coef(vec3 rayDir, vec3 norm){
    return clamp(dot(-rayDir, normalize(norm)), 0.2, 1);
}

// Distance from the eye, roughly normalised to [0, 1] across the volume
float dist_n(vec3 camPos, vec3 pos){
    float dist = length(pos - camPos);
    return (dist - 0.3) / ((length(camPos)+0.866*2) - 0.3);
}

float ICPVR_opacity(vec3 camPos, vec3 rayDir, vec3 pos, float samp, float alpha_acc, float kt, float ks){
    return alpha_tf(samp) * pow(grad_mag_n(pos), pow((kt * shading_coef(rayDir, grad(pos)) * (1.0 - dist_n(camPos, pos)) * (1.0 - alpha_acc)), ks));
}

void main() {
    // View ray in world space (60 degree horizontal field of view)
    float half_width = 0.5773;
    float half_height = half_width * aspectRatio;
    vec3 res_vec = vec3(half_width, half_height, 1.0);
    vec3 rayDirUV = vec3(uv * 2.0 - 1.0, -1.0);
    vec3 rayDir = normalize(rayDirUV * res_vec);
    rayDir = cameraDir * rayDir;
    vec3 rayDir_o = rayDir;
    // Shift the eye to the right so the volume is not hidden behind the UI panel
    vec3 rayOrig = cameraPos.xyz + cameraRight * 0.12 * cameraRad;
    vec3 rayOrig_o = rayOrig;

    // Mirror the ray into the positive octant, a = flipped axes
    vec3 a = vec3(0);
    if(rayDir.x < 0){
        a.x = 1;
        rayOrig.x *= -1;
        rayDir.x *= -1;
    }
    if(rayDir.y < 0){
        a.y = 1;
        rayOrig.y *= -1;
        rayDir.y *= -1;
    }
    if(rayDir.z < 0){
        a.z = 1;
        rayOrig.z *= -1;
        rayDir.z *= -1;
    }

    // Ray / bounding box intersection, the ray misses the volume -> white background
    float tmin, tmax;
    vec3 tminv = (volumeMin - rayOrig) / rayDir;
    vec3 tmaxv = (volumeMax - rayOrig) / rayDir;
    tmin = max3(tminv);
    tmax = min3(tmaxv);
    if(tmin >= tmax){
        outColor0 = vec4(1);
        return;
    }

    // The ray advances from one voxel boundary to the next
    vec3 pos = rayOrig + tmin * rayDir;
    vec3 remains = modfvl(pos,cellSize);
    if(remains.x < 0.01) remains.x = cellSize.x;
    if(remains.y < 0.01) remains.y = cellSize.y;
    if(remains.z < 0.01) remains.z = cellSize.z;
    float t_step = min3(remains / rayDir);

    vec3 accumulatedColor = vec3(0.0);
    float accumulatedOpacity = 0.0;

    if(!technique){
        // ICPVR: front-to-back compositing
        for (float t = tmin; t < tmax; t += t_step) {
            vec3 texCoords = (a - (2*a-1) * ((pos - volumeMin) / (volumeMax - volumeMin)));
            float samp = texture(volumeTexture, texCoords.xyz).r;

            if(samp > lowerThreshold && t > 0){
                vec3 color = color_tf(samp) * shading_coef(rayDir_o, grad_n(texCoords));
                float opacity = ICPVR_opacity(rayOrig_o, rayDir_o, texCoords, 1, accumulatedOpacity, kt, ks);
                accumulatedColor = accumulatedColor + color * opacity;
                accumulatedOpacity = accumulatedOpacity + (1.0 - accumulatedOpacity) * opacity;
            }

            // Early ray termination
            if (accumulatedOpacity >= 0.95)
                break;

            pos += t_step * rayDir;
            vec3 remains = modfvl(pos,cellSize);
            if(remains.x < 0.01) remains.x = cellSize.x;
            if(remains.y < 0.01) remains.y = cellSize.y;
            if(remains.z < 0.01) remains.z = cellSize.z;
            t_step = min3(remains / rayDir);
        }
        outColor0 = vec4(accumulatedColor + (1.0-accumulatedOpacity) * vec3(1), 1);
        return;
    }
    else{
    switch(target){
        case 0:
            // First hit of the iso-surface: position (in texture space) and depth
            float alpha = 0;
            float depth = 1000000.0;

            for (float t = tmin; t < tmax; t += t_step) {
                vec3 texCoords = (a - (2*a-1) * ((pos - volumeMin) / (volumeMax - volumeMin)));
                float samp = texture(volumeTexture, texCoords.xyz).r;
                if(samp > iso_val && t > 0){
                    accumulatedColor = texCoords;
                    alpha = 1;
                    depth = t;
                    break;
                }

                pos += t_step * rayDir;
                vec3 remains = modfvl(pos,cellSize);
                if(remains.x < 0.01) remains.x = cellSize.x;
                if(remains.y < 0.01) remains.y = cellSize.y;
                if(remains.z < 0.01) remains.z = cellSize.z;
                t_step = min3(remains / rayDir);
            }

            outColor0 = vec4(accumulatedColor, alpha);
            outColor2 = depth;
            return;
        case 1:
            // Normal and shaded colour at the hit position
            vec4 pos_t = texture(positionTex, vec3(uv, iteration));
            vec3 norm;
            vec4 color;
            if(pos_t.w != 0){
                norm = grad_n(pos_t.xyz);
                color = vec4(iso_col * shading_coef(rayDir_o, norm), 1);
            }
            else{
                norm = grad_n(pos_t.xyz);
                color = vec4(1,1,1,0);
            }
            norm = (norm + 1.0) * 0.5;
            outColor1 = norm;
            outColor0 = color;
            return;
        case 2:
            // Curvature: how much the normal changes between neighbouring pixels
            vec3 up = texture(normalTex, vec3((uv + vec2(0,1.0/resolution.y)), iteration)).xyz * 2.0 - 1;
            vec3 down = texture(normalTex, vec3((uv - vec2(0,1.0/resolution.y)), iteration)).xyz * 2.0 - 1;
            vec3 right = texture(normalTex, vec3((uv + vec2(1.0/resolution.x,0)), iteration)).xyz * 2.0 - 1;
            vec3 left = texture(normalTex, vec3((uv - vec2(1.0/resolution.x,0)), iteration)).xyz * 2.0 - 1;
            vec3 center = texture(normalTex, vec3(uv, iteration)).xyz * 2.0 - 1;
            outColor2 = (length(center - up) + length(center - down) + length(center - right) + length(center - left)) * (curvature / 5.0);
            return;
        case 3:
            // Composition: blend layers back to front, non-focus layers fade inside the lens
            int order[MAX_LAYERS];
            for(int i = 0; i < MAX_LAYERS; i++){
                order[i] = i;
            }
            for(int i = 0; i < layers_count-1; i++){
                for(int j = i + 1; j < layers_count; j++){
                    float depthDiff = texture(depthT, vec3(uv, order[i])).r - texture(depthT, vec3(uv, order[j])).r;
                    bool swapLayers = (abs(depthDiff) <= 0.01) ? (iso_v[order[i]] < iso_v[order[j]])
                                                               : (depthDiff < -0.01);
                    if(swapLayers){
                        int tmp = order[i];
                        order[i] = order[j];
                        order[j] = tmp;
                    }
                }
            }
            vec3 cursPos = texture(posT, vec3(cursorPos, order[layers_count-1])).rgb;
            vec3 border = vec3(1);
            outColor0 = vec4(1,1,1,0);
            float dist;
            for(int i = 0; i < layers_count; i++){
                vec3 pixPos = texture(posT, vec3(uv, order[i])).rgb;
                dist = length(cursPos - pixPos);
                if(i == layers_count - 1 && dist / lensSize > 0.98 && dist / lensSize <= 1.0)
                    border = vec3(0.1);
                float trans = clamp(max(dist / lensSize, texture(curvT, vec3(uv,order[i])).r), 0.0, 1.0);

                vec4 texCol = texture(colorT, vec3(uv,order[i]));
                vec3 color = texCol.rgb * border;
                float opacity = texCol.w;
                if(!is_focus[order[i]]){
                    opacity *= trans;
                }
                if(outColor0.w < 0.01)
                    outColor0 = vec4(color * clamp(opacity - trans_strength + 1,0,1) + vec3(1) * (1-clamp(opacity - trans_strength + 1,0,1)), opacity);
                else
                    outColor0 = vec4(outColor0.rgb * (1 - opacity) + color * opacity, opacity);
            }
            return;
        default:
            outColor2 = 0.1;
            outColor1 = vec3(0,0,1);
            outColor0 = vec4(0.7,0.1,0.1,1.0);
            return;
    }
    }
}
