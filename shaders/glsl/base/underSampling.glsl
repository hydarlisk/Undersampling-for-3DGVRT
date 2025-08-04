/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define LOCAL_SIZE_X 32
#define LOCAL_SIZE_Y 32

#define MAX_SIMILARITY_VAR 150

#define SHARED_MEMORY 1

#define COLOR_SIMILARITY 0      //0 : Simple, 1 : PSNR

#define PSNR_THRESHOLD 30

#if COLOR_SIMILARITY == 0
bool colorSimilarityCheck(vec3 color1, vec3 color2) {
    if (distance(color1, color2) < colorThreshold) {
        return true;
    }
    return false;
}
#elif COLOR_SIMILARITY == 1
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
//bool hitInfoCheck(uvec2 nearPixel1, uvec2 nearPixel2) {
//    uint hitCnt1 = rayHitCounts.cnts[calcIdx(nearPixel1)];
//    uint hitCnt2 = rayHitCounts.cnts[calcIdx(nearPixel2)];
//    
//    uint minCnt = min(MAX_SIMILARITY_VAR, min(hitCnt1, hitCnt2));
//    uint 
//    for (int i = 0; i < minCnt; i++) {
//        if () {}
//    }
//}
#endif

bool similarityCheck(uvec2 nearPixel1, uvec2 nearPixel2, out vec4 finalColor) {
    vec4 color1 = imageLoad(image, ivec2(nearPixel1));
    vec4 color2 = imageLoad(image, ivec2(nearPixel2));

    bool similar = colorSimilarityCheck(color1.xyz, color2.xyz);
    if (similar) finalColor = (color1 + color2) / 2;
    return similar;
}