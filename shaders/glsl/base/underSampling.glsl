/*
 * Abura Soba, 2025
 *
 * 3dgs.glsl
 *
 */

#define LOCAL_SIZE_X 32
#define LOCAL_SIZE_Y 32

#define MAX_SIMILARITY_VAR 30

#define SHARED_MEMORY 1

#define COLOR_SIMILARITY 0      //0 : Simple, 1 : PSNR

#define HIT_SIMILARITY 0.5
#define PSNR_THRESHOLD 30