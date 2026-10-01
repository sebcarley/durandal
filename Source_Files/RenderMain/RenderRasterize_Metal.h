/*
	RenderRasterize_Metal.h — Durandal project

	The Metal counterpart of RenderRasterize_Shader. Same visibility,
	sorting and placement (all inherited from RenderRasterizerClass); each
	surface is drawn with the same texture choice, lighting inputs, blend
	and alpha rules as the OpenGL shader renderer, through DurandalMetal.

	Not yet ported (drawn by nothing in this renderer): bloom, bump
	mapping (walls use the plain wall shader), 3D models.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef RENDERRASTERIZE_METAL_H
#define RENDERRASTERIZE_METAL_H

#include "cseries.h"
#include "map.h"
#include "RenderRasterize.h"
#include "OGL_Textures.h"
#include "Rasterizer_Metal.h"
#include "DurandalMetal.h"

#include <memory>

#ifdef HAVE_OPENGL

class RenderRasterize_Metal : public RenderRasterizerClass {

	Rasterizer_Metal_Class *RasPtr = nullptr;

	int objectCount = 0;
	world_distance objectY = 0;
	float weaponFlare = 0;
	float selfLuminosity = 0;

	long_vector2d leftmost_clip, rightmost_clip;

	// Clip planes in world space: left, right (portal window), liquid surface
	simd_float4 clip_planes[3];
	uint32_t clip_mask = 0;

	// Per-frame shader inputs shared by every draw
	DurandalMetal::Uniforms frame_uniforms;
	// Character shadows (Round 12): this frame's dynamic lights, for the
	// shadow direction
	DurandalMetal::Light shadow_lights[DurandalMetal::kMaximumLights];
	int shadow_light_count = 0;
	// Bounced Light (R4): figures take the redistributed light around them
	bool light_bounce = false;
	// Heat shimmer: the lava surfaces drawn this frame, as rectangles in the
	// world view (x0, y0, x1, y1, top-left origin)
	std::vector<simd_float4> hot_rects;
	void note_hot(const DurandalMetal::Vertex* vertices, int count);

	// A surface's shader, colour and blend, as the GL renderer would set them
	struct Material {
		std::unique_ptr<TextureManager> TMgr;
		DurandalMetal::DrawState state;
		DurandalMetal::Uniforms uniforms;
		bool ok = false;
	};

	void set_blend(Material& m, bool void_present, bool tinted = false);
	void draw(Material& m, const DurandalMetal::Vertex* polygon, int count);
	struct LiquidStyle {
		float murk, opacity, waves, caustics, glow;
		float absorb[3];
	};
	static LiquidStyle liquid_style(short type);
	DurandalMetal::VolumeParams volume_params(bool shadows) const;
	void use_ramps(Material& m);
	bool setup_glow(Material& m, float wobble, float offset);
	simd_float4 infravision_colour(short collection) const;

protected:
	virtual void render_node(sorted_node_data *node, bool SeeThruLiquids, RenderStep renderStep);
	virtual void store_endpoint(endpoint_data *endpoint, long_vector2d& p);

	virtual void render_node_floor_or_ceiling(
		  clipping_window_data *window, polygon_data *polygon, horizontal_surface_data *surface,
		  bool void_present, bool ceil, RenderStep renderStep);
	virtual void render_node_side(
		  clipping_window_data *window, vertical_surface_data *surface,
		  bool void_present, RenderStep renderStep);

	virtual void render_node_object(render_object_data *object, bool other_side_of_media, RenderStep renderStep);

	virtual void clip_to_window(clipping_window_data *win);
	virtual void _render_node_object_helper(render_object_data *object, RenderStep renderStep);
	// 3D pickups: a model in place of the object's sprite
	void render_model(render_object_data *object);
	// Character shadows: the sprite's silhouette on its floor, drawn first
	void draw_sprite_shadow(render_object_data *object, Material& m, const DurandalMetal::Vertex* quad);

	void render_viewer_sprite_layer(RenderStep renderStep);
	void render_viewer_sprite(rectangle_definition& RenderRectangle, RenderStep renderStep);

	Material setupWallTexture(const shape_descriptor& Texture, short transferMode, float pulsate, float wobble, float intensity, float offset);
	Material setupSpriteTexture(const rectangle_definition& rect, short type, float offset);

public:
	virtual void setupGL(Rasterizer_Metal_Class& Rasterizer);
	virtual void render_tree(void);
	bool renders_viewer_sprites_in_tree() { return true; }
};

#endif
#endif
