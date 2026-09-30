/*
	RenderRasterize_Metal.cpp — Durandal project

	Port of RenderRasterize_Shader.cpp to DurandalMetal. Function by
	function it follows the OpenGL shader renderer: the same texture set-up,
	colours, uniforms, blend and alpha-test rules, clip planes and depth
	handling, so the two can be compared side by side. Comments marked
	"GL:" note what the OpenGL original does at that point.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "OGL_Headers.h"

#include "RenderRasterize_Metal.h"

#include "lightsource.h"
#include "media.h"
#include "player.h"
#include "weapons.h"
#include "AnimatedTextures.h"
#include "OGL_Render.h"
#include "OGL_Setup.h"
#include "OGL_Model_Def.h"
#include "ViewControl.h"
#include "Logging.h"
#include "screen.h"
#include "DurandalPreferences.h"
#include "DurandalLights.h"
#include "DurandalRadiance.h"
#include "DurandalGL.h"
#include "DurandalBenchmark.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef HAVE_OPENGL

using namespace DurandalMetal;
using namespace DurandalMatrix;

namespace {

const double TWO_PI = 8 * atan(1.0);
const float FixedAngleToRadians = TWO_PI / (float(FIXED_ONE) * float(FULL_CIRCLE));
const float FixedAngleToDegrees = 360.0 / (float(FIXED_ONE) * float(FULL_CIRCLE));
const double Radian2Circle = 1 / TWO_PI;
const double FullCircleReciprocal = 1 / double(FULL_CIRCLE);

bool uses_wall_vertex_shader(Program p)
{
	return p == kWall || p == kWallInfravision || p == kWallRamp;
}

}

// Shared with the shader renderer (RenderRasterize_Shader.cpp): texture
// slides and wobble, including Durandal's smoothing.
void instantiate_transfer_mode(struct view_data *view, short transfer_mode, world_distance &x0, world_distance &y0);
float calcWobble(short transferMode, short transfer_phase);
float durandal_calc_wobble_for(const view_data* view, short transferMode);

extern GLdouble Screen_2_Clip[16];
extern void position_sprite_axis(short *x0, short *x1, short scale_width, short screen_width, short positioning_mode, _fixed position, bool flip, world_distance world_left, world_distance world_right);

// Liquids (W1): each kind's look. Murk: distance (world units) over which
// the view under the surface fades, scaled per channel by absorb; opacity
// of the surface texture; wave strength; caustics on what is below; glow.
RenderRasterize_Metal::LiquidStyle RenderRasterize_Metal::liquid_style(short type)
{
	switch (type) {
		case _media_water:  return { 1000, 0.25f, 1.00f, 0.35f, 0.0f, {1.6f, 0.8f, 0.6f} };
		case _media_lava:   return { 1, 1.00f, 0.25f, 0.00f, 0.8f, {1.0f, 1.0f, 1.0f} };
		case _media_goo:    return { 300, 0.70f, 0.40f, 0.10f, 0.0f, {0.8f, 1.4f, 0.7f} };
		case _media_sewage: return { 350, 0.65f, 0.50f, 0.12f, 0.0f, {1.0f, 0.9f, 1.6f} };
		case _media_jjaro:  return { 1200, 0.30f, 0.80f, 0.30f, 0.0f, {1.4f, 0.7f, 0.9f} };
		default:            return { 1200, 0.30f, 0.60f, 0.20f, 0.0f, {1.0f, 1.0f, 1.0f} };
	}
}

// Volumetric fog (V1): each environment's haze. Colour is what the haze
// shows in full light (it takes on each room's light level); density per
// world unit; dust breaks it into drifting patches (0 flat, 1 fully);
// mist thickens it near the floor.
// Under a liquid, its murk.
VolumeParams RenderRasterize_Metal::volume_params(bool shadows) const
{
	struct Haze { float r, g, b, density, dust, mist; };
	Haze haze;
	switch (static_world->environment_code) {
		case 1:  haze = { 0.80f, 0.48f, 0.28f, 0.012f, 1.0f, 1.6f }; break;	// lava
		case 2:  haze = { 0.52f, 0.62f, 0.40f, 0.010f, 1.0f, 1.8f }; break;	// sewage
		case 3:  haze = { 0.56f, 0.52f, 0.70f, 0.005f, 0.9f, 1.2f }; break;	// Jjaro
		case 4:  haze = { 0.52f, 0.62f, 0.56f, 0.0075f, 0.9f, 1.2f }; break;	// Pfhor
		default: haze = { 0.48f, 0.56f, 0.68f, 0.005f, 0.9f, 1.5f }; break;	// water
	}
	// Fog Strength: light, medium, thick
	static const float strength[3] = { 0.55f, 1.0f, 1.8f };
	haze.density *= strength[std::clamp(Durandal::Prefs().fog_strength, 0, 2)];
	VolumeParams p = {};
	p.inverse = simd_inverse(simd_mul(RasPtr->projection, RasPtr->modelview));
	p.camera = simd_make_float4(view->origin.x, view->origin.y, view->origin.z, 1);
	p.air = simd_make_float4(haze.r * 0.75f, haze.g * 0.75f, haze.b * 0.75f, haze.density / WORLD_ONE);
	p.murk = simd_make_float4(0, 0, 0, 0);
	p.viewer_under = 0;
	if (view->under_media_boundary && view->under_media_index != NONE) {
		if (const media_data* media = get_media_data(view->under_media_index)) {
			struct Murk { float r, g, b, visibility; };	// visibility in world units
			Murk m;
			switch (media->type) {
				case _media_water:  m = { 0.16f, 0.34f, 0.42f, 2.0f }; break;
				case _media_lava:   m = { 0.90f, 0.35f, 0.08f, 0.2f }; break;
				case _media_goo:    m = { 0.26f, 0.38f, 0.10f, 0.6f }; break;
				case _media_sewage: m = { 0.28f, 0.34f, 0.18f, 0.7f }; break;
				case _media_jjaro:  m = { 0.28f, 0.22f, 0.38f, 1.8f }; break;
				default:            m = { 0.20f, 0.30f, 0.35f, 1.5f }; break;
			}
			p.murk = simd_make_float4(m.r, m.g, m.b, 1.0f / (m.visibility * WORLD_ONE));
			p.viewer_under = 1;
		}
	}
	p.polygon = view->origin_polygon_index;
	p.shadows = shadows ? 1 : 0;
	p.time = view->tick_count / float(TICKS_PER_SECOND);
	p.dust = haze.dust;
	p.light_scale = 0.8f;
	p.mist = haze.mist;
	return p;
}

void RenderRasterize_Metal::setupGL(Rasterizer_Metal_Class& Rasterizer)
{
	RasPtr = &Rasterizer;
}

simd_float4 RenderRasterize_Metal::infravision_colour(short collection) const
{
	GLfloat color[4] = {1, 1, 1, 1};
	FindInfravisionVersionRGBA(collection, color);
	return simd_make_float4(color[0], color[1], color[2], 1);
}

void RenderRasterize_Metal::render_tree()
{
	weaponFlare = PIN(view->maximum_depth_intensity - NATURAL_LIGHT_INTENSITY, 0, FIXED_ONE) / float(FIXED_ONE);
	selfLuminosity = PIN(NATURAL_LIGHT_INTENSITY, 0, FIXED_ONE) / float(FIXED_ONE);

	Uniforms& u = frame_uniforms;
	u = Uniforms{};
	u.projection = RasPtr->projection;
	u.modelview = RasPtr->modelview;
	u.texture_matrix = identity();
	u.model = identity();
	u.color = simd_make_float4(1, 1, 1, 1);
	u.alpha_threshold = -1;
	u.distance_mode = 1;
	u.viewer_light = simd_make_float4(0, 0, 0, 0);
	u.figure_patches = simd_make_int4(-1, -1, 0, 0);
	u.visibility = 1;
	u.time = view->tick_count;
	const float pixel_height = view->screen_height * MainScreenPixelScale();
	u.static_block = std::max(1.f, std::round(pixel_height / 320.f));
	u.shading_style = Durandal::Prefs().shading_style;
	u.texel_lighting = Durandal::Enabled(Durandal::kTexelLighting) ? 1 : 0;
	u.filtering = Durandal::Enabled(Durandal::kCrispFiltering) ? 1 : 0;
	u.write_glow = GlowActive() ? 1 : 0;
	u.emissive = 0;
	u.sky = Durandal::Enabled(Durandal::kHDRSky) ? 3 : 0;
	u.camera = simd_make_float4(view->origin.x, view->origin.y, view->origin.z, 1);
	// Ambient shadows (Round 12): the view for reconstructing positions
	{
		const simd_float4x4 vp = simd_mul(RasPtr->projection, RasPtr->modelview);
		SetView(vp, simd_inverse(vp), u.camera);
		SetViewerUnderLiquid(view->under_media_boundary);
	}
	u.caustics = 0;
	u.liquid = simd_make_float4(0, 0, 0, 0);
	// Liquids (W1): the view wavers under a liquid
	DurandalGL::SetWorldDistortion(Durandal::Enabled(Durandal::kLiquids) && view->under_media_boundary ? 1.0f : 0.0f,
								   view->tick_count / float(TICKS_PER_SECOND));

	// Dynamic lights (E2) from the visible polygons' objects
	DurandalMetal::Light dynamic_lights[kMaximumLights];
	int light_count = 0;
	if (Durandal::Enabled(Durandal::kDynamicLights))
		light_count = DurandalLights::Gather(view, RSPtr->SortedNodes, weaponFlare, dynamic_lights);
	SetLights(dynamic_lights, light_count);
	u.light_count = light_count;
	std::copy(dynamic_lights, dynamic_lights + light_count, shadow_lights);
	shadow_light_count = light_count;
	// Light shadows (E3): the map, walked from each lit point to the light;
	// volumetric fog (V1) walks it too
	const bool volumetric = Durandal::Enabled(Durandal::kVolumetricFog);
	const bool redistribution = Durandal::Enabled(Durandal::kLightRedistribution);
	const bool shadows = light_count && Durandal::Enabled(Durandal::kLightShadows);
	u.shadows = 0;
	if (shadows || volumetric || redistribution) {
		static std::vector<simd_float4> map;
		SetMap(map.data(), DurandalLights::BuildMap(map));
		u.shadows = shadows ? 1 : 0;
	}

	// Contact shadows under items, monsters and scenery
	DurandalMetal::Caster casters[kMaximumCasters];
	u.caster_count = Durandal::Enabled(Durandal::kContactShadows)
		? DurandalLights::GatherCasters(view, RSPtr->SortedNodes, casters) : 0;
	SetCasters(casters, u.caster_count);
	DurandalBenchmark::FrameCounts(light_count, u.caster_count);	// development timing

	// GL: gl_Fog is set by OGL_StartMain; U_FogMode = -1 when fog is off
	u.fog_mode = -1;
	if (OGL_FogData* fog = OGL_GetCurrFogData())
	{
		u.fog_mode = fog->Mode;
		const GLfloat* c = OGL_GetCurrFogColor();
		u.fog_color = simd_make_float4(c[0], c[1], c[2], c[3]);
		if (fog->Mode == OGL_Fog_Linear)
		{
			u.fog_start = WORLD_ONE * fog->Start;
			u.fog_end = WORLD_ONE * (fog->Start + fog->Depth);
		}
		else
		{
			u.fog_density = 1.0f / std::max(1.f, WORLD_ONE * fog->Depth);
		}
		if (fog->IsPresent && fog->AffectsLandscapes)
			u.fog_mix = fog->LandscapeMix;
	}

	// Light redistribution (E4): bake some of the surface cache, before
	// any draw. Each surface's light may drop to 60% or rise to 135% of
	// its own brightness, with a little colour bleed
	u.patch = -1;
	// Redistribution Strength (Round 12): how far a surface's light may
	// fall below or rise above its authored level, and the colour bleed
	switch (std::clamp(Durandal::Prefs().gi_strength, 0, 2)) {
		case 0: u.gi_range = simd_make_float4(0.25f, 0.2f, 0.4f, 0.08f); break;
		case 2: u.gi_range = simd_make_float4(0.55f, 0.8f, 0.6f, 0.18f); break;
		default: u.gi_range = simd_make_float4(0.4f, 0.35f, 0.5f, 0.12f); break;
	}
	// Bounced Light (R4, Rampant): the bake passes light on as it is drawn,
	// and figures take the light of the floor and ceiling around them
	light_bounce = redistribution && Durandal::Enabled(Durandal::kLightBounce);
	u.gi = redistribution && DurandalRadiance::Frame(view, RSPtr->SortedNodes, u.gi_range, light_bounce) ? 1 : 0;
	// Surface relief (M1): on the 8-bit shading path
	u.relief = Durandal::Enabled(Durandal::kSurfaceRelief) ? 1.0f : 0.0f;

	// Volumetric fog (V1): the haze this frame, built before any draw
	u.volume = 0;
	if (volumetric) {
		VolumeParams p = volume_params(shadows);
		// MML fog above liquids, when a scenario has it, becomes the haze
		OGL_FogData* fog = OGL_GetCurrFogData();
		if (fog && fog->IsPresent && !view->under_media_boundary) {
			const GLfloat* c = OGL_GetCurrFogColor();
			p.air = simd_make_float4(c[0], c[1], c[2], 1.0f / std::max(1.f, WORLD_ONE * fog->Depth));
			u.fog_mode = -1;
			u.fog_mix = 0;
		}
		simd_float4 screen;
		if (RunVolume(p, screen)) {
			u.volume = 1;
			u.volume_screen = screen;
		}
	}

	u.yaw = view->virtual_yaw * FixedAngleToRadians;
	u.pitch = view->mimic_sw_perspective ? 0.0 : view->virtual_pitch * FixedAngleToRadians;
	u.roll = view->mimic_sw_perspective ? 0.0 : view->durandal_roll * (M_PI / 180.0);	// True Look: Sidestep Sway

	short leftmost = INT16_MAX;
	short rightmost = INT16_MIN;
	vector<clipping_window_data>& windows = RSPtr->RVPtr->ClippingWindows;
	for (auto it = windows.begin(); it != windows.end(); ++it) {
		if (it->x0 < leftmost) {
			leftmost = it->x0;
			leftmost_clip = it->left;
		}
		if (it->x1 > rightmost) {
			rightmost = it->x1;
			rightmost_clip = it->right;
		}
	}
	clip_mask = 0;

	// GL also renders a kGlow pass for bloom; not ported yet.
	RenderRasterizerClass::render_tree(kDiffuse);
	render_viewer_sprite_layer(kDiffuse);
}

void RenderRasterize_Metal::render_node(sorted_node_data *node, bool SeeThruLiquids, RenderStep renderStep)
{
	objectCount = 0;
	objectY = 0;
	frame_uniforms.polygon = node->polygon_index;	// light shadows (E3)
	// Liquids (W1): caustics on what is under this polygon's liquid
	frame_uniforms.caustics = 0;
	if (Durandal::Enabled(Durandal::kLiquids)) {
		const polygon_data* polygon = get_polygon_data(node->polygon_index);
		if (polygon->media_index != NONE) {
			const media_data* media = get_media_data(polygon->media_index);
			frame_uniforms.caustics = liquid_style(media->type).caustics;
			frame_uniforms.media_height = media->height;
		}
	}
	RenderRasterizerClass::render_node(node, SeeThruLiquids, renderStep);
	// GL: glDisable(GL_CLIP_PLANE0/1)
	clip_mask &= ~3u;
}

void RenderRasterize_Metal::clip_to_window(clipping_window_data *win)
{
	// GL: planes (i, j, 0, 0) specified in a frame translated to the view
	// origin and rotated by yaw + 90 degrees, less 0.1 for the left plane
	// and plus 0.1 for the right, for slack at the edges. In world space
	// that is the rotated normal through the view origin.
	const double base = view->yaw * (360.0 / FULL_CIRCLE) + 90.0;
	auto plane = [this](const long_vector2d& n, double degrees) {
		const double a = degrees * M_PI / 180.0;
		const double nx = std::cos(a) * n.i - std::sin(a) * n.j;
		const double ny = std::sin(a) * n.i + std::cos(a) * n.j;
		const double d = -(nx * view->origin.x + ny * view->origin.y);
		return simd_make_float4(nx, ny, 0, d);
	};

	if (win->left.i != leftmost_clip.i || win->left.j != leftmost_clip.j) {
		clip_planes[0] = plane(win->left, base - 0.1);
		clip_mask |= 1u;
	} else {
		clip_mask &= ~1u;
	}

	if (win->right.i != rightmost_clip.i || win->right.j != rightmost_clip.j) {
		clip_planes[1] = plane(win->right, base + 0.1);
		clip_mask |= 2u;
	} else {
		clip_mask &= ~2u;
	}
}

void RenderRasterize_Metal::store_endpoint(endpoint_data *endpoint, long_vector2d& p)
{
	p.i = endpoint->vertex.x;
	p.j = endpoint->vertex.y;
}

void RenderRasterize_Metal::set_blend(Material& m, bool void_present, bool tinted)
{
	if (m.TMgr->IsBlended() || tinted) {
		m.state.blend = static_cast<Blend>(m.TMgr->NormalBlend());
		m.uniforms.alpha_threshold = 0.001f;
	} else {
		m.state.blend = kBlendNone;
		m.uniforms.alpha_threshold = 0.5f;
	}
	if (void_present && m.TMgr->IsBlended()) {
		m.state.blend = kBlendNone;
		m.uniforms.alpha_threshold = -1;
	}
}

void RenderRasterize_Metal::draw(Material& m, const Vertex* polygon, int count)
{
	if (count < 3)
		return;
	Uniforms u = m.uniforms;
	if (!uses_wall_vertex_shader(m.state.program)) {
		// GL: sprite.vert and landscape.vert have no depth nudge or TBN
		u.depth_offset = 0;
		u.normal = simd_make_float4(0, 0, 0, 0);
	}
	// Round 12: additive draws (glows, explosions) are light, not surfaces:
	// they never write the distance image
	if (m.state.blend == kBlendAdd || m.state.blend == kBlendAddPremultiplied)
		u.distance_mode = 0;
	u.clip_mask = clip_mask;
	for (int i = 0; i < 3; ++i)
		u.clip_planes[i] = clip_planes[i];

	// GL_POLYGON / GL_QUADS -> triangle fan
	Vertex triangles[3 * (MAXIMUM_VERTICES_PER_POLYGON + 4)];
	int n = 0;
	for (int i = 1; i + 1 < count && n + 3 <= int(sizeof(triangles) / sizeof(Vertex)); ++i) {
		triangles[n++] = polygon[0];
		triangles[n++] = polygon[i];
		triangles[n++] = polygon[i + 1];
	}
	Draw(m.state, u, triangles, n);
}

RenderRasterize_Metal::Material RenderRasterize_Metal::setupSpriteTexture(const rectangle_definition& rect, short type, float offset)
{
	Material m;
	m.uniforms = frame_uniforms;
	auto& TMgr = m.TMgr;
	TMgr = std::make_unique<TextureManager>();

	float shade = PIN(static_cast<GLfloat>(rect.ambient_shade) / static_cast<GLfloat>(FIXED_ONE), 0, 1);
	// Sprite lighting (S1): as the software renderer, sprites take their
	// polygon's floor light; with the switch on, a bright ceiling above
	// them lights them too (never darker than the floor alone)
	if (type == OGL_Txtr_Inhabitant && Durandal::Enabled(Durandal::kSpriteLighting)) {
		const float ceiling = PIN(static_cast<GLfloat>(rect.ceiling_light) / static_cast<GLfloat>(FIXED_ONE), 0, 1);
		shade = std::max(shade, 0.5f * (shade + ceiling));
	}
	simd_float4 color = simd_make_float4(shade, shade, shade, 1);

	TMgr->ShapeDesc = rect.ShapeDesc;
	TMgr->LowLevelShape = rect.LowLevelShape;
	TMgr->ShadingTables = rect.shading_tables;
	TMgr->Texture = rect.texture;
	TMgr->TransferMode = rect.transfer_mode;
	TMgr->TransferData = rect.transfer_data;
	TMgr->IsShadeless = (rect.flags & _SHADELESS_BIT) != 0;
	TMgr->TextureType = type;

	if (current_player->infravision_duration) {
		struct bitmap_definition* dummy;
		// grab the normal shading tables, since the shader does the tinting
		extended_get_shape_bitmap_and_shading_table(GET_DESCRIPTOR_COLLECTION(TMgr->ShapeDesc), TMgr->LowLevelShape, &dummy, &TMgr->ShadingTables, _shading_normal);
	}

	float flare = weaponFlare;
	bool chosen = false;

	// priorities: static, infravision, tinted/solid, shadeless
	if (TMgr->TransferMode == _static_transfer) {
		TMgr->IsShadeless = 1;
		flare = -1;
		m.state.program = kInvincible;
		chosen = true;
		m.uniforms.transfer_fade_out = ((float)((uint16)rect.transfer_data)) / (float)((int)FIXED_ONE);
	} else if (current_player->infravision_duration) {
		color = infravision_colour(GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(rect.ShapeDesc)));
		m.state.program = kSpriteInfravision;
		chosen = true;
	} else if (TMgr->TransferMode == _tinted_transfer) {
		flare = -1;
		m.state.program = kInvisible;
		chosen = true;
		m.uniforms.visibility = 1.0 - rect.transfer_data / 32.0f;
	} else if (TMgr->TransferMode == _solid_transfer) {
		color = simd_make_float4(0, 1, 0, 1);
	} else if (TMgr->TransferMode == _textured_transfer) {
		if (TMgr->IsShadeless) {
			color = simd_make_float4(1, 1, 1, 1);
			flare = -1;
		}
	} else {
		color = simd_make_float4(0, 0, 1, 1);
	}

	if (!chosen)
		m.state.program = kSprite;

	if (!TMgr->Setup()) {
		TMgr->ShapeDesc = UNONE;
		return m;
	}
	TMgr->RenderNormal();
	use_ramps(m);

	float tm[16];
	TMgr->GetTextureMatrix(tm);
	m.uniforms.texture_matrix = simd_matrix(
		simd_make_float4(tm[0], tm[1], tm[2], tm[3]), simd_make_float4(tm[4], tm[5], tm[6], tm[7]),
		simd_make_float4(tm[8], tm[9], tm[10], tm[11]), simd_make_float4(tm[12], tm[13], tm[14], tm[15]));

	m.uniforms.flare = flare;
	m.uniforms.self_luminosity = selfLuminosity;
	m.uniforms.pulsate = 0;
	m.uniforms.wobble = 0;
	m.uniforms.depth_offset = offset;
	m.uniforms.glow = 0;
	m.uniforms.color = color;
	// HD art: a replacement image's bloom share (normal_bloom_*), and the
	// glow gain on a replacement's bright pixels (shader glow_gain).
	// Development: DURANDAL_HD_GLOW_GAIN=<f> sets it (2)
	m.uniforms.bloom_scale = TMgr->BloomScale();
	m.uniforms.bloom_shift = TMgr->BloomShift();
	static const float hd_glow_gain = [] {
		const char* g = std::getenv("DURANDAL_HD_GLOW_GAIN");
		return g ? float(std::atof(g)) : 2.0f;
	}();
	m.uniforms.glow_gain = TMgr->IsSubstitute() ? hd_glow_gain : 1.0f;
	// Glow (E1): the frame's minimum light makes projectiles, explosions,
	// muzzle flashes and lava scenery emissive
	if (m.uniforms.write_glow && TMgr->TransferMode == _textured_transfer && !current_player->infravision_duration) {
		shape_information_data* info = extended_get_shape_information(GET_DESCRIPTOR_COLLECTION(rect.ShapeDesc), rect.LowLevelShape);
		if (info)
			m.uniforms.emissive = PIN(info->minimum_light_intensity, 0, FIXED_ONE) / float(FIXED_ONE);
	}
	m.ok = true;
	return m;
}

RenderRasterize_Metal::Material RenderRasterize_Metal::setupWallTexture(const shape_descriptor& Texture, short transferMode, float pulsate, float wobble, float intensity, float offset)
{
	Material m;
	m.uniforms = frame_uniforms;
	auto& TMgr = m.TMgr;
	TMgr = std::make_unique<TextureManager>();
	LandscapeOptions *opts = NULL;
	TMgr->ShapeDesc = Texture;
	if (TMgr->ShapeDesc == UNONE)
		return m;
	get_shape_bitmap_and_shading_table(Texture, &TMgr->Texture, &TMgr->ShadingTables, _shading_normal);

	TMgr->TransferMode = _textured_transfer;
	TMgr->IsShadeless = current_player->infravision_duration ? 1 : 0;
	TMgr->TransferData = 0;

	float flare = weaponFlare;
	simd_float4 color = simd_make_float4(intensity, intensity, intensity, 1);
	bool chosen = false;

	switch (transferMode) {
		case _xfer_static:
			TMgr->TextureType = OGL_Txtr_Wall;
			TMgr->TransferMode = _static_transfer;
			TMgr->IsShadeless = 1;
			flare = -1;
			m.state.program = kInvincible;
			chosen = true;
			m.uniforms.transfer_fade_out = 0;
			break;
		case _xfer_landscape:
		case _xfer_big_landscape:
			TMgr->TextureType = OGL_Txtr_Landscape;
			TMgr->TransferMode = _big_landscaped_transfer;
			opts = View_GetLandscapeOptions(Texture);
			TMgr->LandscapeVertRepeat = opts->VertRepeat;
			TMgr->Landscape_AspRatExp = opts->SphereMap ? 1 : opts->OGL_AspRatExp;
			if (current_player->infravision_duration) {
				color = infravision_colour(GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(Texture)));
				m.state.program = opts->SphereMap ? kLandscapeSphereInfravision : kLandscapeInfravision;
			} else {
				m.state.program = opts->SphereMap ? kLandscapeSphere : kLandscape;
			}
			// Round 12: the sky is drawn on the level's own polygons; in the
			// distance image it is far, as a sky is, or ambient shadows shade
			// the creases between those polygons as lines across it
			m.uniforms.distance_mode = 2;
			chosen = true;
			break;
		default:
			TMgr->TextureType = OGL_Txtr_Wall;
			if (TMgr->IsShadeless) {
				color = simd_make_float4(1, 1, 1, 1);
				flare = -1;
			}
	}

	if (!chosen) {
		if (current_player->infravision_duration) {
			color = infravision_colour(GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(Texture)));
			m.state.program = kWallInfravision;
		} else {
			// GL: bump mapping would use S_Bump here; not ported yet
			m.state.program = kWall;
		}
	}

	if (!TMgr->Setup()) {
		TMgr->ShapeDesc = UNONE;
		return m;
	}
	TMgr->RenderNormal();
	use_ramps(m);
	// HD art: a replacement wall's normal map (Normal Maps) lights it as
	// relief does the shapes file's art (texture 5), and the pack's bloom
	// share (normal_bloom_*) goes to the glow image
	m.uniforms.normal_map = 0;
	if (m.state.program == kWall && Durandal::Enabled(Durandal::kNormalMaps) &&
		TMgr->GetTextureState() && TMgr->GetTextureState()->IsBumped) {
		TMgr->RenderBump();
		// The packs' maps have y up the image (OpenGL convention).
		// Development: DURANDAL_NORMAL_MAP_Y=down reads y as the image's
		// down; DURANDAL_NORMAL_MAP_GAIN=<f> sets the strength (1.5)
		static const uint32_t convention = [] {
			const char* y = std::getenv("DURANDAL_NORMAL_MAP_Y");
			return (y && std::strcmp(y, "down") == 0) ? 2u : 1u;
		}();
		static const float gain = [] {
			const char* g = std::getenv("DURANDAL_NORMAL_MAP_GAIN");
			return g ? float(std::atof(g)) : 1.5f;
		}();
		m.uniforms.normal_map = convention;
		m.uniforms.normal_gain = gain;
	}
	m.uniforms.bloom_scale = TMgr->BloomScale();
	m.uniforms.bloom_shift = TMgr->BloomShift();

	float tm[16];
	TMgr->GetTextureMatrix(tm);
	m.uniforms.texture_matrix = simd_matrix(
		simd_make_float4(tm[0], tm[1], tm[2], tm[3]), simd_make_float4(tm[4], tm[5], tm[6], tm[7]),
		simd_make_float4(tm[8], tm[9], tm[10], tm[11]), simd_make_float4(tm[12], tm[13], tm[14], tm[15]));

	if (TMgr->TextureType == OGL_Txtr_Landscape && opts) {
		if (opts->SphereMap) {
			m.uniforms.offsetx = opts->Azimuth * TWO_PI * FullCircleReciprocal;
			m.uniforms.offsety = opts->Elevation * TWO_PI * FullCircleReciprocal;
		} else {
			double TexScale = std::abs(TMgr->U_Scale);
			double HorizScale = double(1 << opts->HorizExp);
			m.uniforms.scalex = HorizScale * (npotTextures ? 1.0 : TexScale) * Radian2Circle;
			m.uniforms.offsetx = HorizScale * (0.25 + opts->Azimuth * FullCircleReciprocal);

			short AdjustedVertExp = opts->VertExp + opts->OGL_AspRatExp;
			double VertScale = (AdjustedVertExp >= 0) ? double(1 << AdjustedVertExp)
			                   : 1 / double(1 << (-AdjustedVertExp));
			m.uniforms.scaley = VertScale * TexScale * Radian2Circle;
			m.uniforms.offsety = (0.5 + TMgr->U_Offset) * TexScale;
		}
	}

	m.uniforms.flare = flare;
	m.uniforms.self_luminosity = selfLuminosity;
	m.uniforms.pulsate = pulsate;
	m.uniforms.wobble = wobble;
	m.uniforms.depth_offset = offset;
	m.uniforms.glow = 0;
	m.uniforms.color = color;
	m.ok = true;
	return m;
}

// Marathon shading (L1): plain textured walls and sprites from the shapes
// file take their colours through the 8-bit shading ramps instead
void RenderRasterize_Metal::use_ramps(Material& m)
{
	if (!Durandal::Enabled(Durandal::kShadingTables))
		return;
	if (m.state.program != kWall && m.state.program != kSprite)
		return;
	auto& TMgr = m.TMgr;
	if (TMgr->TransferMode != _textured_transfer || !TMgr->GetTextureState())
		return;
	if (BindRamps(*TMgr->GetTextureState(), TMgr->GetCollection(), TMgr->GetCTable()))
		m.state.program = (m.state.program == kWall) ? kWallRamp : kSpriteRamp;
}

bool RenderRasterize_Metal::setup_glow(Material& m, float wobble, float offset)
{
	auto& TMgr = m.TMgr;
	if (TMgr->TransferMode != _textured_transfer || !TMgr->IsGlowMapped())
		return false;
	// The ramps already give self-luminous colours their floor. The glow
	// texture is still placed once: without it the texture manager would
	// rebuild this texture's images every frame (TextureState::NeedsImages)
	if (m.state.program == kWallRamp || m.state.program == kSpriteRamp) {
		TMgr->RenderGlowing();
		return false;
	}

	// GL: S_Wall (or S_Bump) for walls, S_Sprite otherwise
	m.state.program = (TMgr->TextureType == OGL_Txtr_Wall) ? kWall : kSprite;
	TMgr->RenderGlowing();
	m.state.blend = static_cast<Blend>(TMgr->GlowBlend());
	m.uniforms.alpha_threshold = 0.001f;
	m.uniforms.flare = weaponFlare;
	m.uniforms.self_luminosity = selfLuminosity;
	m.uniforms.wobble = wobble;
	m.uniforms.depth_offset = offset - 1.0f;
	m.uniforms.glow = TMgr->MinGlowIntensity();
	// HD art: the glow image's bloom share (glow_bloom_*); the frame's own
	// emissive light was written by the base pass
	m.uniforms.bloom_scale = TMgr->GlowBloomScale();
	m.uniforms.bloom_shift = TMgr->GlowBloomShift();
	m.uniforms.emissive = 0;
	return true;
}

void RenderRasterize_Metal::render_node_floor_or_ceiling(clipping_window_data *window,
	polygon_data *polygon, horizontal_surface_data *surface, bool void_present, bool ceil, RenderStep renderStep)
{
	float offset = 0;

	const shape_descriptor& texture = AnimTxtr_Translate(surface->texture);
	float intensity = get_light_intensity(surface->lightsource_index) / float(FIXED_ONE - 1);
	float wobble = durandal_calc_wobble_for(view, surface->transfer_mode);
	// GL: wobble and pulsate behave the same way on floors and ceilings,
	// and a stronger wobble looks more like the classic renderer
	Material m = setupWallTexture(texture, surface->transfer_mode, wobble * 4.0, 0, intensity, offset);
	if (!m.ok || m.TMgr->ShapeDesc == UNONE)
		return;

	set_blend(m, void_present);

	// Light redistribution (E4): the polygon's own floor or ceiling
	if (surface->height == (ceil ? polygon->ceiling_height : polygon->floor_height) && surface->texture == (ceil ? polygon->ceiling_texture : polygon->floor_texture))
		m.uniforms.patch = ceil ? DurandalRadiance::CeilingPatch(frame_uniforms.polygon) : DurandalRadiance::FloorPatch(frame_uniforms.polygon);

	// Liquids (W1): the media surface, drawn by its own shader over what
	// is below it
	if (polygon->media_index != NONE && Durandal::Enabled(Durandal::kLiquids) &&
		(m.state.program == kWall || m.state.program == kWallRamp)) {
		const media_data* media = get_media_data(polygon->media_index);
		if (surface->height == media->height && surface->texture == media->texture) {
			const LiquidStyle style = liquid_style(media->type);
			m.state.program = kLiquid;
			m.state.blend = kBlendNone;
			m.uniforms.alpha_threshold = -1;
			m.uniforms.liquid = simd_make_float4(media->type + 1, style.murk, style.opacity, style.waves);
			m.uniforms.liquid_colour = simd_make_float4(style.absorb[0], style.absorb[1], style.absorb[2], style.glow);
			m.uniforms.caustics = 0;
			// The level lights a liquid's surface on its own, often brighter
			// than the room it is in; drawn with depth and sheen, that reads
			// as a floodlit pool in a dark room. Capped at the room's own
			// light (lava keeps its glow)
			if (media->type != _media_lava) {
				const float room = 0.5f * (get_light_intensity(polygon->floor_lightsource_index) +
										   get_light_intensity(polygon->ceiling_lightsource_index)) / float(FIXED_ONE - 1);
				const float lit = std::min(m.uniforms.color.x, room);
				m.uniforms.color = simd_make_float4(lit, lit, lit, m.uniforms.color.w);
			}
		}
	}

	short vertex_count = polygon->vertex_count;
	if (!vertex_count)
		return;

	clip_to_window(window);

	world_distance x = 0.0, y = 0.0;
	instantiate_transfer_mode(view, surface->transfer_mode, x, y);

	if (ceil) {
		m.uniforms.normal = simd_make_float4(0, 0, -1, 0);
		m.uniforms.tangent = simd_make_float4(0, 1, 0, 1);
	} else {
		m.uniforms.normal = simd_make_float4(0, 0, 1, 0);
		m.uniforms.tangent = simd_make_float4(0, 1, 0, -1);
	}

	float scale;
	switch (surface->transfer_mode) {
		case _xfer_2x: scale = 2 * WORLD_ONE * m.TMgr->TileRatio(); break;
		case _xfer_4x: scale = 4 * WORLD_ONE * m.TMgr->TileRatio(); break;
		default: scale = WORLD_ONE * m.TMgr->TileRatio(); break;
	}

	Vertex vertices[MAXIMUM_VERTICES_PER_POLYGON];
	for (short i = 0; i < vertex_count && i < MAXIMUM_VERTICES_PER_POLYGON; ++i) {
		const short index = ceil ? polygon->endpoint_indexes[vertex_count - 1 - i] : polygon->endpoint_indexes[i];
		world_point2d vertex = get_endpoint_data(index)->vertex;
		vertices[i] = { float(vertex.x), float(vertex.y), float(surface->height),
			(vertex.x + surface->origin.x + x) / scale,
			(vertex.y + surface->origin.y + y) / scale };
	}

	draw(m, vertices, vertex_count);

	// GL: the pulsate uniform stays set from the wall set-up
	m.uniforms.patch = -1;
	if (m.state.program != kLiquid && setup_glow(m, 0, offset))
		draw(m, vertices, vertex_count);
}

void RenderRasterize_Metal::render_node_side(clipping_window_data *window, vertical_surface_data *surface, bool void_present, RenderStep renderStep)
{
	float offset = 0;
	if (!void_present)
		offset = -2.0;

	const shape_descriptor& texture = AnimTxtr_Translate(surface->texture_definition->texture);
	float intensity = (get_light_intensity(surface->lightsource_index) + surface->ambient_delta) / float(FIXED_ONE - 1);
	float wobble = durandal_calc_wobble_for(view, surface->transfer_mode);
	float pulsate = 0;
	if (surface->transfer_mode == _xfer_pulsate) {
		pulsate = wobble;
		wobble = 0;
	}
	Material m = setupWallTexture(texture, surface->transfer_mode, pulsate, wobble, intensity, offset);
	if (!m.ok || m.TMgr->ShapeDesc == UNONE)
		return;

	set_blend(m, void_present);
	m.uniforms.patch = DurandalRadiance::WallPatch(surface->texture_definition);	// light redistribution (E4)

	world_distance h = MIN(surface->h1, surface->hmax);
	if (h <= surface->h0)
		return;

	world_point2d vertex[2];
	uint16 flags;
	long_to_overflow_short_2d(surface->p0, vertex[0], flags);
	long_to_overflow_short_2d(surface->p1, vertex[1], flags);

	clip_to_window(window);

	uint16 div;
	switch (surface->transfer_mode) {
		case _xfer_2x: div = 2 * WORLD_ONE * m.TMgr->TileRatio(); break;
		case _xfer_4x: div = 4 * WORLD_ONE * m.TMgr->TileRatio(); break;
		default: div = WORLD_ONE * m.TMgr->TileRatio(); break;
	}

	double dx = (surface->p1.i - surface->p0.i) / double(surface->length);
	double dy = (surface->p1.j - surface->p0.j) / double(surface->length);

	world_distance x0 = surface->texture_definition->x0 % div;
	world_distance y0 = surface->texture_definition->y0 % div;

	double tOffset = surface->h1 + view->origin.z + y0;

	m.uniforms.normal = simd_make_float4(-dy, dx, 0, 0);
	m.uniforms.tangent = simd_make_float4(dx, dy, 0, 1);

	world_distance x = 0.0, y = 0.0;
	instantiate_transfer_mode(view, surface->transfer_mode, x, y);
	x0 -= x;
	tOffset -= y;

	const float top = h + view->origin.z;
	const float bottom = surface->h0 + view->origin.z;
	const float px[4] = { float(vertex[0].x), float(vertex[1].x), float(vertex[1].x), float(vertex[0].x) };
	const float py[4] = { float(vertex[0].y), float(vertex[1].y), float(vertex[1].y), float(vertex[0].y) };
	const float pz[4] = { top, top, bottom, bottom };

	Vertex vertices[4];
	for (int i = 0; i < 4; ++i) {
		float p2 = (i == 1 || i == 2) ? surface->length : 0;
		vertices[i] = { px[i], py[i], pz[i],
			float((tOffset - pz[i]) / static_cast<float>(div)),
			(x0 + p2) / static_cast<float>(div) };
	}

	draw(m, vertices, 4);

	m.uniforms.patch = -1;
	if (setup_glow(m, wobble, offset))
		draw(m, vertices, 4);
}

void RenderRasterize_Metal::render_node_object(render_object_data *object, bool other_side_of_media, RenderStep renderStep)
{
	if (!object->clipping_windows)
		return;

	// Sprites in a liquid are drawn above and below the surface in
	// separate passes, as the software renderer does (GL: clip plane 5).
	short media_index = get_polygon_data(object->node->polygon_index)->media_index;
	media_data *media = (media_index != NONE) ? get_media_data(media_index) : NULL;
	if (media) {
		float h = media->height;
		clip_planes[2] = simd_make_float4(0, 0, 1, -h);
		if (view->under_media_boundary ^ other_side_of_media)
			clip_planes[2] = simd_make_float4(0, 0, -1, h);
		clip_mask |= 4u;
	} else if (other_side_of_media) {
		return;
	}

	for (clipping_window_data *win = object->clipping_windows; win; win = win->next_window) {
		clip_to_window(win);
		_render_node_object_helper(object, renderStep);
	}

	clip_mask &= ~4u;
}

void RenderRasterize_Metal::_render_node_object_helper(render_object_data *object, RenderStep renderStep)
{
	rectangle_definition& rect = object->rectangle;
	const world_point3d& pos = rect.Position;

	if (rect.ModelPtr) {
		render_model(object);
		return;
	}

	float offset = 0;
	const bool strict_depth = OGL_ForceSpriteDepth();
	if (strict_depth) {
		// look for parasitic objects based on y position,
		// and offset them to draw in proper depth order
		if (pos.y == objectY) {
			objectCount++;
			offset = objectCount * -1.0;
		} else {
			objectCount = 0;
			objectY = pos.y;
		}
	}

	Material m = setupSpriteTexture(rect, OGL_Txtr_Inhabitant, offset);
	if (!m.ok || m.TMgr->ShapeDesc == UNONE)
		return;
	auto& TMgr = m.TMgr;

	// Bounced Light (R4): the floor under the figure's feet and the ceiling
	// over it (the render node's polygon can be another one entirely)
	if (light_bounce && frame_uniforms.gi) {
		world_point2d where = { world_distance(pos.x), world_distance(pos.y) };
		short polygon_index = world_point_to_polygon_index(&where);
		if (polygon_index == NONE)
			polygon_index = object->node->polygon_index;
		m.uniforms.figure_patches = simd_make_int4(DurandalRadiance::FloorPatch(polygon_index),
												   DurandalRadiance::CeilingPatch(polygon_index), 0, 0);
	}

	// GL: glTranslated(pos); glRotated(yaw, z); optionally glRotated(pitch, -y)
	simd_float4x4 transform = translate(identity(), pos.x, pos.y, pos.z);
	transform = rotate(transform, view->virtual_yaw * FixedAngleToDegrees, 0.0, 0.0, 1.0);
	if (!view->mimic_sw_perspective) {
		if (TMgr->ForceXYBillboard() || (view->billboard_xy && !TMgr->ForceYBillboard()))
			transform = rotate(transform, view->virtual_pitch * FixedAngleToDegrees, 0.0, -1.0, 0.0);
	}

	float texCoords[2][2];
	if (rect.flip_vertical) {
		texCoords[0][1] = TMgr->U_Offset;
		texCoords[0][0] = TMgr->U_Scale + TMgr->U_Offset;
	} else {
		texCoords[0][0] = TMgr->U_Offset;
		texCoords[0][1] = TMgr->U_Scale + TMgr->U_Offset;
	}
	if (rect.flip_horizontal) {
		texCoords[1][1] = TMgr->V_Offset;
		texCoords[1][0] = TMgr->V_Scale + TMgr->V_Offset;
	} else {
		texCoords[1][0] = TMgr->V_Offset;
		texCoords[1][1] = TMgr->V_Scale + TMgr->V_Offset;
	}

	set_blend(m, false, TMgr->TransferMode == _tinted_transfer);
	m.state.depth_test = strict_depth;	// GL: depth test off unless sprites are forced into depth

	const float left = rect.WorldLeft * rect.HorizScale * rect.Scale;
	const float right = rect.WorldRight * rect.HorizScale * rect.Scale;
	const float top = rect.WorldTop * rect.Scale;
	const float bottom = rect.WorldBottom * rect.Scale;
	const simd_float4 local[4] = {
		simd_make_float4(0, left, top, 1), simd_make_float4(0, right, top, 1),
		simd_make_float4(0, right, bottom, 1), simd_make_float4(0, left, bottom, 1) };
	const float uv[4][2] = {
		{ texCoords[0][0], texCoords[1][0] }, { texCoords[0][0], texCoords[1][1] },
		{ texCoords[0][1], texCoords[1][1] }, { texCoords[0][1], texCoords[1][0] } };

	Vertex vertices[4];
	for (int i = 0; i < 4; ++i) {
		const simd_float4 w = simd_mul(transform, local[i]);
		vertices[i] = { w.x, w.y, w.z, uv[i][0], uv[i][1] };
	}

	if (Durandal::Enabled(Durandal::kCharacterShadows))
		draw_sprite_shadow(object, m, vertices);
	draw(m, vertices, 4);
	if (setup_glow(m, 0, offset))
		draw(m, vertices, 4);
}

// Character shadows (Round 12). Monsters, BoBs, scenery and items cast
// their sprite's silhouette onto the floor of their polygon (or its
// liquid), along the nearest dynamic light in range, else a fixed high
// light, so a shadow lies beside every figure. Emissive frames
// (projectiles, explosions) and tinted or static ones cast none. Seen
// from eye height a floor shadow is a low band, so the fixed light sits
// at 40 degrees for a shadow longer than the figure is tall, and behind
// the viewer's left shoulder (Marathon has no sun to fix it to), so the
// shadow falls forward and to the right of every figure, never hidden
// behind its own sprite.
// Development: DURANDAL_SHADOW="strength,elevation,azimuth" (0.6, 40, 135;
// the azimuth is relative to the view); DURANDAL_SHADOW_LOG=1 prints the
// first shadows.
void RenderRasterize_Metal::draw_sprite_shadow(render_object_data *object, Material& m, const Vertex* quad)
{
	auto& TMgr = m.TMgr;
	if (TMgr->TransferMode != _textured_transfer || m.uniforms.emissive > 0 || (m.state.program != kSprite && m.state.program != kSpriteRamp))
		return;
	const short collection = GET_COLLECTION(GET_DESCRIPTOR_COLLECTION(object->rectangle.ShapeDesc));
	const bool figure = (collection >= 2 && collection <= 16 && collection != 4) || collection == 31 || (collection >= 22 && collection <= 26) || collection == 7;
	if (!figure)
		return;
	static float strength = 0.6f, elevation = 40, azimuth = 135;
	static bool parsed = false;
	if (!parsed) {
		parsed = true;
		if (const char* v = std::getenv("DURANDAL_SHADOW"))
			sscanf(v, "%f,%f,%f", &strength, &elevation, &azimuth);
	}
	// The floor under the object's own feet (the render node's polygon is
	// where it is sorted, which can be another polygon entirely)
	world_point2d where = { world_distance(object->rectangle.Position.x), world_distance(object->rectangle.Position.y) };
	short polygon_index = world_point_to_polygon_index(&where);
	if (polygon_index == NONE)
		polygon_index = object->node->polygon_index;
	const polygon_data* polygon = get_polygon_data(polygon_index);
	float plane = polygon->floor_height;
	if (polygon->media_index != NONE)
		plane = std::max(plane, float(get_media_data(polygon->media_index)->height));
	plane += 2;
	// The feet: the quad's lowest corner
	float feet = quad[0].z;
	for (int i = 1; i < 4; ++i)
		feet = std::min(feet, quad[i].z);
	const float height = std::max(quad[0].z, quad[1].z) - feet;
	const float rise = feet - plane;
	if (rise > 1.5f * WORLD_ONE || rise < -WORLD_ONE / 2 || height <= 0)
		return;	// flying high, sunk, or nothing to cast
	// Fading as it rises (flying monsters)
	const float grounded = 1.0f - std::max(rise, 0.0f) / (1.5f * WORLD_ONE);

	// Shadow direction: the nearest dynamic light in range, else fixed
	const simd_float4 centre = simd_make_float4(0.25f * (quad[0].x + quad[1].x + quad[2].x + quad[3].x),
												0.25f * (quad[0].y + quad[1].y + quad[2].y + quad[3].y), feet, 1);
	simd_float3 L = simd_make_float3(0, 0, -1);
	bool from_light = false;
	float best = 1e30f;
	for (int i = 0; i < shadow_light_count; ++i) {
		const simd_float4 lp = shadow_lights[i].position_radius;
		const float dx = centre.x - lp.x, dy = centre.y - lp.y, dz = feet - lp.z;
		const float d2 = dx * dx + dy * dy + dz * dz;
		if (lp.z > feet + WORLD_ONE / 8 && d2 < lp.w * lp.w && d2 < best) {
			best = d2;
			L = simd_normalize(simd_make_float3(dx, dy, dz));
			from_light = true;
		}
	}
	if (!from_light) {
		const float el = elevation * float(M_PI / 180);
		const float az = (view->yaw * (360.0f / FULL_CIRCLE) + azimuth) * float(M_PI / 180);
		L = simd_make_float3(std::cos(el) * std::cos(az), std::cos(el) * std::sin(az), -std::sin(el));
	}
	// Never flatter than 30 degrees: a low light would throw the shadow
	// across the room
	const float horizontal = std::sqrt(L.x * L.x + L.y * L.y);
	if (-L.z < horizontal * std::tan(30 * float(M_PI / 180))) {
		const float lz = -horizontal * std::tan(30 * float(M_PI / 180));
		L = simd_normalize(simd_make_float3(L.x, L.y, lz));
	}

	Vertex shadow[4];
	for (int i = 0; i < 4; ++i) {
		const float t = (plane - quad[i].z) / L.z;	// along L down to the plane (L.z < 0, quad above)
		shadow[i] = { quad[i].x + L.x * t, quad[i].y + L.y * t, plane, quad[i].u, quad[i].v };
	}
	Material s;
	s.uniforms = m.uniforms;
	s.state.program = kShadow;
	s.state.blend = kBlendCrossfade;
	s.state.depth_test = true;
	s.state.depth_write = false;
	s.uniforms.alpha_threshold = 0.02f;
	s.uniforms.color = simd_make_float4(0, 0, 0, strength * grounded * (from_light ? 1.0f : 0.85f));
	s.uniforms.emissive = 0;
	static const bool log_shadows = std::getenv("DURANDAL_SHADOW_LOG") != nullptr;
	static int logged = 0;
	if (log_shadows && logged < 6) {
		++logged;
		fprintf(stderr, "Durandal shadow: tick %d coll %d feet %.0f plane %.0f height %.0f L (%.2f %.2f %.2f) quad z %.0f %.0f %.0f %.0f -> (%.0f,%.0f) (%.0f,%.0f) (%.0f,%.0f) (%.0f,%.0f) program %d\n",
				view->tick_count, collection, feet, plane, height, L.x, L.y, L.z, quad[0].z, quad[1].z, quad[2].z, quad[3].z,
				shadow[0].x, shadow[0].y, shadow[1].x, shadow[1].y, shadow[2].x, shadow[2].y, shadow[3].x, shadow[3].y, int(m.state.program));
	}
	// The sprite's colour texture (the ramp path bound its indices after it)
	TMgr->RenderNormal();
	draw(s, shadow, 4);
	TMgr->RenderNormal();
}

// 3D pickups (HD art): a plugin's model replaces the object's sprite. As
// the OpenGL renderer, the model is placed by translate, yaw and scale
// (the loader baked the MML rotations and scale into its vertices) and lit
// as a wall: the room's light, the headlight by depth, dynamic lights on
// its faces. Depth tested, so it sits in the world; no contact shadow on
// itself. Spinning Pickups turns items slowly (a look only: the object
// never moves).
void RenderRasterize_Metal::render_model(render_object_data *object)
{
	rectangle_definition& rect = object->rectangle;
	OGL_ModelData* model = rect.ModelPtr;
	Model3D& mesh = model->Model;
	if (mesh.Positions.size() < 9 || mesh.VertIndices.size() < 3)
		return;
	const short coll_colour = GET_DESCRIPTOR_COLLECTION(rect.ShapeDesc);
	const short collection = GET_COLLECTION(coll_colour);
	const short clut = ModifyCLUT(rect.transfer_mode, GET_COLLECTION_CLUT(coll_colour));
	OGL_SkinData* skin = model->GetSkin(clut);
	if (!skin || !skin->NormalImg.IsPresent())
		return;

	Material m;
	m.uniforms = frame_uniforms;
	float shade = PIN(static_cast<GLfloat>(rect.ambient_shade) / static_cast<GLfloat>(FIXED_ONE), 0, 1);
	if (Durandal::Enabled(Durandal::kSpriteLighting)) {
		const float ceiling = PIN(static_cast<GLfloat>(rect.ceiling_light) / static_cast<GLfloat>(FIXED_ONE), 0, 1);
		shade = std::max(shade, 0.5f * (shade + ceiling));
	}
	simd_float4 color = simd_make_float4(shade, shade, shade, 1);
	float flare = weaponFlare;
	m.state.program = kWall;
	bool textured = false;
	if (rect.transfer_mode == _static_transfer) {
		flare = -1;
		m.state.program = kInvincible;
		m.uniforms.transfer_fade_out = ((float)((uint16)rect.transfer_data)) / (float)((int)FIXED_ONE);
	} else if (current_player->infravision_duration) {
		color = infravision_colour(collection);
		m.state.program = kWallInfravision;
	} else if (rect.transfer_mode == _tinted_transfer) {
		flare = -1;
		m.state.program = kInvisible;
		m.uniforms.visibility = 1.0 - rect.transfer_data / 32.0f;
	} else if (rect.transfer_mode == _solid_transfer) {
		color = simd_make_float4(0, 1, 0, 1);
	} else if (rect.transfer_mode == _textured_transfer) {
		if (rect.flags & _SHADELESS_BIT) {
			color = simd_make_float4(1, 1, 1, 1);
			flare = -1;
		} else {
			textured = true;
		}
	} else {
		color = simd_make_float4(0, 0, 1, 1);
	}

	if (!PlaceModelSkin(skin, 0, &skin->NormalImg))
		return;
	m.state.depth_test = true;
	m.state.depth_write = true;
	// GL RenderModel: blended when the skin says so or the object is tinted
	if (skin->OpacityType != OGL_OpacType_Crisp || rect.transfer_mode == _tinted_transfer) {
		m.state.blend = static_cast<Blend>(skin->NormalBlend);
		m.uniforms.alpha_threshold = 0.001f;
	} else {
		m.state.blend = kBlendNone;
		m.uniforms.alpha_threshold = 0.5f;
	}
	m.uniforms.texture_matrix = identity();
	m.uniforms.normal = simd_make_float4(0, 0, 0, 0);
	m.uniforms.tangent = simd_make_float4(0, 0, 0, 0);
	m.uniforms.flare = flare;
	m.uniforms.self_luminosity = selfLuminosity;
	m.uniforms.pulsate = 0;
	m.uniforms.wobble = 0;
	m.uniforms.depth_offset = 0;
	m.uniforms.glow = 0;
	m.uniforms.color = color;
	m.uniforms.caster_count = 0;
	m.uniforms.patch = -1;
	m.uniforms.normal_map = 0;
	m.uniforms.bloom_scale = skin->BloomScale;
	m.uniforms.bloom_shift = skin->BloomShift;
	m.uniforms.glow_gain = 1;
	if (m.uniforms.write_glow && textured) {
		shape_information_data* info = extended_get_shape_information(coll_colour, rect.LowLevelShape);
		if (info)
			m.uniforms.emissive = PIN(info->minimum_light_intensity, 0, FIXED_ONE) / float(FIXED_ONE);
	}

	// Placement (GL: translate, rotate by the azimuth, scale)
	const world_point3d& pos = rect.Position;
	double azimuth = (360.0 / FULL_CIRCLE) * rect.Azimuth;
	if (collection == _collection_items && Durandal::Enabled(Durandal::kSpinPickups)) {
		// One turn every four seconds, each item on its own phase
		azimuth += view->tick_count * (360.0 / (4 * TICKS_PER_SECOND)) + ((pos.x + pos.y) & 0xFF) * (360.0 / 256);
	}
	simd_float4x4 place = translate(identity(), pos.x, pos.y, pos.z);
	place = rotate(place, azimuth, 0.0, 0.0, 1.0);
	const float horizontal = rect.Scale * rect.HorizScale;
	place = simd_mul(place, simd_diagonal_matrix(simd_make_float4(horizontal, horizontal, rect.Scale, 1)));
	m.uniforms.model = place;
	m.uniforms.clip_mask = clip_mask;
	for (int i = 0; i < 3; ++i)
		m.uniforms.clip_planes[i] = clip_planes[i];

	const float* texcoords = mesh.TxtrCoords.empty() ? nullptr : mesh.TxtrCoords.data();
	// Development: DURANDAL_MODEL_LOG=1 prints each model's first draw
	static const bool log_models = std::getenv("DURANDAL_MODEL_LOG") != nullptr;
	if (log_models) {
		static std::vector<const void*> seen;
		if (std::find(seen.begin(), seen.end(), model) == seen.end()) {
			seen.push_back(model);
			fprintf(stderr, "Durandal model: tick %d collection %d sequence %d at (%d, %d, %d), %zu vertices\n",
					view->tick_count, collection, GET_DESCRIPTOR_SHAPE(rect.ShapeDesc), pos.x, pos.y, pos.z, mesh.Positions.size() / 3);
		}
	}
	DrawModel(m.state, m.uniforms, model, mesh.Positions.data(), texcoords, int(mesh.Positions.size() / 3),
			  mesh.VertIndices.data(), int(mesh.VertIndices.size()), model->Sidedness);

	// The skin's glow image, as a sprite's glow pass
	if (textured && skin->GlowImg.IsPresent() && PlaceModelSkin(skin, 1, &skin->GlowImg)) {
		m.state.blend = static_cast<Blend>(skin->GlowBlend);
		m.uniforms.alpha_threshold = 0.001f;
		m.uniforms.glow = skin->MinGlowIntensity;
		m.uniforms.bloom_scale = skin->GlowBloomScale;
		m.uniforms.bloom_shift = skin->GlowBloomShift;
		m.uniforms.emissive = 0;
		DrawModel(m.state, m.uniforms, model, mesh.Positions.data(), texcoords, int(mesh.Positions.size() / 3),
				  mesh.VertIndices.data(), int(mesh.VertIndices.size()), model->Sidedness);
	}
}

void RenderRasterize_Metal::render_viewer_sprite_layer(RenderStep renderStep)
{
	if (!view->show_weapons_in_hand)
		return;

	rectangle_definition rect;
	weapon_display_information display_data;
	shape_information_data *shape_information;
	short count;

	rect.ModelPtr = nullptr;
	rect.Opacity = 1;

	count = 0;
	while (get_weapon_display_information(&count, &display_data))
	{
		shape_information = extended_get_shape_information(display_data.collection, display_data.low_level_shape_index);
		if (!shape_information) continue;

		rect.ShapeDesc = BUILD_DESCRIPTOR(display_data.collection, 0);
		rect.LowLevelShape = display_data.low_level_shape_index;

		if (shape_information->flags & _X_MIRRORED_BIT) display_data.flip_horizontal = !display_data.flip_horizontal;
		if (shape_information->flags & _Y_MIRRORED_BIT) display_data.flip_vertical = !display_data.flip_vertical;

		position_sprite_axis(&rect.x0, &rect.x1, view->screen_height, view->screen_width, display_data.horizontal_positioning_mode,
			display_data.horizontal_position, display_data.flip_horizontal, shape_information->world_left, shape_information->world_right);
		position_sprite_axis(&rect.y0, &rect.y1, view->screen_height, view->screen_height, display_data.vertical_positioning_mode,
			display_data.vertical_position, display_data.flip_vertical, -shape_information->world_top, -shape_information->world_bottom);

		extended_get_shape_bitmap_and_shading_table(display_data.collection, display_data.low_level_shape_index, &rect.texture, &rect.shading_tables, view->shading_mode);
		if (!rect.texture) continue;

		rect.flags = 0;
		rect.clip_left = 0;
		rect.clip_right = view->screen_width;
		rect.clip_top = 0;
		rect.clip_bottom = view->screen_height;
		rect.flip_horizontal = display_data.flip_horizontal;
		rect.flip_vertical = display_data.flip_vertical;

		/* lighting: depth of zero in the camera's polygon index */
		rect.depth = 0;
		rect.ambient_shade = get_light_intensity(get_polygon_data(view->origin_polygon_index)->floor_lightsource_index);
		rect.ambient_shade = MAX(shape_information->minimum_light_intensity, rect.ambient_shade);
		if (view->shading_mode == _shading_infravision) rect.flags |= _SHADELESS_BIT;

		rect.xc = (rect.x0 + rect.x1) >> 1;
		instantiate_rectangle_transfer_mode(view, &rect, display_data.transfer_mode, display_data.transfer_phase);

		render_viewer_sprite(rect, renderStep);
	}
}

