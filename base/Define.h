/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 *
 * Define.h
 *
 */

#pragma once

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#define START_FRAME 10
#define MEASURE_FRAME 30
#else
#define START_FRAME 50
#define MEASURE_FRAME 500
#endif
#define MEASURE_START_CAM 0
#define MEASURE_END_CAM 2

#define EVAL_QUALITY 0				//only quaternion camera. F1 to remove imgui. 'o' to start dump

#define UNDERSAMPLING 0						// Should be managed with 3dgs.glsl
#define SIMILARITY_VAR 1

#define MAX_SIMILARITY_VAR 10				// Should be managed with 3dgs.glsl

#define SNAPDRAGON 0

/* kdtree */
#define KDTREE 0
#define USE_TRI_ACCEL 0
#define VERTEX_SIZE 4

/* Debug */
#define STATISTICS 0		// Should be managed with 3dgs.glsl 
#define ENABLE_HIT_COUNTS 0	// Should be managed with 3dgs.glsl
#define DUMP_ICOSAHEDRON 0

#define INIT_COLOR_THRESHOLD 0.15
#define INIT_HIT_THRESHOLD 0.5
#define INIT_WEIGHT_THRESHOLD 0.1
#define INIT_DEPTH_THRESHOLD 1

#define REMOVE_DUPLICATE_ANYHIT_BY_SHADER 1 // Should be managed with 3dgs.glsl

#define DEBUG_TOTAL_ISECTCNT 1	// Should be managed with 3dgs.glsl
#if DEBUG_TOTAL_ISECTCNT
#undef REMOVE_DUPLICATE_ANYHIT_BY_SHADER
#define REMOVE_DUPLICATE_ANYHIT_BY_SHADER 0
#endif

#define WIDTH 800
#define HEIGHT 800

//#define WIDTH 2340
//#define HEIGHT 1080

#define USE_TIME_BASED_FPS false

/* cameras */
#define QUATERNION_CAMERA false
#define LOAD_NERF_CAMERA true
#define DYNAMIC_CAMERA false
#define DYNAMIC_CAMERA_JS 1

#if DYNAMIC_CAMERA_JS
#undef QUATERNION_CAMERA
#define QUATERNION_CAMERA false
#endif

#define ASSET 11
#define LOAD_GLTF 0


#define DEBUG_FILE_PATH "../results/debug/"

#if EVAL_QUALITY
//#define CAMERA_FILE "transforms_test.json"
#define CAMERA_FILE "transforms_val.json"
#else
#define CAMERA_FILE "transforms_val.json"
#endif

#define FOV_Y 39.6f
#define NEAR_PLANE 0.005f
#define FAR_PLANE 20.00f
// quaternion camera
#define CAM_MOVE_SPEED 0.15f
#define CAM_ROTATION_SPEED 0.5f

 // ---------- split blas ---------- //
#define SPLIT_BLAS 0		// This macro should be managed with 3dgs.glsl
#define NUMBER_OF_CELLS_PER_LONGEST_AXIS 10
#define SCENE_EPSILON 1e-4f
#define ONE_VERTEX_BUFFER false

// ---------- compute pipeline ray query ---------- //
#define RAY_QUERY 0
#define TB_SIZE_X 1	// Should be managed with define.glsl
#define TB_SIZE_Y 2	// Should be managed with define.glsl

/* Handle Macro Dependencies */
#if ENABLE_HIT_COUNTS
#undef UNDERSAMPLING
#define UNDERSAMPLING 0
#endif

#if UNDERSAMPLING == 0
#undef SIMILARITY_VAR
#define SIMILARITY_VAR 0
#undef STATISTICS
#define STATISTICS 0
#endif

#if DYNAMIC_CAMERA
#undef QUATERNION_CAMERA
#define QUATERNION_CAMERA false
#endif

#if SPLIT_BLAS || ENABLE_HIT_COUNTS
#undef RAY_QUERY
#define RAY_QUERY 0
#endif

//#define N_IS_UP		// Should be managed with 3DGRT Asset Num.
#define Y_IS_UP

