/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define LOCAL_SIZE_X 32
#define LOCAL_SIZE_Y 32

#define SHARED_MEMORY 1


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