/*
	Durandal: texture cache (DurandalTextureCache.h).

	Entry file: a header, the key it was built from (checked on reading,
	since the file is named by the key's hash), then the BC7 blocks of
	every mip level. Written to a temporary name and renamed into place,
	so a crash mid-write leaves nothing the reader could take for an entry.
*/

#import <Foundation/Foundation.h>	// before the engine headers (their DEBUG macro upsets CarbonCore)

#include "DurandalTextureCache.h"

#include "DurandalBC7.h"
#include "DurandalMetal.h"
#include "DurandalPreferences.h"
#include "FileHandler.h"
#include "ImageLoader.h"
#include "Logging.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace DurandalTextureCache {

namespace {

const uint32_t kVersion = 3;	// 3: alpha exact through the p-bit (DurandalBC7 quantise)
enum { kOpaque = 1, kPremultiplied = 2 };

struct Header
{
	char magic[4];		// "DTXC"
	uint32_t version;
	uint32_t width, height, mips;
	uint32_t format;	// ImageDescriptor::BC7
	uint32_t flags;
	uint32_t key_length;
	uint32_t bytes;		// of block data, all levels
	float vscale, uscale;
};
// The key follows the header; the data starts at the next multiple of 16
size_t data_offset(uint32_t key_length)
{
	return (sizeof(Header) + key_length + 15) & ~size_t(15);
}

std::once_flag directory_once;
std::string directory;
std::atomic<int> hits(0), built(0), failures(0);
std::atomic<uint64_t> built_bytes(0);
std::atomic<uint32_t> serial(0);

uint64_t fnv1a(const std::string& s)
{
	uint64_t h = 1469598103934665603ull;
	for (unsigned char c : s)
	{
		h ^= c;
		h *= 1099511628211ull;
	}
	return h;
}

std::string file_for(const std::string& key)
{
	const std::string& dir = Directory();
	if (dir.empty())
		return std::string();
	char name[40];
	snprintf(name, sizeof(name), "/%016llx.dtx", (unsigned long long)fnv1a(key));
	return dir + name;
}

void describe(std::string& key, const char* what, const FileSpecifier& file)
{
	if (file == FileSpecifier())
		return;
	const char* path = file.GetPath();
	struct stat st;
	char buf[80];
	if (stat(path, &st) == 0)
		snprintf(buf, sizeof(buf), ":%lld:%lld.%09ld;", (long long)st.st_size,
				 (long long)st.st_mtimespec.tv_sec, (long)st.st_mtimespec.tv_nsec);
	else
		snprintf(buf, sizeof(buf), ":missing;");
	key += what;
	key += ':';
	key += path;
	key += buf;
}

size_t chain_bytes(int w, int h, int mips)
{
	size_t n = 0;
	for (int m = 0; m < mips; ++m)
		n += DurandalBC7::Bytes(std::max(1, w >> m), std::max(1, h >> m));
	return n;
}

}  // namespace

const std::string& Directory()
{
	std::call_once(directory_once, [] {
		@autoreleasepool {
			NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
			if (paths.count == 0)
				return;
			NSString* dir = [paths[0] stringByAppendingPathComponent:@"Durandal/Textures"];
			NSError* error = nil;
			if ([[NSFileManager defaultManager] createDirectoryAtPath:dir withIntermediateDirectories:YES attributes:nil error:&error])
				directory = dir.fileSystemRepresentation;
			else
				logWarning("Durandal texture cache: cannot create %s: %s", dir.UTF8String, error.localizedDescription.UTF8String);
		}
	});
	return directory;
}

bool Active()
{
	return DurandalMetal::DisplayActive() && Durandal::Enabled(Durandal::kTextureCache);
}

std::string Key(const FileSpecifier& colours, const FileSpecifier& mask, int flags,
				int actual_width, int actual_height, int max_size)
{
	std::string key = "bc7m6/1;";
	describe(key, "c", colours);
	describe(key, "m", mask);
	char buf[96];
	snprintf(buf, sizeof(buf), "f:%d;a:%dx%d;s:%d", flags, actual_width, actual_height, max_size);
	key += buf;
	return key;
}

// The entry is mapped, not read: its pages are the file's own, paged in
// when the texture is first uploaded and never part of the game's
// footprint, so a level's thousands of images cost the CPU side nothing.
// Mapped private and writable, so a stray write could never reach the file.
bool Fetch(const std::string& key, ImageDescriptor& image)
{
	const std::string path = file_for(key);
	if (path.empty())
		return false;
	const int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0)
		return false;
	struct stat st;
	if (fstat(fd, &st) != 0 || st.st_size < off_t(sizeof(Header)))
	{
		close(fd);
		failures++;
		return false;
	}
	const size_t length = size_t(st.st_size);
	void* base = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	close(fd);
	if (base == MAP_FAILED)
	{
		failures++;
		return false;
	}
	Header h;
	memcpy(&h, base, sizeof(h));
	bool ok = memcmp(h.magic, "DTXC", 4) == 0 && h.version == kVersion &&
		h.format == ImageDescriptor::BC7 && h.key_length == key.size() &&
		h.width > 0 && h.height > 0 && h.width <= 16384 && h.height <= 16384 && h.mips >= 1 && h.mips <= 15 &&
		h.bytes == chain_bytes(int(h.width), int(h.height), int(h.mips)) &&
		data_offset(h.key_length) + h.bytes <= length &&
		memcmp(static_cast<const char*>(base) + sizeof(Header), key.data(), key.size()) == 0;
	if (!ok)
	{
		munmap(base, length);
		failures++;
		return false;
	}
	uint32* pixels = reinterpret_cast<uint32*>(static_cast<char*>(base) + data_offset(h.key_length));
	image.AdoptCompressed(ImageDescriptor::BC7, int(h.width), int(h.height), int(h.mips), h.vscale, h.uscale, pixels, int(h.bytes));
	image.MappedBase = base;
	image.MappedLength = length;
	image.Opaque = (h.flags & kOpaque) != 0;
	image.PremultipliedAlpha = (h.flags & kPremultiplied) != 0;
	hits++;
	return true;
}

