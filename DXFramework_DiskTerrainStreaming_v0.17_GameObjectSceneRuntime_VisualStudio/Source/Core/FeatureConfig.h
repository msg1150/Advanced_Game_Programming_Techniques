#pragma once
#ifndef DX_ENABLE_ENGINE_RUNTIME
#define DX_ENABLE_ENGINE_RUNTIME 1
#endif
#ifndef DX_ENABLE_ENGINE_GRAPHICS
#define DX_ENABLE_ENGINE_GRAPHICS DX_ENABLE_ENGINE_RUNTIME
#endif
#ifndef DX_ENABLE_ENGINE_COLLISION
#define DX_ENABLE_ENGINE_COLLISION DX_ENABLE_ENGINE_RUNTIME
#endif
#ifndef DX_ENABLE_ENGINE_GAMEPLAY
#define DX_ENABLE_ENGINE_GAMEPLAY DX_ENABLE_ENGINE_RUNTIME
#endif
// Override from CMake or FeatureOptions.props. Optional headers/sources are only
// referenced inside their matching guards; disabled modules can be removed.
#ifndef DX_ENABLE_GPU_PICKING
#define DX_ENABLE_GPU_PICKING 1
#endif
#ifndef DX_ENABLE_SCENE_PERSISTENCE
#define DX_ENABLE_SCENE_PERSISTENCE 1
#endif
#ifndef DX_ENABLE_TERRAIN_QUERY
#define DX_ENABLE_TERRAIN_QUERY 1
#endif
#ifndef DX_ENABLE_TERRAIN_PLACEMENT
#define DX_ENABLE_TERRAIN_PLACEMENT DX_ENABLE_TERRAIN_QUERY
#endif
#if DX_ENABLE_TERRAIN_PLACEMENT && !DX_ENABLE_TERRAIN_QUERY
#error Terrain placement requires a terrain query provider. Disable placement or enable query.
#endif
