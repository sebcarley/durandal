/*
	DurandalPreferences.cpp — Durandal project

	See DurandalPreferences.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalPreferences.h"

#include "cseries.h"
#include "DurandalArt.h"
#include "DurandalFetch.h"
#include "DurandalTextureCache.h"
#include "Plugins.h"
#include "DurandalScenario.h"
#include "shell.h"
#include "XML_ParseTreeRoot.h"
#include "XML_LevelScript.h"
#include "map.h"
#include "game_wad.h"
#include "InfoTree.h"
#include "preferences.h"
#include "sdl_dialogs.h"
#include "sdl_widgets.h"
#include "screen.h"
#include "Logging.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace Durandal {

namespace {

// Bump when the stored format changes, and migrate in Parse().
const int kSchema = 2;	// 2 (28 Sep 2026): Spinning Pickups off once for everyone

Preferences prefs;
bool read_durandal_element = false;

const char* const kFeatureAttr[kNumberOfFeatures] = {
	"per_frame_look",
	"smooth_world",
	"graded_occlusion",
	"metal_renderer",
	"hdr_output",
	"widescreen",
	"shading_tables",
	"texel_lighting",
	"crisp_filtering",
	"edge_smoothing",
	"crisp_text",
	"crisp_terminals",
	"glow",
	"bloom",
	"hdr_sky",
	"dynamic_lights",
	"sprite_lighting",
	"light_shadows",
	"liquids",
	"contact_shadows",
	"volumetric_fog",
	"light_redistribution",
	"surface_relief",
	"room_reverb",
	"true_look",
	"sidestep_sway",
	"free_look",
	"hd_walls",
	"hd_monsters",
	"hd_weapons",
	"hd_scenery",
	"normal_maps",
	"models_3d",
	"spin_pickups",
	"ambient_shadows",
	"character_shadows",
	"texture_cache",
	"weapon_lighting",
	"light_bounce",
	"traced_shadows",
	"reflections",
	"traced_ambient",
	"dust_embers",
	"heat_shimmer",
};

}

// Since baseline-7 (29 Sep 2026) what has passed QA runs for everyone,
// however the app is launched; what has not needs QA(). DURANDAL_STOCK=1
// closes the gate altogether.
bool Available()
{
	static const bool stock = [] {
		const char* v = std::getenv("DURANDAL_STOCK");
		return v && std::strcmp(v, "1") == 0;
	}();
	return !stock;
}

bool QA()
{
#ifndef __OPTIMIZE__
	return true;	// Debug build
#else
	static const bool qa = [] {
		const char* v = std::getenv("DURANDAL_QA");
		return v && std::strcmp(v, "1") == 0;
	}();
	return qa;
#endif
}

bool Released(Feature feature)
{
	// Nothing is in QA today: Weapon Takes the Light and the six Rampant
	// features passed on 2 Oct 2026. A new feature goes here as a case
	// returning false until the owner passes it
	switch (feature)
	{
		default:
			return true;
	}
}

bool Enabled(Feature feature)
{
	return Available() && prefs.features[feature] && (Released(feature) || QA());
}

int FeatureTier(Feature feature)
{
	switch (feature)
	{
		case kHDROutput:
		case kSpinPickups:	// the owner's choice (27 Sep 2026): off unless switched on
			return 0;
		case kGlow:
		case kBloom:
		case kHDRSky:
		case kDynamicLights:
		case kSpriteLighting:
		case kLightShadows:
		case kLiquids:
		case kContactShadows:
		case kReverb:
		case kTrueLook:
		case kSidestepSway:
		case kFreeLook:
		case kWeaponLighting:
			return kTierEnhanced;
		case kVolumetricFog:
		case kLightRedistribution:
		case kSurfaceRelief:
		case kHDWalls:
		case kHDMonsters:
		case kHDWeapons:
		case kHDScenery:
		case kNormalMaps:
		case k3DPickups:
		case kAmbientShadows:
		case kCharacterShadows:
		case kTextureCache:
			return kTierFlagship;
		case kLightBounce:
		case kTracedShadows:
		case kReflections:
		case kTracedAmbient:
		case kDustEmbers:
		case kHeatShimmer:
			return kTierRampant;
		default:
			// Including the Metal renderer since Round 3: every Classic look
			// feature needs it, and Stock (OpenGL) is upstream exactly
			return kTierClassic;
	}
}

bool IsTierFeature(Feature feature)
{
	return FeatureTier(feature) != 0;
}

// The named tiers in order, Custom apart; Rampant is stored as 5 and
// ranks above Flagship
static bool TierIncludes(int tier, Feature feature)
{
	return tier != kTierStock && tier != kTierCustom && FeatureTier(feature) != 0 && FeatureTier(feature) <= tier;
}

// Whether the Quality menu offers Rampant: once any of its features has
// passed QA, or while QA is open
static bool RampantOffered()
{
	if (QA())
		return true;
	for (int i = 0; i < kNumberOfFeatures; ++i)
		if (FeatureTier(Feature(i)) == kTierRampant && Released(Feature(i)))
			return true;
	return false;
}

Preferences& Prefs()
{
	return prefs;
}

void SetDefaults()
{
	// Defaults describe the Classic tier; they only take effect once
	// AfterRead() or the dialog applies a tier, or the gate is open.
	prefs.quality_tier = kTierClassic;
	for (int i = 0; i < kNumberOfFeatures; ++i)
		prefs.features[i] = IsTierFeature(Feature(i));
	prefs.shading_style = kShadingSmooth;
	prefs.fog_strength = 1;
	prefs.gi_strength = 1;
	prefs.distance_shade = 0;
	prefs.scene_brightness = 0;
	prefs.scene_contrast = prefs.scene_gamma = 100;
	prefs.cheat_level_select = prefs.cheat_god = prefs.cheat_all_weapons = prefs.cheat_noclip = false;
	prefs.cheat_god_key = SDL_SCANCODE_G;
	prefs.cheat_noclip_key = SDL_SCANCODE_N;
	prefs.cheat_summon_key = SDL_SCANCODE_C;	// unbound by default and in the owner's keys
	prefs.soundtrack.clear();
	prefs.hud_style = 0;
	read_durandal_element = false;
}

void Parse(const InfoTree& root)
{
	read_durandal_element = true;
	int schema = 0;
	root.read_attr("schema", schema);
	int tier = prefs.quality_tier;
	if (root.read_attr("quality_tier", tier) &&
		(tier == kTierStock || tier == kTierClassic || tier == kTierEnhanced || tier == kTierFlagship ||
		 tier == kTierCustom || tier == kTierRampant))
		prefs.quality_tier = tier;
	for (int i = 0; i < kNumberOfFeatures; ++i)
	{
		// A feature newer than the file: on in Custom (the tester's tier) and
		// in the tiers that include it
		if (!root.read_attr(kFeatureAttr[i], prefs.features[i]))
			prefs.features[i] = IsTierFeature(Feature(i)) &&
				(prefs.quality_tier == kTierCustom || TierIncludes(prefs.quality_tier, Feature(i)));
	}
	// Schema 2: Spinning Pickups was a tier feature for a day, so older
	// files have it on; it was asked for off (28 Sep 2026)
	if (schema < 2)
		prefs.features[kSpinPickups] = false;
	int style = prefs.shading_style;
	if (root.read_attr("shading_style", style) && (style == kShadingBanded || style == kShadingSmooth))
		prefs.shading_style = style;
	int fog = prefs.fog_strength;
	if (root.read_attr("fog_strength", fog) && fog >= 0 && fog <= 2)
		prefs.fog_strength = fog;
	int gi = prefs.gi_strength;
	if (root.read_attr("gi_strength", gi) && gi >= 0 && gi <= 2)
		prefs.gi_strength = gi;
	int shade;
	if (root.read_attr("distance_shade", shade))
		prefs.distance_shade = std::clamp(shade, 0, 100);
	int grade;
	if (root.read_attr("scene_brightness", grade))
		prefs.scene_brightness = std::clamp(grade, -25, 25);
	if (root.read_attr("scene_contrast", grade))
		prefs.scene_contrast = std::clamp(grade, 50, 200);
	if (root.read_attr("scene_gamma", grade))
		prefs.scene_gamma = std::clamp(grade, 50, 200);
	root.read_attr("cheat_level_select", prefs.cheat_level_select);
	root.read_attr("cheat_god", prefs.cheat_god);
	root.read_attr("cheat_all_weapons", prefs.cheat_all_weapons);
	root.read_attr("cheat_noclip", prefs.cheat_noclip);
	int key;
	if (root.read_attr("cheat_god_key", key) && key >= 0 && key < SDL_NUM_SCANCODES)
		prefs.cheat_god_key = key;
	if (root.read_attr("cheat_noclip_key", key) && key >= 0 && key < SDL_NUM_SCANCODES)
		prefs.cheat_noclip_key = key;
	if (root.read_attr("cheat_summon_key", key) && key >= 0 && key < SDL_NUM_SCANCODES)
		prefs.cheat_summon_key = key;
	root.read_attr("soundtrack", prefs.soundtrack);
	int hud = prefs.hud_style;
	if (root.read_attr("hud_style", hud) && hud >= 0 && hud <= 2)
		prefs.hud_style = hud;
}

InfoTree Tree()
{
	InfoTree root;
	root.put_attr("schema", kSchema);
	root.put_attr("quality_tier", prefs.quality_tier);
	for (int i = 0; i < kNumberOfFeatures; ++i)
		root.put_attr(kFeatureAttr[i], prefs.features[i]);
	root.put_attr("shading_style", prefs.shading_style);
	root.put_attr("fog_strength", prefs.fog_strength);
	root.put_attr("gi_strength", prefs.gi_strength);
	root.put_attr("distance_shade", prefs.distance_shade);
	root.put_attr("scene_brightness", prefs.scene_brightness);
	root.put_attr("scene_contrast", prefs.scene_contrast);
	root.put_attr("scene_gamma", prefs.scene_gamma);
	root.put_attr("cheat_level_select", prefs.cheat_level_select);
	root.put_attr("cheat_god", prefs.cheat_god);
	root.put_attr("cheat_all_weapons", prefs.cheat_all_weapons);
	root.put_attr("cheat_noclip", prefs.cheat_noclip);
	root.put_attr("cheat_god_key", prefs.cheat_god_key);
	root.put_attr("cheat_noclip_key", prefs.cheat_noclip_key);
	root.put_attr("cheat_summon_key", prefs.cheat_summon_key);
	root.put_attr("soundtrack", prefs.soundtrack);
	root.put_attr("hud_style", prefs.hud_style);
	return root;
}

bool ShouldWrite()
{
	return read_durandal_element || Available();
}

void AfterRead()
{
	if (!read_durandal_element && Available())
	{
		logNote("Durandal: first run, applying the Flagship tier");
		ApplyTier(kTierFlagship);
	}

	// Development runs (benchmark, menu shot; they never write preferences):
	// DURANDAL_SET="attr=value,..." overrides settings by their stored names
	const char* set = std::getenv("DURANDAL_SET");
	if (set && (std::getenv("DURANDAL_BENCHMARK") || std::getenv("DURANDAL_MENU_SHOT")))
	{
		std::string all(set);
		size_t pos = 0;
		while (pos < all.size())
		{
			size_t end = all.find(',', pos);
			if (end == std::string::npos)
				end = all.size();
			const std::string item = all.substr(pos, end - pos);
			const size_t eq = item.find('=');
			if (eq != std::string::npos)
			{
				const std::string name = item.substr(0, eq);
				const int value = std::atoi(item.c_str() + eq + 1);
				if (name == "quality_tier" && value >= kTierStock && value <= kTierRampant)
					ApplyTier(value);	// put first: later items override
				if (name == "shading_style")
					prefs.shading_style = value;
				if (name == "fog_strength")
					prefs.fog_strength = std::clamp(value, 0, 2);
				if (name == "gi_strength")
					prefs.gi_strength = std::clamp(value, 0, 2);
				if (name == "distance_shade")
					prefs.distance_shade = std::clamp(value, 0, 100);
				if (name == "scene_brightness")
					prefs.scene_brightness = std::clamp(value, -25, 25);
				if (name == "scene_contrast")
					prefs.scene_contrast = std::clamp(value, 50, 200);
				if (name == "scene_gamma")
					prefs.scene_gamma = std::clamp(value, 50, 200);
				if (name == "cheat_god")
					prefs.cheat_god = value != 0;
				if (name == "cheat_all_weapons")
					prefs.cheat_all_weapons = value != 0;
				if (name == "cheat_noclip")
					prefs.cheat_noclip = value != 0;
				if (name == "soundtrack")
					prefs.soundtrack = item.substr(eq + 1);
				if (name == "hud_style")
					prefs.hud_style = std::clamp(value, 0, 2);
				for (int i = 0; i < kNumberOfFeatures; ++i)
					if (name == kFeatureAttr[i])
						prefs.features[i] = value != 0;
			}
			pos = end + 1;
		}
	}

	// HD art: the installed packs follow the switches (DurandalArt.h)
	DurandalArt::Apply();
}

void ApplyTier(int tier)
{
	prefs.quality_tier = tier;
	switch (tier)
	{
		case kTierStock:
			for (int i = 0; i < kNumberOfFeatures; ++i)
				if (IsTierFeature(Feature(i)))
					prefs.features[i] = false;
			graphics_preferences->fps_target = 30;	// upstream default
			prefs.cheat_level_select = prefs.cheat_god = prefs.cheat_all_weapons = prefs.cheat_noclip = false;
			prefs.scene_brightness = 0;
			prefs.scene_contrast = prefs.scene_gamma = 100;
			prefs.distance_shade = 0;
			prefs.soundtrack.clear();
			break;
		case kTierClassic:
		case kTierEnhanced:
		case kTierFlagship:
		case kTierRampant:
			for (int i = 0; i < kNumberOfFeatures; ++i)
				if (IsTierFeature(Feature(i)))
					prefs.features[i] = TierIncludes(tier, Feature(i));
			prefs.shading_style = kShadingSmooth;
			// Redistribution Strength: Rampant lets light move furthest
			// (Strong); the others keep the Round 8 look (Medium)
			prefs.gi_strength = tier == kTierRampant ? 2 : 1;
			graphics_preferences->fps_target = 0;	// uncapped, paced by vsync
			break;
		default:
			break;
	}
}

// Which tab of the dialog each feature sits on
enum Tab { kTabFeel, kTabLook, kTabArt, kTabLight, kTabRampant, kNumberOfTabs };

static Tab FeatureTab(Feature feature)
{
	switch (feature)
	{
		case kPerFrameLook:
		case kSmoothWorld:
		case kGradedOcclusion:
		case kReverb:
		case kWidescreen:
		case kTrueLook:
		case kSidestepSway:
		case kFreeLook:
			return kTabFeel;
		case kHDROutput:
		case kWeaponLighting:
		case kGlow:
		case kBloom:
		case kHDRSky:
		case kDynamicLights:
		case kSpriteLighting:
		case kLightShadows:
		case kLiquids:
		case kContactShadows:
		case kVolumetricFog:
		case kLightRedistribution:
		case kAmbientShadows:
		case kCharacterShadows:
			return kTabLight;
		case kHDWalls:
		case kHDMonsters:
		case kHDWeapons:
		case kHDScenery:
		case kNormalMaps:
		case k3DPickups:
		case kSpinPickups:
		case kTextureCache:
			return kTabArt;
		case kLightBounce:
		case kTracedShadows:
		case kReflections:
		case kTracedAmbient:
		case kDustEmbers:
		case kHeatShimmer:
			return kTabRampant;
		default:
			return kTabLook;
	}
}

// Get HD Art (ART tab; DurandalFetch.h): the community's packs fetched
// from their authors' pages and installed, with a line per pack. New packs
// are added to the plugin list and switched as the HD Art switches say;
// their art loads from the next level
static void GetHDArtDialog()
{
	const std::vector<DurandalFetch::PackInfo>& packs = DurandalFetch::Packs();
	int megabytes = 0;
	const int missing = DurandalFetch::Missing(&megabytes);
	const int64_t free_mb = DurandalFetch::FreeMegabytes();
	const bool room = free_mb < 0 || free_mb > int64_t(megabytes) * 3 + 500;	// archive, unpacked copy, margin

	dialog d;
	vertical_placer* placer = new vertical_placer;
	placer->dual_add(new w_title("GET HD ART"), d);
	placer->add(new w_spacer(), true);
	static char intro[96];
	snprintf(intro, sizeof(intro), "The community's HD art for %s: walls and sky, monsters,", DurandalScenario::Name(DurandalScenario::Current()));
	placer->dual_add(new w_static_text(intro), d);
	placer->dual_add(new w_static_text("scenery, weapons and 3D models, from its authors' own pages on"), d);
	placer->dual_add(new w_static_text("Simplici7y. It is theirs: Durandal fetches it but never ships it."), d);
	placer->add(new w_spacer(), true);
	// Each line starts as wide as it will get (a static text keeps its first width)
	std::vector<w_static_text*> lines;
	for (const DurandalFetch::PackInfo& pack : packs)
	{
		DurandalFetch::PackStatus s;
		s.state = DurandalFetch::IsInstalled(pack) ? DurandalFetch::State::kInstalledAlready : DurandalFetch::State::kWaiting;
		std::string text = DurandalFetch::Describe(pack, s);
		text.resize(std::max<size_t>(text.size(), 68), ' ');
		w_static_text* line = new w_static_text(text.c_str());
		lines.push_back(line);
		placer->dual_add(line, d);
	}
	placer->add(new w_spacer(), true);
	char summary_text[160];
	if (missing == 0)
		snprintf(summary_text, sizeof(summary_text), "All five packs are installed.");
	else if (!room)
		snprintf(summary_text, sizeof(summary_text), "Not enough free space: about %d GB is needed.", (megabytes * 3 + 500 + 1023) / 1024);
	else
		snprintf(summary_text, sizeof(summary_text), "%d to fetch, about %d MB. They go in Application Support/%s/Plugins.",
				 missing, megabytes, get_application_name().c_str());
	std::string padded(summary_text);
	padded.resize(std::max<size_t>(padded.size(), 72), ' ');
	w_static_text* summary = new w_static_text(padded.c_str());
	placer->dual_add(summary, d);
	placer->add(new w_spacer(), true);

	bool started = false, applied = false, close_when_done = false;
	horizontal_placer* buttons = new horizontal_placer;
	w_button* download = new w_button("DOWNLOAD", [&](void*) {
		if (started || missing == 0 || !room)
			return;
		started = true;
		DurandalFetch::Start();
		summary->set_text("Fetching. Close cancels; packs already installed stay.");
	}, nullptr);
	download->set_enabled(missing > 0 && room);
	buttons->dual_add(download, d);
	buttons->dual_add(new w_button("CLOSE", [&](void*) {
		if (started && !applied)
		{
			close_when_done = true;
			DurandalFetch::Cancel();
			summary->set_text("Cancelling...");
			return;
		}
		d.quit(0);
	}, nullptr), d);
	placer->add(buttons, true);
	d.set_widget_placer(placer);

	d.set_processing_function([&](dialog*) {
		if (!started || applied)
			return;
		const DurandalFetch::Status status = DurandalFetch::Snapshot();
		for (size_t i = 0; i < packs.size() && i < status.packs.size(); ++i)
			lines[i]->set_text(DurandalFetch::Describe(packs[i], status.packs[i]).c_str());
		if (!status.finished)
			return;
		applied = true;
		download->set_enabled(false);
		// The new packs join the plugin list and follow the HD Art switches
		for (const std::string& folder : status.new_folders)
			Plugins::instance()->add_directory(folder);
		DurandalArt::Rescan();
		DurandalArt::Apply();
		int installed = 0, failed = 0;
		for (const DurandalFetch::PackStatus& s : status.packs)
		{
			installed += s.state == DurandalFetch::State::kInstalled ? 1 : 0;
			failed += s.state == DurandalFetch::State::kFailed ? 1 : 0;
		}
		char done_text[160];
		if (failed)
			snprintf(done_text, sizeof(done_text), "%d installed, %d not (see above; scripts/get-hd-art.sh or by hand).", installed, failed);
		else
			snprintf(done_text, sizeof(done_text), "%d installed. The HD art applies from the next level.", installed);
		summary->set_text(done_text);
		if (close_when_done)
			d.quit(0);
	});
	d.run();
}

void Dialog(void* parent_dialog)
{
	dialog d;
	vertical_placer* placer = new vertical_placer;
	placer->dual_add(new w_title("DURANDAL"), d);
	placer->add(new w_spacer(), true);

	// Rampant sits between Flagship and Custom when it is offered
	const bool rampant = RampantOffered();
	static const char* tier_labels_all[] = { "Stock", "Classic", "Enhanced", "Flagship", "Rampant", "Custom", nullptr };
	static const char* tier_labels_four[] = { "Stock", "Classic", "Enhanced", "Flagship", "Custom", nullptr };
	const char** tier_labels = rampant ? tier_labels_all : tier_labels_four;
	std::vector<int> index_to_tier = { kTierStock, kTierClassic, kTierEnhanced, kTierFlagship };
	if (rampant)
		index_to_tier.push_back(kTierRampant);
	index_to_tier.push_back(kTierCustom);
	auto tier_to_index = [&](int tier) {
		for (size_t i = 0; i < index_to_tier.size(); ++i)
			if (index_to_tier[i] == tier)
				return int(i);
		return int(index_to_tier.size()) - 1;	// Custom (and Rampant when it is not offered)
	};

	table_placer* top = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	top->col_flags(0, placeable::kAlignRight);
	w_select* tier_w = new w_select(tier_to_index(prefs.quality_tier), tier_labels);
	top->dual_add(tier_w->label("Quality"), d);
	top->dual_add(tier_w, d);
	placer->add(top, true);
	placer->add(new w_spacer(), true);

	tab_placer* tabs = new tab_placer();
	std::vector<std::string> tab_labels = { "FEEL", "LOOK", "ART", "LIGHT", "RAMPANT", "CHEATS" };
	w_tab* tab_w = new w_tab(tab_labels, tabs);
	placer->dual_add(tab_w, d);
	placer->add(new w_spacer(), true);

	table_placer* tables[kNumberOfTabs];
	for (int t = 0; t < kNumberOfTabs; ++t)
	{
		tables[t] = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
		tables[t]->col_flags(0, placeable::kAlignRight);
	}

	static const char* feature_labels[kNumberOfFeatures] = {
		"Per-frame Mouse Look",
		"Smooth Liquids, Lights, Fades",
		"Graded Sound Occlusion",
		"Metal Renderer (on next launch)",
		"HDR Output",
		"Widescreen",
		"Marathon Shading",
		"Texel Lighting",
		"Crisp Filtering",
		"Smooth Edges",
		"Crisp Text",
		"Crisp Terminals",
		"Glow (with HDR Output)",
		"Bloom",
		"HDR Sky",
		"Dynamic Lights",
		"Ceiling Light on Sprites",
		"Light Shadows",
		"Real Liquids",
		"Contact Shadows",
		"Volumetric Fog",
		"Light Redistribution",
		"Surface Relief",
		"Room Reverb",
		"True Look Up/Down",
		"Sidestep Sway",
		"Free Look Beyond Aim",
		"HD Walls and Sky",
		"HD Monsters",
		"HD Weapons and Items",
		"HD Scenery",
		"Normal Maps (HD walls)",
		"3D Pickups (model packs)",
		"Spinning Pickups",
		"Ambient Shadows",
		"Character Shadows",
		"Texture Cache (compressed HD art)",
		"Weapon Takes the Light",
		"Bounced Light",
		"Traced Shadows",
		"Reflecting Liquids",
		"Traced Ambient Shadows",
		"Dust and Embers",
		"Heat Shimmer",
	};
	w_toggle* feature_w[kNumberOfFeatures];
	w_select* style_w = nullptr;
	w_select* fog_w = nullptr;
	w_select* gi_w = nullptr;
	for (int i = 0; i < kNumberOfFeatures; ++i)
	{
		// A feature still in QA has no switch unless QA is open
		feature_w[i] = nullptr;
		if (!Released(Feature(i)) && !QA())
			continue;
		feature_w[i] = new w_toggle(prefs.features[i]);
		table_placer* table = tables[FeatureTab(Feature(i))];
		table->dual_add(feature_w[i]->label(feature_labels[i]), d);
		table->dual_add(feature_w[i], d);
		if (i == kShadingTables)
		{
			static const char* style_labels[] = { "Banded", "Smooth", nullptr };
			style_w = new w_select(prefs.shading_style, style_labels);
			table->dual_add(style_w->label("Shading Style"), d);
			table->dual_add(style_w, d);
		}
		if (i == kVolumetricFog)
		{
			static const char* fog_labels[] = { "Light", "Medium", "Thick", nullptr };
			fog_w = new w_select(prefs.fog_strength, fog_labels);
			table->dual_add(fog_w->label("Fog Strength"), d);
			table->dual_add(fog_w, d);
		}
		if (i == kLightRedistribution)
		{
			static const char* gi_labels[] = { "Subtle", "Medium", "Strong", nullptr };
			gi_w = new w_select(prefs.gi_strength, gi_labels);
			table->dual_add(gi_w->label("Redistribution Strength"), d);
			table->dual_add(gi_w, d);
		}
	}
	// Scene grade: brightness, contrast and gamma of the world image
	class w_grade_slider : public w_slider {
	public:
		w_grade_slider(int items, int sel, int base, int step, bool signed_percent)
			: w_slider(items, sel), base(base), step(step), signed_percent(signed_percent) { init_formatted_value(); }
		int value() const { return base + selection * step; }
		std::string formatted_value() override {
			char text[16];
			if (signed_percent)
				snprintf(text, sizeof(text), "%+d%%", value());
			else
				snprintf(text, sizeof(text), "%d.%02d", value() / 100, value() % 100);
			return text;
		}
	private:
		int base, step;
		bool signed_percent;
	};
	w_grade_slider* brightness_w = new w_grade_slider(11, (prefs.scene_brightness + 25) / 5, -25, 5, true);
	w_grade_slider* contrast_w = new w_grade_slider(16, (prefs.scene_contrast - 50) / 10, 50, 10, false);
	w_grade_slider* gamma_w = new w_grade_slider(16, (prefs.scene_gamma - 50) / 10, 50, 10, false);
	tables[kTabLook]->dual_add(brightness_w->label("Scene Brightness"), d);
	tables[kTabLook]->dual_add(brightness_w, d);
	tables[kTabLook]->dual_add(contrast_w->label("Scene Contrast"), d);
	tables[kTabLook]->dual_add(contrast_w, d);
	tables[kTabLook]->dual_add(gamma_w->label("Scene Gamma"), d);
	tables[kTabLook]->dual_add(gamma_w, d);
	// Distance shade (Round 12): far surfaces darken, so long halls recede
	w_grade_slider* shade_w = new w_grade_slider(11, prefs.distance_shade / 10, 0, 10, true);
	tables[kTabLook]->dual_add(shade_w->label("Distance Shade"), d);
	tables[kTabLook]->dual_add(shade_w, d);
	// Marathon's HUD (QA passed 4 Oct 2026): with or without the frame round the view
	w_select* hud_w = nullptr;
	if (DurandalScenario::Marathon1() && Available())
	{
		static const char* hud_labels[] = { "Classic", "Basic (no frame)", "Enhanced", nullptr };
		hud_w = new w_select(prefs.hud_style, hud_labels);
		tables[kTabLook]->dual_add(hud_w->label("HUD"), d);
		tables[kTabLook]->dual_add(hud_w, d);
	}
	tables[kTabLook]->add_row(new w_spacer(), true);
	tables[kTabLook]->dual_add_row(new w_static_text("Everything here but Crisp Terminals needs"), d);
	tables[kTabLook]->dual_add_row(new w_static_text("the Metal renderer; relaunch after switching it."), d);
	tables[kTabLight]->add_row(new w_spacer(), true);
	tables[kTabLight]->dual_add_row(new w_static_text("Metal renderer only. Glow shows on an HDR display."), d);
	// RAMPANT: the fifth tier's own switches (docs/PLAN-fifth-tier.md)
	tables[kTabRampant]->add_row(new w_spacer(), true);
	tables[kTabRampant]->dual_add_row(new w_static_text("More than this machine was built for. Metal renderer only."), d);
	tables[kTabRampant]->dual_add_row(new w_static_text("Bounced Light needs Light Redistribution;"), d);
	tables[kTabRampant]->dual_add_row(new w_static_text("Traced Shadows need Dynamic Lights and Light Shadows;"), d);
	tables[kTabRampant]->dual_add_row(new w_static_text("Reflecting Liquids need Real Liquids;"), d);
	tables[kTabRampant]->dual_add_row(new w_static_text("Traced Ambient Shadows need Ambient Shadows;"), d);
	tables[kTabRampant]->dual_add_row(new w_static_text("Heat Shimmer needs Bloom."), d);
	// ART: the packs installed for each category (DurandalArt.h). The art
	// is not shipped; the packs go in the user's Plugins folder
	tables[kTabArt]->add_row(new w_spacer(), true);
	static std::string art_lines[DurandalArt::kNumberOfCategories];
	for (int c = 0; c < DurandalArt::kNumberOfCategories; ++c)
	{
		std::string installed = DurandalArt::Installed(DurandalArt::Category(c));
		if (installed.empty())
			installed = "none installed";
		else if (installed.size() > 34)
			installed = installed.substr(0, 31) + "...";
		art_lines[c] = std::string(DurandalArt::CategoryName(DurandalArt::Category(c))) + ": " + installed;
		tables[kTabArt]->dual_add_row(new w_static_text(art_lines[c].c_str()), d);
	}
	// Soundtrack: one of the installed soundtrack plugins, or none
	static std::vector<std::string> soundtrack_names;
	static std::vector<const char*> soundtrack_labels;
	soundtrack_names.clear();
	soundtrack_labels.clear();
	soundtrack_labels.push_back("None");
	int soundtrack_selection = 0;
	for (const DurandalArt::Soundtrack& st : DurandalArt::Soundtracks())
	{
		if (st.name == prefs.soundtrack)
			soundtrack_selection = int(soundtrack_names.size()) + 1;
		soundtrack_names.push_back(st.name.size() > 30 ? st.name.substr(0, 27) + "..." : st.name);
	}
	for (const std::string& name : soundtrack_names)
		soundtrack_labels.push_back(name.c_str());
	soundtrack_labels.push_back(nullptr);
	w_select* soundtrack_w = new w_select(soundtrack_selection, soundtrack_labels.data());
	tables[kTabArt]->dual_add(soundtrack_w->label("Soundtrack"), d);
	tables[kTabArt]->dual_add(soundtrack_w, d);
	tables[kTabArt]->add_row(new w_spacer(), true);
	tables[kTabArt]->dual_add_row(new w_static_text("Art needs the Metal renderer; art and soundtrack apply"), d);
	tables[kTabArt]->dual_add_row(new w_static_text("at the next level (a Lua soundtrack at the next new game)."), d);
	static char packs_line[96];
	snprintf(packs_line, sizeof(packs_line), "Packs go in Application Support/%s/Plugins.", get_application_name().c_str());
	tables[kTabArt]->dual_add_row(new w_static_text(packs_line), d);
	// Get HD Art (QA passed 3 Oct 2026): the community's packs fetched and installed
	// Each game has its own pack list; a game without one (Marathon, until
	// its packs are chosen) does not offer the button
	if (Available() && !DurandalFetch::Packs().empty())
		tables[kTabArt]->dual_add_row(new w_button("GET HD ART...", [](void*) { GetHDArtDialog(); }, nullptr), d);
	// Texture cache: what the first loads have built so far
	static char cache_line[96];
	{
		size_t files = 0;
		uint64_t bytes = 0;
		DurandalTextureCache::Stats(files, bytes);
		if (files)
			snprintf(cache_line, sizeof(cache_line), "Texture cache: %zu images, %.1f GB in Library/Caches/Durandal.", files, double(bytes) / (1024.0 * 1024 * 1024));
		else
			snprintf(cache_line, sizeof(cache_line), "Texture cache: empty; each level's first load fills it.");
	}
	tables[kTabArt]->dual_add_row(new w_static_text(cache_line), d);

	// Testing cheats (DurandalCheats.h)
	table_placer* cheats = new table_placer(2, get_theme_space(ITEM_WIDGET), true);
	cheats->col_flags(0, placeable::kAlignRight);
	w_toggle* level_w = new w_toggle(prefs.cheat_level_select);
	cheats->dual_add(level_w->label("Level Select on New Game"), d);
	cheats->dual_add(level_w, d);
	w_toggle* god_w = new w_toggle(prefs.cheat_god);
	cheats->dual_add(god_w->label("God Mode"), d);
	cheats->dual_add(god_w, d);
	w_toggle* weapons_w = new w_toggle(prefs.cheat_all_weapons);
	cheats->dual_add(weapons_w->label("All Weapons and Ammo"), d);
	cheats->dual_add(weapons_w, d);
	w_toggle* noclip_w = new w_toggle(prefs.cheat_noclip);
	cheats->dual_add(noclip_w->label("Noclip (through walls)"), d);
	cheats->dual_add(noclip_w, d);
	// Keys that switch god mode and noclip during a game (click, press a key)
	w_key* god_key_w = new w_key(SDL_Scancode(prefs.cheat_god_key), w_key::KeyboardKey);
	cheats->dual_add(god_key_w->label("God Mode Key"), d);
	cheats->dual_add(god_key_w, d);
	w_key* noclip_key_w = new w_key(SDL_Scancode(prefs.cheat_noclip_key), w_key::KeyboardKey);
	cheats->dual_add(noclip_key_w->label("Noclip Key"), d);
	cheats->dual_add(noclip_key_w, d);
	w_key* summon_key_w = new w_key(SDL_Scancode(prefs.cheat_summon_key), w_key::KeyboardKey);
	cheats->dual_add(summon_key_w->label("Summon BOBs Key"), d);
	cheats->dual_add(summon_key_w, d);
	cheats->add_row(new w_spacer(), true);
	cheats->dual_add_row(new w_static_text("Single player only. God Mode, All Weapons, Noclip"), d);
	cheats->dual_add_row(new w_static_text("and Summon BOBs stop that game's film recording."), d);

	for (int t = 0; t < kNumberOfTabs; ++t)
		tabs->add(tables[t], true);
	tabs->add(cheats, true);
	placer->add(tabs, true);
	placer->add(new w_spacer(), true);
	// Development: DURANDAL_MENU_SHOT_TAB=<n> opens a dialog shot on tab n
	// (the placer only; the tab row still highlights the first tab)
	if (std::getenv("DURANDAL_MENU_SHOT"))
		if (const char* tab = std::getenv("DURANDAL_MENU_SHOT_TAB"))
			tabs->choose_tab(std::clamp(std::atoi(tab), 0, int(kNumberOfTabs)));

	// Choosing a tier sets the toggles; touching a toggle makes it Custom.
	tier_w->set_selection_changed_callback([&](void*) {
		int tier = index_to_tier[tier_w->get_selection()];
		if (tier == kTierCustom)
			return;
		for (int i = 0; i < kNumberOfFeatures; ++i)
			if (feature_w[i] && IsTierFeature(Feature(i)))
				feature_w[i]->set_selection(TierIncludes(tier, Feature(i)) ? 1 : 0);
		// As ApplyTier: Rampant's light moves furthest
		if (gi_w && tier != kTierStock)
			gi_w->set_selection(tier == kTierRampant ? 2 : 1);
	});
	for (int i = 0; i < kNumberOfFeatures; ++i)
		if (feature_w[i] && IsTierFeature(Feature(i)))
			feature_w[i]->set_selection_changed_callback([&](void*) {
			tier_w->set_selection(tier_to_index(kTierCustom));
		});

	horizontal_placer* button_placer = new horizontal_placer;
	button_placer->dual_add(new w_button("ACCEPT", dialog_ok, &d), d);
	button_placer->dual_add(new w_button("CANCEL", dialog_cancel, &d), d);
	placer->add(button_placer, true);

	d.set_widget_placer(placer);
	clear_screen();

	if (d.run() == 0)
	{
		int tier = index_to_tier[tier_w->get_selection()];
		if (tier != kTierCustom)
		{
			ApplyTier(tier);
		}
		else
		{
			prefs.quality_tier = kTierCustom;
			for (int i = 0; i < kNumberOfFeatures; ++i)
				if (feature_w[i] && IsTierFeature(Feature(i)))
					prefs.features[i] = feature_w[i]->get_selection() != 0;
		}
		for (int i = 0; i < kNumberOfFeatures; ++i)
			if (feature_w[i] && !IsTierFeature(Feature(i)))
				prefs.features[i] = feature_w[i]->get_selection() != 0;
		if (tier == kTierCustom || tier == kTierStock)
			prefs.shading_style = style_w ? style_w->get_selection() : prefs.shading_style;
		if (fog_w)
			prefs.fog_strength = fog_w->get_selection();
		if (gi_w)
			prefs.gi_strength = gi_w->get_selection();
		prefs.distance_shade = shade_w->value();
		if (hud_w)
			prefs.hud_style = hud_w->get_selection();
		prefs.scene_brightness = brightness_w->value();
		prefs.scene_contrast = contrast_w->value();
		prefs.scene_gamma = gamma_w->value();
		if (tier != kTierStock)
		{
			prefs.cheat_level_select = level_w->get_selection() != 0;
			prefs.cheat_god = god_w->get_selection() != 0;
			prefs.cheat_all_weapons = weapons_w->get_selection() != 0;
			prefs.cheat_noclip = noclip_w->get_selection() != 0;
		}
		prefs.cheat_god_key = god_key_w->get_key();
		prefs.cheat_noclip_key = noclip_key_w->get_key();
		prefs.cheat_summon_key = summon_key_w->get_key();
		if (tier != kTierStock)
		{
			const int choice = soundtrack_w->get_selection();
			prefs.soundtrack = (choice > 0 && choice <= int(DurandalArt::Soundtracks().size()))
				? DurandalArt::Soundtracks()[choice - 1].name : std::string();
		}
		// HD art: switch the packs before the file is written, so it
		// records their state; then, as the Plugins dialog does, re-read
		// every MML so the next level uses (or drops) their art
		const bool art_changed = DurandalArt::Apply();
		write_preferences();
		if (art_changed)
		{
			ResetAllMMLValues();
			LoadBaseMMLScripts(true);
			Plugins::instance()->load_mml(true);
			Plugins::instance()->set_map_checksum(get_current_map_checksum());
			LoadLevelScripts(get_map_file());
		}
	}
}

}