#if ASSET == 0
#define ASSET_PATH "3DGRTModels/hotdog/"
#define PLY_FILE "hotdog2_3dgrt.ply"
#define ASSET_NAME "hotdog"
#elif ASSET == 1
#define ASSET_PATH "3DGRTModels/mic/"
#define PLY_FILE "mic_3dgrt.ply"
#define ASSET_NAME "mic"

#elif ASSET == 2
#define ASSET_PATH "3DGRTModels/ship/"
#define PLY_FILE "ship_3dgrt.ply"
#define ASSET_NAME "ship"
#define KDT_FILE "lego.kdt"
#define GLBIN_FILE "lego.bin"

#elif ASSET == 3
#define ASSET_PATH "3DGRTModels/lego/"
#define PLY_FILE "lego_3dgrt.ply"
#define ASSET_NAME "lego"
#define KDT_FILE "lego.kdt"
#define GLBIN_FILE "lego.bin"

#elif ASSET == 4
#define ASSET_PATH "3DGRTModels/drums/"
#define PLY_FILE "drums_3dgrt.ply"
#define ASSET_NAME "drums"
//#define NO_CAM_DATA

#elif ASSET == 5
#define ASSET_PATH "3DGRTModels/chair/"
#define PLY_FILE "chair_3dgrt.ply"
#define ASSET_NAME "chair"

#elif ASSET == 6
#define ASSET_PATH "3DGRTModels/room/"
//#define PLY_FILE "hotdog_3dgrt2.ply"
#define PLY_FILE "room_3dgrt.ply"
#define ASSET_NAME "room"
#define KDT_FILE "hotdog.kdtbin"
#define GLBIN_FILE "hotdog.glbin"

#elif ASSET == 7
#define ASSET_PATH "3DGRTModels/counter/"
#define PLY_FILE "counter_3dgrt.ply"
#define ASSET_NAME "counter"
//#define NO_CAM_DATA


#elif ASSET == 8
#define ASSET_PATH "3DGRTModels/kitchen/"
//#define PLY_FILE "hotdog_3dgrt2.ply"
#define PLY_FILE "kitchen_3dgrt.ply"
#define ASSET_NAME "kitchen"
#define KDT_FILE "hotdog.kdtbin"
#define GLBIN_FILE "hotdog.glbin"


#elif ASSET == 9
#define ASSET_PATH "3DGRTModels/bonsai/"
//#define PLY_FILE "hotdog_3dgrt2.ply"
#define PLY_FILE "bonsai_3dgrt.ply"
#define ASSET_NAME "bonsai"
#define KDT_FILE "hotdog.kdtbin"
#define GLBIN_FILE "hotdog.glbin"


#elif ASSET == 10
#define ASSET_PATH "3DGRTModels/bicycle/"
//#define PLY_FILE "hotdog_3dgrt2.ply"
#define PLY_FILE "bicycle_3dgrt.ply"
#define ASSET_NAME "bicycle"
#define KDT_FILE "hotdog.kdtbin"
#define GLBIN_FILE "hotdog.glbin"
#elif ASSET == 11
#define ASSET_PATH "3DGRTModels/garden/"
//#define PLY_FILE "hotdog_3dgrt2.ply"
#define PLY_FILE "garden_3dgrt.ply"
#define ASSET_NAME "garden"
#endif

#ifdef NO_CAM_DATA
#undef LOAD_NERF_CAMERA
#define LOAD_NERF_CAMERA false
#endif

#define CUBEMAP_TEXTURE_PATH "cubeMapTextures/blueSky.ktx"

/*** 3DGS ***/
#define NUM_OF_GAUSSIANS 1024	// This macro should be managed with 3dgs.glsl
#define MAX_N_FEATURES 3
#define SPECULAR_DIMENSION 3 * ((MAX_N_FEATURES + 1) * (MAX_N_FEATURES + 1) - 1)
#define KERNEL_DEGREE 4
#define KERNEL_MIN_RESPONSE 0.0113