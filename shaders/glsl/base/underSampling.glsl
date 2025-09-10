/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */


#define REAL_JACCARD 1


#define LOCAL_SIZE_X 32
#define LOCAL_SIZE_Y 32

#define MAX_SIMILARITY_VAR 10   // Should be managed with Define.h

#define SHARED_MEMORY 1

#define COLOR_SIMILARITY 0      //0 : Simple, 1 : PSNR

#define HIT_SIMILARITY 0.5
#define PSNR_THRESHOLD 30

#if COLOR_SIMILARITY == 0
bool colorSimilarityCheck(vec3 color1, vec3 color2) {
    if (distance(color1, color2) < colorThreshold) {
        return true;
    }
    return false;
}
#elif COLOR_SIMILARITY == 1
vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float calculatePSNR(vec3 color1, vec3 color2) {
    // RGB 값은 0.0 ~ 1.0 범위라고 가정 (GLSL에서는 보통 이렇게 표현)
    float maxVal = 1.0;

    // MSE: Mean Squared Error
    float mse = dot(color1 - color2, color1 - color2) / 3.0;

    // MSE가 0이면 동일한 색 (무한대 방지)
    if (mse == 0.0) {
        return 100.0; // 매우 높은 PSNR (실질적으로 무한대)
    }

    // PSNR 계산
    return 10.0 * log(maxVal * maxVal / mse) / log(10.0); // log base 10
}

bool colorSimilarityCheck(vec3 color1, vec3 color2) {
    float psnr = calculatePSNR(color1, color2);
    if (psnr > PSNR_THRESHOLD)
        return true;
    return false;
}
#endif

uint calcIdx(uvec2 pixel) {
    return pixel.y * width + pixel.x;
}

#if SIMILARITY_VAR
bool hitInfoCheck(uvec2 nearPixel1, uvec2 nearPixel2, uvec2 targetPixel) {
    uint hitCnt1 = validCnt[calcIdx(nearPixel1)];
    uint hitCnt2 = validCnt[calcIdx(nearPixel2)];
    uint cmpCnt = min(MAX_SIMILARITY_VAR, min(hitCnt1, hitCnt2));
    if (cmpCnt == 0) return true;

    uint a[MAX_SIMILARITY_VAR];
    uint b[MAX_SIMILARITY_VAR];
    uint c[MAX_SIMILARITY_VAR];
    uint offset1 = calcIdx(nearPixel1) * MAX_SIMILARITY_VAR;
    uint offset2 = calcIdx(nearPixel2) * MAX_SIMILARITY_VAR;
    uint intersection = 0;
    uint unionCount = 0;

#if REAL_JACCARD
    for (int i = 0; i < hitCnt1; i++) {
        a[i] = id[offset1 + i];
    }
    for (int i = 0; i < hitCnt2; i++) {
        b[i] = id[offset2 + i];
    }
    for (int i = 0; i < hitCnt1; i++) {
        for (int j = 0; j < hitCnt2; j++) {
            if (a[i] == b[j]) {
                c[intersection] = a[i];
                intersection++;
                break;
            }
        }
    }
    unionCount = hitCnt1 + hitCnt2 - intersection;
#else
    for (int i = 0; i < cmpCnt; i++) {
        a[i] = id[offset1 + i];
        b[i] = id[offset2 + i];
    }
    for (int i = 0; i < cmpCnt; i++) {
        for (int j = 0; j < cmpCnt; j++) {
            if (a[i] == b[j]) {
                c[intersection] = a[i];
                intersection++;
                break;
            }
        }
    }
    unionCount = 2 * cmpCnt - intersection;
#endif
    float similarity = (unionCount > 0) ? float(intersection) / float(unionCount) : 0.0;
    similarityVar[offset1 + additionalRT] = similarity;
    if (similarity > hitThreshold) {
        uint offset = calcIdx(targetPixel) * MAX_SIMILARITY_VAR;
        for (int i = 0; i < intersection; i++) {
            id[offset + i] = c[i];
        }
        rayHitCounts.cnts[calcIdx(targetPixel)] = intersection;
        return true;
    }
    return false;
}
#endif

bool depthSimilarityCheck(uvec2 nearPixel1, uvec2 nearPixel2, uvec2 targetPixel) {
    float depth1 = accumDepth[calcIdx(nearPixel1)];
    float depth2 = accumDepth[calcIdx(nearPixel2)];
    if (abs(depth1 - depth2) <= depthThreshold) {
        accumDepth[calcIdx(targetPixel)] = (depth1 + depth2) / 2;
        return true;
    }

    return false;
}

bool similarityCheck(uvec2 nearPixel1, uvec2 nearPixel2, uvec2 targetPixel, out vec4 finalColor) {
    vec4 color1 = imageLoad(image, ivec2(nearPixel1));
    vec4 color2 = imageLoad(image, ivec2(nearPixel2));

    bool colorSimilar = colorSimilarityCheck(color1.xyz, color2.xyz);
    bool similar = colorSimilar;
#if SIMILARITY_VAR
    bool hitInfoSimilar = hitInfoCheck(nearPixel1, nearPixel2, targetPixel);
    similar = similar && hitInfoSimilar;
    bool depthSimilar = depthSimilarityCheck(nearPixel1, nearPixel2, targetPixel);
    similar = similar && depthSimilar;
#endif
    similar = depthSimilar;
    if (similar) finalColor = (color1 + color2) / 2;
    return similar;
}