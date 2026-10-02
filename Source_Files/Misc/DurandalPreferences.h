#ifndef DURANDAL_PREFERENCES_H
#define DURANDAL_PREFERENCES_H
/*
	DurandalPreferences.h — Durandal project

	Settings for Durandal's enhancements, the feature gate, and the single
	quality-tier control. Stored as a <durandal> element in the normal
	preferences file, so upstream settings are untouched.

	Feature gate: enhancements are available in Debug builds, and in Release
	builds only when launched with DURANDAL_QA=1 in the environment (the
	shared Xcode scheme's Run action sets it). Opened from Finder, a Release
	build behaves exactly like stock Aleph One. A feature is live only when
	the gate is open AND its preference is on.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <string>

class InfoTree;

namespace Durandal {

enum QualityTier {
	kTierStock = 0,		// today's game exactly: 30 fps, no enhancements
	kTierClassic = 1,	// stock look, played modern
	kTierEnhanced = 2,	// Classic plus glow and light (Round 4 on)
	kTierFlagship = 3,	// Enhanced plus volumetrics (Round 7 on)
	kTierCustom = 4,	// (stored as 4 in existing files, so the fifth tier comes after it)
	kTierRampant = 5	// Flagship plus light that goes further (Round 13; docs/PLAN-fifth-tier.md)
};

enum Feature {
	kPerFrameLook,		// F1: mouse look applied every frame, input polled every frame
	kSmoothWorld,		// F2: liquids, lights, fades and texture motion interpolated
	kGradedOcclusion,	// A2: graded sound occlusion, per-frame listener
	kMetalRenderer,		// F3-F5: Metal renderer and display (applies at launch; on in Classic)
	kHDROutput,			// F5: EDR half-float output in Metal display mode (not part of any tier)
	kWidescreen,		// world view fills the width above the classic HUD on wide screens
	kShadingTables,		// L1: Marathon's 8-bit shading ramps (Metal renderer)
	kTexelLighting,		// L2: each texel lit at its centre (Metal renderer)
	kCrispFiltering,	// L4: sharp-bilinear texels, filtered minification (Metal renderer)
	kEdgeSmoothing,		// L4: 4x multisampled world edges (Metal renderer)
	kCrispText,			// T2: HUD and on-screen text rasterised at display scale (Metal display)
	kCrispTerminals,	// T1: terminal text drawn at display scale over the whole-number-scaled terminal
	kGlow,				// E1: emissive light above SDR white with HDR output (Metal display; Enhanced)
	kBloom,				// E1: bloom from emissive light only (Metal display; Enhanced)
	kHDRSky,			// V2: sky as a cylinder, bright sky glows (Metal display; Enhanced)
	kDynamicLights,		// E2: projectiles, explosions and flashes light the world (Metal; Enhanced)
	kSpriteLighting,	// S1: sprites also lit by a bright ceiling above them (Metal; Enhanced)
	kLightShadows,		// E3: dynamic lights blocked by walls, steps and ceilings (Metal; Enhanced)
	kLiquids,			// W1: liquid surfaces with depth, ripples and caustics (Metal; Enhanced)
	kContactShadows,	// soft shadows on the floor under items, monsters and scenery (Metal; Enhanced)
	kVolumetricFog,		// V1: haze and dust lit by the rooms and dynamic lights, underwater murk (Metal; Flagship)
	kLightRedistribution,	// E4: light moved within each surface by traced light, anchored (Metal; Flagship)
	kSurfaceRelief,		// M1: relief from the art under the headlight and dynamic lights (Metal, 8-bit shading; Flagship)
	kReverb,			// A1: room reverb from the map around the listener, underwater muffling (Enhanced)
	kTrueLook,			// C1: looking up and down rotates the camera, as Quake, instead of shearing (Metal; Enhanced)
	kSidestepSway,		// C2: the view rolls a little with sideways movement, as Quake (Metal, with True Look; Enhanced)
	kFreeLook,			// C3: the view looks beyond the physics model's aim limit; the crosshair marks the aim (with True Look; Enhanced)
	kHDWalls,			// HD art: installed packs replace the walls and landscapes (DurandalArt.h; Metal; Flagship)
	kHDMonsters,		//   the monsters and the player
	kHDWeapons,			//   weapons in hand, projectiles and items
	kHDScenery,			//   scenery
	kNormalMaps,		// HD walls' normal maps shape the headlight and dynamic lights, as relief does (Metal; Flagship)
	k3DPickups,			// HD art: installed model packs replace item sprites with 3D models (Metal; Flagship)
	kSpinPickups,		//   the 3D pickups turn slowly (a look only; not part of any tier, off by default)
	kAmbientShadows,	// Round 12: screen-space ambient occlusion from the world's depth (Metal display; Flagship)
	kCharacterShadows,	// Round 12: sprites cast their silhouette on the floor (Metal; Flagship)
	kTextureCache,		// HD art kept block-compressed in ~/Library/Caches, built at first load (DurandalTextureCache.h; Metal display; Flagship)
	kWeaponLighting,	// the weapon in hand takes the strength and colour of the dynamic lights around the viewer (Metal; Enhanced)
	kLightBounce,		// R4: redistributed light bounces on, figures take it, the sky gives its own colour (with Light Redistribution; Rampant)
	kTracedShadows,		// R1: figures cast soft shadows from dynamic lights, light no longer leaks between stacked rooms (with Light Shadows; Rampant)
	kReflections,		// R2: water, sewage and goo reflect the room and the sky, traced through the map (with Real Liquids; Rampant)
	kTracedAmbient,		// R3: ambient shadows traced through the map and the figures, not read from the picture (with Ambient Shadows; Rampant)
	kDustEmbers,		// air that moves: dust lit where light falls on it, embers rising off lava (Metal; Rampant)
	kHeatShimmer,		// air that moves: the air over lava wavers (with Bloom; Metal display; Rampant)
	kNumberOfFeatures
};

enum ShadingStyle {
	kShadingBanded = 0,	// the 32 shading tables, as 8-bit Marathon
	kShadingSmooth = 1	// the same ramps, blended between tables
};

struct Preferences {
	int quality_tier;
	bool features[kNumberOfFeatures];
	int shading_style;
	int fog_strength;			// volumetric fog (V1): 0 light, 1 medium, 2 thick
	int gi_strength;			// light redistribution (E4): 0 subtle, 1 medium, 2 strong (how far light may move within a surface)
	int distance_shade;			// Round 12: far surfaces darken, 0..100 percent (0 none); not part of any tier, 0 in Stock
	// Scene grade (Look tab; not part of any tier, neutral in Stock): the
	// world image only, applied in the output pass. Percent: brightness
	// -25..25 (offset), contrast 50..200 (about mid grey), gamma 50..200
	int scene_brightness;
	int scene_contrast;
	int scene_gamma;
	// Testing cheats (DurandalCheats.h); not part of any tier, off in Stock
	bool cheat_level_select;
	bool cheat_god;
	bool cheat_all_weapons;
	bool cheat_noclip;			// walk through walls, monsters and scenery
	// Keys (SDL scancodes) that switch god mode and noclip during a game
	int cheat_god_key;
	int cheat_noclip_key;
	// Key that brings five armed BOBs in beside the player
	int cheat_summon_key;
	// Soundtrack (ART tab; DurandalArt.h): the name of the installed
	// soundtrack plugin that plays, empty for none. Not part of any tier
	// (Stock clears it); exactly one soundtrack plugin is enabled at a time
	std::string soundtrack;
};

// True when enhancements may run at all: always, unless DURANDAL_STOCK=1
// closes the gate (the film tests' stock run, comparisons).
bool Available();

// True when features still awaiting QA may run: Debug builds, and Release
// builds launched with DURANDAL_QA=1 (the shared scheme's Run sets it).
bool QA();

// Features that have passed QA run for everyone; the rest need QA().
bool Released(Feature feature);

// True when the gate is open, the feature is switched on, and it has
// either passed QA or QA is open.
bool Enabled(Feature feature);

// Features the quality tier sets; the rest (HDR output) are independent
// switches.
bool IsTierFeature(Feature feature);

// The lowest tier that turns the feature on (Classic, Enhanced, Flagship or
// Rampant); 0 for independent switches.
int FeatureTier(Feature feature);

Preferences& Prefs();

// Preferences plumbing, called from preferences.cpp.
void SetDefaults();
void Parse(const InfoTree& root);
InfoTree Tree();
// False when the gate is closed and the file had no <durandal> element, so
// a stock-mode launch never writes Durandal settings it didn't read.
bool ShouldWrite();
// Called after the file is read; on the first Durandal run (no <durandal>
// element) with the gate open, applies the Flagship tier.
void AfterRead();

// Sets the tier's features and the upstream settings it owns (frame-rate
// target). Custom leaves everything as it is.
void ApplyTier(int tier);

// The DURANDAL preferences dialog (button hidden when the gate is closed).
void Dialog(void* parent_dialog);

}

#endif
