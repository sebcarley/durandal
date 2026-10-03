/*
	DurandalArt.cpp — Durandal project

	See DurandalArt.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalArt.h"

#include "cseries.h"
#include "FileHandler.h"
#include "Logging.h"
#include "Plugins.h"

#include <cstdlib>

namespace DurandalArt {

namespace {

std::vector<Pack> packs;
std::vector<Soundtrack> soundtracks;
bool scanned = false;

// The text of a file in the plugin's folder, empty if it cannot be read
std::string read_plugin_file(const Plugin& plugin, const std::string& path)
{
	FileSpecifier file = plugin.directory + path;
	OpenedFile opened;
	int32 length = 0;
	if (!file.Open(opened) || !opened.GetLength(length) || length <= 0)
		return std::string();
	std::string text(size_t(length), '\0');
	if (!opened.Read(length, &text[0]))
		return std::string();
	return text;
}

// Marathon 2's collections by what they hold (the interface, 0, is left
// out: HUD packs are not art in this sense)
int category_of(int collection)
{
	switch (collection)
	{
		case 1: case 4: case 7:
			return kWeapons;
		case 2: case 3: case 5: case 6: case 8: case 9: case 10: case 11:
		case 12: case 13: case 14: case 15: case 16: case 31:
			return kMonsters;
		case 17: case 18: case 19: case 20: case 21:
		case 27: case 28: case 29: case 30:
			return kWalls;
		case 22: case 23: case 24: case 25: case 26:
			return kScenery;
		default:
			return -1;
	}
}

// Scans an MML file's text for <texture> elements that name an image and
// <model> elements that name a model, without a full parse (CFP Monsters
// declares 4,000 of them): each element's coll="n" gives the category.
void scan_mml(const std::string& text, Pack& pack)
{
	size_t pos = 0;
	while ((pos = text.find('<', pos)) != std::string::npos)
	{
		const size_t end = text.find('>', pos);
		if (end == std::string::npos)
			break;
		const std::string element = text.substr(pos, end - pos);
		pos = end + 1;
		const bool texture = element.compare(0, 8, "<texture") == 0 && element.find("normal_image") != std::string::npos;
		const bool model = element.compare(0, 6, "<model") == 0 && element.find("file=") != std::string::npos;
		if (!texture && !model)
			continue;
		const size_t c = element.find("coll=\"");
		if (c == std::string::npos)
			continue;
		const int category = model ? kModels : category_of(std::atoi(element.c_str() + c + 6));
		if (category < 0)
			continue;
		pack.entries[category]++;
		if (texture && element.find("offset_image") != std::string::npos)
			pack.normal_maps = true;
	}
}

void scan()
{
	if (scanned)
		return;
	scanned = true;
	for (Plugin& plugin : *Plugins::instance())
	{
		Pack pack;
		pack.name = plugin.name;
		pack.version = plugin.version;
		pack.path = plugin.directory.GetPath();
		for (int c = 0; c < kNumberOfCategories; ++c)
			pack.entries[c] = 0;
		pack.category = kWalls;
		pack.normal_maps = false;
		bool music_mml = false;
		for (const std::string& mml : plugin.mmls)
		{
			const std::string text = read_plugin_file(plugin, mml);
			scan_mml(text, pack);
			if (text.find("<music") != std::string::npos)
				music_mml = true;
		}
		// Soundtracks: a map patch whose resources name <music> tracks, a
		// solo Lua script that uses the Music API, or MML with <music>
		const char* kind = nullptr;
		for (const MapPatch& patch : plugin.map_patches)
			for (const auto& resource : patch.resource_map)
				if (!kind && read_plugin_file(plugin, resource.second).find("<music") != std::string::npos)
					kind = "map patch";
		if (!kind && !plugin.solo_lua.empty())
		{
			const std::string lua = read_plugin_file(plugin, plugin.solo_lua);
			if (lua.find("Music.new") != std::string::npos || lua.find("Music.play") != std::string::npos)
				kind = "Lua";
		}
		if (!kind && music_mml)
			kind = "MML";
		if (kind)
		{
			soundtracks.push_back({ plugin.name, plugin.version, pack.path, kind });
			logNote("Durandal art: soundtrack %s %s (%s)", plugin.name.c_str(), plugin.version.c_str(), kind);
		}
		int total = 0;
		for (int c = 0; c < kNumberOfCategories; ++c)
		{
			total += pack.entries[c];
			if (pack.entries[c] > pack.entries[pack.category])
				pack.category = Category(c);
		}
		if (!total)
			continue;
		std::string what;
		for (int c = 0; c < kNumberOfCategories; ++c)
			if (pack.entries[c])
				what += std::string(what.empty() ? "" : ", ") + CategoryName(Category(c)) + " " + std::to_string(pack.entries[c]);
		logNote("Durandal art: %s %s follows %s (%s%s)", pack.name.c_str(), pack.version.c_str(),
				CategoryName(pack.category), what.c_str(), pack.normal_maps ? ", normal maps" : "");
		packs.push_back(pack);
	}
}

}

const std::vector<Pack>& Packs()
{
	scan();
	return packs;
}

const std::vector<Soundtrack>& Soundtracks()
{
	scan();
	return soundtracks;
}

Durandal::Feature FeatureFor(Category category)
{
	switch (category)
	{
		case kMonsters: return Durandal::kHDMonsters;
		case kWeapons: return Durandal::kHDWeapons;
		case kScenery: return Durandal::kHDScenery;
		case kModels: return Durandal::k3DPickups;
		default: return Durandal::kHDWalls;
	}
}

const char* CategoryName(Category category)
{
	switch (category)
	{
		case kMonsters: return "Monsters";
		case kWeapons: return "Weapons and Items";
		case kScenery: return "Scenery";
		case kModels: return "3D Pickups";
		default: return "Walls and Sky";
	}
}

std::string Installed(Category category)
{
	std::string result;
	for (const Pack& pack : Packs())
	{
		if (pack.category != category)
			continue;
		if (!result.empty())
			result += ", ";
		result += pack.name;
		if (!pack.version.empty())
			result += " " + pack.version;
	}
	return result;
}

void Rescan()
{
	packs.clear();
	soundtracks.clear();
	scanned = false;
	scan();
}

bool Apply()
{
	if (!Durandal::Available())
		return false;
	scan();
	bool changed = false;
	for (Plugin& plugin : *Plugins::instance())
	{
		const std::string path = plugin.directory.GetPath();
		for (const Pack& pack : packs)
		{
			if (pack.path != path)
				continue;
			const bool want = Durandal::Enabled(FeatureFor(pack.category));
			if (plugin.enabled != want)
			{
				plugin.enabled = want;
				changed = true;
				logNote("Durandal art: %s %s", pack.name.c_str(), want ? "on" : "off");
			}
		}
		// One soundtrack at a time: the chosen one, none in Stock
		const bool stock = Durandal::Prefs().quality_tier == Durandal::kTierStock;
		for (const Soundtrack& st : soundtracks)
		{
			if (st.path != path)
				continue;
			const bool want = !stock && st.name == Durandal::Prefs().soundtrack;
			if (plugin.enabled != want)
			{
				plugin.enabled = want;
				changed = true;
				logNote("Durandal art: soundtrack %s %s", st.name.c_str(), want ? "on" : "off");
			}
		}
	}
	if (changed)
		Plugins::instance()->invalidate();
	return changed;
}

}
