#ifndef DURANDAL_FETCH_H
#define DURANDAL_FETCH_H
/*
	DurandalFetch.h — Durandal project

	Get HD Art: fetches the community HD art Durandal is played with and
	installs it as plugins, from inside the game (the ART tab's button), as
	scripts/get-hd-art.sh does from a terminal. Nothing here is ours to
	distribute: every pack comes from its authors' own Simplici7y page, by
	the download link they publish (Google Drive files through their direct
	download address), and is theirs. Credits and versions:
	docs/HD_ASSETS.md.

	For each pack, in turn, on a background thread: skip it if its folder is
	already in the Plugins folder (nothing is ever overwritten); find the
	download through the page's downloads/new redirect; download it with
	progress; check it is a zip archive; note when it differs from the
	version tested (its SHA-256), and install it all the same; unpack it
	with macOS's own ditto; move the folder holding Plugin.xml into the
	Plugins folder under the pack's name. Uses only what macOS provides
	(NSURLSession, CommonCrypto, /usr/bin/ditto and /usr/bin/unzip).

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include <cstdint>
#include <string>
#include <vector>

namespace DurandalFetch {

// The packs: the four Community/Freeverse packs and the 3D Items plugin
struct PackInfo {
	const char* key;		// walls, monsters, scenery, weapons, 3d
	const char* folder;		// its folder in the Plugins folder
	const char* item;		// its Simplici7y item
	int megabytes;			// the download, roughly
	const char* sha256;		// the version tested
};
const std::vector<PackInfo>& Packs();

enum class State { kWaiting, kInstalledAlready, kFinding, kDownloading, kChecking, kUnpacking, kInstalled, kFailed, kCancelled };

struct PackStatus {
	State state = State::kWaiting;
	int64_t received = 0, expected = 0;	// bytes, while downloading
	bool updated = false;				// differs from the version tested
	std::string note;					// why it failed, for the dialog
};

struct Status {
	bool running = false;
	bool finished = false;
	std::vector<PackStatus> packs;		// as Packs()
	std::vector<std::string> new_folders;	// installed this time (full paths)
};

// The Plugins folder packs are installed into: the local data folder's
// Plugins (DURANDAL_PLUGINS_DIR overrides it, for tests)
std::string PluginsFolder();

// Whether a pack's folder is in the Plugins folder already
bool IsInstalled(const PackInfo& pack);

// Packs not yet in the Plugins folder, and their download in megabytes
int Missing(int* megabytes = nullptr);

// Free space where the packs would go, in megabytes
int64_t FreeMegabytes();

// Starts fetching every pack that is missing (no-op while running)
void Start();
// Asks the running fetch to stop (the current download is cancelled;
// packs already installed stay)
void Cancel();
Status Snapshot();

// One line for the dialog: the pack's name and what is happening to it
std::string Describe(const PackInfo& pack, const PackStatus& status);

}

#endif
