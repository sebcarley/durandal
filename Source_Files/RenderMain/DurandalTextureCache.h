#ifndef DURANDAL_TEXTURE_CACHE_H
#define DURANDAL_TEXTURE_CACHE_H
/*
	Durandal: texture cache.

	HD art packs are PNGs: hundreds per level, decoded at every level
	start and kept as RGBA for the level on the CPU and the GPU. With the
	Texture Cache switch on (ART tab; Metal display), a replacement image
	is block-compressed (BC7 with its mip chain, DurandalBC7.h) the first
	time it loads and written to ~/Library/Caches/Durandal/Textures; later
	loads read that file instead of decoding the PNG, and the image stays
	compressed on the CPU and the GPU, a quarter of the bytes. Entries are
	keyed by the source files' paths, sizes and modification times and the
	loading parameters, so an updated pack rebuilds its own. The folder
	can be deleted at any time; anything unrecognised in it is ignored.
	Every function may be called from the loader's worker threads.
*/

#include <cstddef>
#include <cstdint>
#include <string>

class FileSpecifier;
class ImageDescriptor;

namespace DurandalTextureCache {

// Metal display with the switch on
bool Active();

// The key for an image loaded from colours (with mask, or none) with the
// ImageLoader flags, the MML size hints and the size limit
std::string Key(const FileSpecifier& colours, const FileSpecifier& mask, int flags,
				int actual_width, int actual_height, int max_size);

// Fills image from the cache: true on a hit
bool Fetch(const std::string& key, ImageDescriptor& image);

// Compresses a decoded (RGBA8, single level) image, writes the entry and
// replaces the image's contents with the compressed version
bool Store(const std::string& key, ImageDescriptor& image);

// One log line for the loads since the last report, if there were any
void Report(const char* what);

// Level-load phases: with DURANDAL_CACHE_LOG=1 prints the time since launch
void Mark(const char* what);
// Accumulates time under a name (a string literal); Mark() prints and clears the tallies
void Tally(const char* what, double ms);

// What is on disk
void Stats(size_t& files, uint64_t& bytes);
const std::string& Directory();

}

#endif