bool Store(const std::string& key, ImageDescriptor& image)
{
	if (!image.IsPresent() || image.GetFormat() != ImageDescriptor::RGBA8 || image.GetMipMapCount() > 1)
		return false;
	const std::string path = file_for(key);
	if (path.empty())
		return false;
	const int w = image.GetWidth(), h = image.GetHeight();
	int mips = 0;
	bool opaque = false;
	std::vector<uint8_t> data = DurandalBC7::EncodeMipChain(reinterpret_cast<const uint8_t*>(image.GetBuffer()), w, h, mips, opaque);

	Header hd;
	memcpy(hd.magic, "DTXC", 4);
	hd.version = kVersion;
	hd.width = uint32_t(w);
	hd.height = uint32_t(h);
	hd.mips = uint32_t(mips);
	hd.format = ImageDescriptor::BC7;
	hd.flags = (opaque ? kOpaque : 0) | (image.IsPremultiplied() ? kPremultiplied : 0);
	hd.key_length = uint32_t(key.size());
	hd.bytes = uint32_t(data.size());
	hd.vscale = float(image.GetVScale());
	hd.uscale = float(image.GetUScale());

	char suffix[48];
	snprintf(suffix, sizeof(suffix), ".%d.%u.tmp", int(getpid()), unsigned(serial++));
	const std::string temp = path + suffix;
	FILE* f = fopen(temp.c_str(), "wb");
	bool written = f != nullptr;
	if (f)
	{
		static const char padding[16] = {0};
		const size_t pad = data_offset(uint32_t(key.size())) - sizeof(Header) - key.size();
		written = fwrite(&hd, sizeof(hd), 1, f) == 1 && fwrite(key.data(), 1, key.size(), f) == key.size() &&
			fwrite(padding, 1, pad, f) == pad && fwrite(data.data(), 1, data.size(), f) == data.size();
		written = (fclose(f) == 0) && written;
	}
	if (!written || rename(temp.c_str(), path.c_str()) != 0)
	{
		unlink(temp.c_str());
		failures++;
	}
	else
	{
		built++;
		built_bytes += data.size();
		// The mapped entry replaces the decoded image
		const int was_hits = hits;
		if (Fetch(key, image))
		{
			hits = was_hits;
			image.Opaque = opaque;
			return true;
		}
	}

	// No entry: the compressed image still replaces the decoded one, so
	// the level runs on a quarter of the memory
	uint32* pixels = new uint32[(data.size() + 3) / 4];
	memcpy(pixels, data.data(), data.size());
	image.AdoptCompressed(ImageDescriptor::BC7, w, h, mips, image.GetVScale(), image.GetUScale(), pixels, int(data.size()));
	image.Opaque = opaque;
	return true;
}

void Report(const char* what)
{
	const int h = hits.exchange(0), b = built.exchange(0), f = failures.exchange(0);
	const uint64_t bytes = built_bytes.exchange(0);
	if (h || b || f)
	{
		logNote("Durandal texture cache (%s): %d from the cache, %d built (%.1f MB)%s", what, h, b,
				double(bytes) / 1048576.0, f ? ", with failures" : "");
		if (getenv("DURANDAL_CACHE_LOG"))
			fprintf(stderr, "Durandal texture cache (%s): %d from the cache, %d built (%.1f MB)%s\n", what, h, b,
					double(bytes) / 1048576.0, f ? ", with failures" : "");
	}
}

namespace {
struct TallyEntry { const char* what; double ms; int count; };
std::vector<TallyEntry> tallies;
std::mutex tally_mutex;
}

void Tally(const char* what, double ms)
{
	static const bool on = getenv("DURANDAL_CACHE_LOG") != nullptr;
	if (!on)
		return;
	std::lock_guard<std::mutex> lock(tally_mutex);
	for (TallyEntry& t : tallies)
		if (t.what == what)
		{
			t.ms += ms;
			t.count++;
			return;
		}
	tallies.push_back({what, ms, 1});
}

void Mark(const char* what)
{
	static const bool on = getenv("DURANDAL_CACHE_LOG") != nullptr;
	if (!on)
		return;
	static const auto launch = std::chrono::steady_clock::now();
	fprintf(stderr, "Durandal: %8.0f ms  %s\n",
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - launch).count(), what);
	std::lock_guard<std::mutex> lock(tally_mutex);
	for (const TallyEntry& t : tallies)
		fprintf(stderr, "Durandal:              %s: %.0f ms in %d calls\n", t.what, t.ms, t.count);
	tallies.clear();
}

void Stats(size_t& files, uint64_t& bytes)
{
	files = 0;
	bytes = 0;
	const std::string& dir = Directory();
	if (dir.empty())
		return;
	@autoreleasepool {
		NSString* path = [NSString stringWithUTF8String:dir.c_str()];
		NSArray<NSString*>* names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
		for (NSString* name in names)
		{
			if (![name.pathExtension isEqualToString:@"dtx"])
				continue;
			NSDictionary* attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:[path stringByAppendingPathComponent:name] error:nil];
			if (!attrs)
				continue;
			files++;
			bytes += [attrs fileSize];
		}
	}
}

}