void RenderRasterize_Metal::render_viewer_sprite(rectangle_definition& RenderRectangle, RenderStep renderStep)
{
	point2d TopLeft, BottomRight;
	TopLeft.x = MAX(RenderRectangle.x0, RenderRectangle.clip_left);
	TopLeft.y = MAX(RenderRectangle.y0, RenderRectangle.clip_top);
	BottomRight.x = MIN(RenderRectangle.x1, RenderRectangle.clip_right);
	BottomRight.y = MIN(RenderRectangle.y1, RenderRectangle.clip_bottom);

	if (BottomRight.x <= TopLeft.x) return;
	if (BottomRight.y <= TopLeft.y) return;

	Material m = setupSpriteTexture(RenderRectangle, OGL_Txtr_WeaponsInHand, 0);
	m.uniforms.light_count = 0;	// weapons in hand are not in world space
	if (!m.ok || m.TMgr->ShapeDesc == UNONE)
		return;
	auto& TMgr = m.TMgr;

	// GL: projection Screen_2_Clip, identity modelview, screen coordinates
	m.uniforms.projection = from_gl(Screen_2_Clip);
	m.uniforms.modelview = identity();

	double U_Scale = TMgr->U_Scale / (RenderRectangle.y1 - RenderRectangle.y0);
	double V_Scale = TMgr->V_Scale / (RenderRectangle.x1 - RenderRectangle.x0);
	double U_Offset = TMgr->U_Offset;
	double V_Offset = TMgr->V_Offset;

	double u0, u2, v0, v2;
	if (RenderRectangle.flip_vertical) {
		u0 = U_Offset + U_Scale * (RenderRectangle.y1 - TopLeft.y);
		u2 = U_Offset + U_Scale * (RenderRectangle.y1 - BottomRight.y);
	} else {
		u0 = U_Offset + U_Scale * (TopLeft.y - RenderRectangle.y0);
		u2 = U_Offset + U_Scale * (BottomRight.y - RenderRectangle.y0);
	}
	if (RenderRectangle.flip_horizontal) {
		v0 = V_Offset + V_Scale * (RenderRectangle.x1 - TopLeft.x);
		v2 = V_Offset + V_Scale * (RenderRectangle.x1 - BottomRight.x);
	} else {
		v0 = V_Offset + V_Scale * (TopLeft.x - RenderRectangle.x0);
		v2 = V_Offset + V_Scale * (BottomRight.x - RenderRectangle.x0);
	}

	// Same order as the GL renderer, for the same sidedness
	Vertex vertices[4] = {
		{ float(TopLeft.x), float(TopLeft.y), 1, float(u0), float(v0) },
		{ float(BottomRight.x), float(TopLeft.y), 1, float(u0), float(v2) },
		{ float(BottomRight.x), float(BottomRight.y), 1, float(u2), float(v2) },
		{ float(TopLeft.x), float(BottomRight.y), 1, float(u2), float(v0) },
	};

	set_blend(m, false, TMgr->TransferMode == _tinted_transfer);
	// Round 12: the weapon in hand writes the far distance where it is
	// opaque, as the sky, so the ambient shadows and the distance shade of
	// the floor behind it are not painted over it, and it occludes nothing
	m.uniforms.distance_mode = -1;
	m.state.depth_test = false;

	// Weapon lighting: the weapon in hand takes the dynamic lights around
	// the viewer, their strength and colour, as the world's surfaces do: a
	// bolt passing close tints it, and its own flash is among them. The
	// lights come from the polygons in view, so each has a line to the eye.
	if (Durandal::Enabled(Durandal::kWeaponLighting) && shadow_light_count > 0) {
		const simd_float3 eye = simd_make_float3(view->origin.x, view->origin.y, view->origin.z);
		float amount = 0;
		simd_float3 colour = simd_make_float3(0, 0, 0);
		for (int i = 0; i < shadow_light_count; ++i) {
			const simd_float4 lp = shadow_lights[i].position_radius;
			const float d2 = simd_length_squared(simd_make_float3(lp.x, lp.y, lp.z) - eye);
			if (d2 >= lp.w * lp.w)
				continue;
			float f = 1 - d2 / (lp.w * lp.w);
			f *= f;
			const simd_float4 cs = shadow_lights[i].colour_strength;
			amount += cs.w * f;
			colour += simd_make_float3(cs.x, cs.y, cs.z) * (cs.w * f);
		}
		if (amount > 0) {
			// The weapon is nearer its own flash than any wall is, and mostly
			// dark metal: a little more than a wall would take
			// (DURANDAL_WEAPON_LIGHT=<gain> for film runs)
			static const float gain = [] {
				const char* v = getenv("DURANDAL_WEAPON_LIGHT");
				return v ? float(atof(v)) : 1.5f;
			}();
			const simd_float3 c = colour / amount;
			const simd_float3 tint = c / std::max(std::max(c.x, std::max(c.y, c.z)), 1e-3f);
			m.uniforms.viewer_light = simd_make_float4(tint.x, tint.y, tint.z, amount * gain);
		}
	}

	const uint32_t saved_mask = clip_mask;
	clip_mask = 0;
	draw(m, vertices, 4);
	if (setup_glow(m, 0, 0)) {
		m.uniforms.viewer_light = simd_make_float4(0, 0, 0, 0);	// the glow image glows as it is
		draw(m, vertices, 4);
	}
	clip_mask = saved_mask;
}

#endif
