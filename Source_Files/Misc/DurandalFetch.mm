/*
	DurandalFetch.mm — Durandal project

	See DurandalFetch.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

// macOS's headers first: the engine's cseries.h defines macros (DEBUG)
// that CoreServices' headers trip over
#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>

#include "DurandalFetch.h"
#ifndef DURANDAL_FETCH_HARNESS
#include "DurandalScenario.h"
#endif
#ifndef DURANDAL_FETCH_HARNESS
#include "cseries.h"
#include "FileHandler.h"
#include "Logging.h"
extern DirectorySpecifier local_data_dir;
#endif

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>

// A command-line harness (no engine) tests the fetch on its own
#ifdef DURANDAL_FETCH_HARNESS
#define logNote(...) NSLog(@__VA_ARGS__)
#endif

// One task's delegate: a redirect refused and recorded (finding a pack's
// download), or a download's progress and file
@interface DurandalFetchDelegate : NSObject <NSURLSessionDownloadDelegate>
@property (atomic, strong) NSURL* redirect;
@property (atomic, strong) NSString* destination;
@property (atomic, strong) NSError* error;
@property (atomic) BOOL moved;
@property (atomic) int64_t received;
@property (atomic) int64_t expected;
@property (atomic) NSInteger httpStatus;
@property (nonatomic) dispatch_semaphore_t done;
@end

@implementation DurandalFetchDelegate
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task
	willPerformHTTPRedirection:(NSHTTPURLResponse*)response newRequest:(NSURLRequest*)request
	completionHandler:(void (^)(NSURLRequest*))completionHandler
{
	if (self.destination) {
		completionHandler(request);	// a download follows its redirects
		return;
	}
	// Finding a download: the first redirect is the answer, as `curl -sI`
	self.redirect = request.URL;
	completionHandler(nil);
}
- (void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)task
	didWriteData:(int64_t)written totalBytesWritten:(int64_t)total totalBytesExpectedToWrite:(int64_t)expected
{
	self.received = total;
	self.expected = expected;
}
- (void)URLSession:(NSURLSession*)session downloadTask:(NSURLSessionDownloadTask*)task didFinishDownloadingToURL:(NSURL*)location
{
	// The file is gone once this returns: move it now
	if ([task.response isKindOfClass:[NSHTTPURLResponse class]])
		self.httpStatus = ((NSHTTPURLResponse*)task.response).statusCode;
	NSError* error = nil;
	[[NSFileManager defaultManager] removeItemAtPath:self.destination error:nil];
	self.moved = [[NSFileManager defaultManager] moveItemAtURL:location toURL:[NSURL fileURLWithPath:self.destination] error:&error];
}
- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error
{
	if ([task.response isKindOfClass:[NSHTTPURLResponse class]] && !self.httpStatus)
		self.httpStatus = ((NSHTTPURLResponse*)task.response).statusCode;
	if (error && !(self.redirect && error.code == NSURLErrorCancelled))
		self.error = error;
	dispatch_semaphore_signal(self.done);
}
@end

namespace DurandalFetch {

namespace {

// Each game's packs (keep in step with scripts/get-hd-art.sh). CFP Monsters,
// CFP Scenery and 3D Items serve Marathon 2 and Infinity alike; the walls
// and weapons packs are made for one game each.
const std::vector<PackInfo> kPacksM2 = {
	{ "walls", "CFP - Walls M2", "community-freeverse-plugin-walls-m2", 373,
	  "3848f883e92d5fd27c616f5f85df5dbe6efdbbfac4be81d296062cf2d69094b3" },
	{ "monsters", "CFP Monsters", "community-freeverse-plugin-monsters", 516,
	  "94bf18219cb9132428da60f85cbfd64b56ad72bdde3dcad04741ee1cc12d4ece" },
	{ "scenery", "CFP Scenery", "community-freeverse-plugin-scenery", 24,
	  "aea1b1106c94b3d1462900d99fcecac07cc93593197a13766d5f65e8910ff770" },
	{ "weapons", "CFP Weapons M2", "community-freeverse-plugin-weapons-m2", 66,
	  "3354fc30f21892aef33c9f0f5a6fa8fd8b718e1b46553c5b59824a654633a7a0" },
	{ "3d", "3D Items", "3d-items-plugin", 3,
	  "b4673ac3d6b43f4beb4bb629772f50e64e02d3ad97859a77e5bc9386684b3f99" },
};
const std::vector<PackInfo> kPacksInfinity = {
	{ "walls", "CFP - Walls MInf", "communityfreeverse-walls-minf", 425,
	  "48c052ee390cfc8a5c38bc9affaf119a372c2256539c76df3f074de1c40688fa" },
	{ "monsters", "CFP Monsters", "community-freeverse-plugin-monsters", 516,
	  "94bf18219cb9132428da60f85cbfd64b56ad72bdde3dcad04741ee1cc12d4ece" },
	{ "scenery", "CFP Scenery", "community-freeverse-plugin-scenery", 24,
	  "aea1b1106c94b3d1462900d99fcecac07cc93593197a13766d5f65e8910ff770" },
	{ "weapons", "CFP Weapons MInf", "community-freeverse-plugin-weapons", 73,
	  "0f42088c8f9010f7a8fadc5a43cc7a692618f08e30d79c09ad903f63fd71b12f" },
	{ "3d", "3D Items", "3d-items-plugin", 3,
	  "b4673ac3d6b43f4beb4bb629772f50e64e02d3ad97859a77e5bc9386684b3f99" },
};
// Marathon: Tim Vogel's TTEP walls at 1024, Hopper's starfield (after the
// walls, which it overrides; Aleph One's own release), Rock's Texture
// Renewal monsters (a 7z archive; the owner chose them over xBR after a
// side-by-side, 3 Oct 2026), General Tacticus's weapons and 3D scenery
const std::vector<PackInfo> kPacksMarathon = {
	{ "walls", "TTEP 1024", "ttep-updated-plugin-m1-1024x1024", 60,
	  "31364eb067965fe66b2a9a0363b318c44c930986d3eeabc0838665e35426eee0" },
	{ "sky", "Updated Starscape", "https://github.com/Aleph-One-Marathon/data-marathon/releases/download/plugin-removal/Updated.Starscape.zip", 1,
	  "61ae76698b5b3afbfaa22b85630169c3250be07647a58ed37b829dfe7a116a0e" },
	{ "monsters", "Texture Renewal Monsters", "marathon-texture-renewal-project-monsters-module", 44,
	  "91d3c291044a95b7788155138a1d7b5d39518e35be43a3f28a743b9f90f1663f" },
	{ "weapons", "M1 Weapons Redux", "tacticus-m1-weapons-redux-2", 14,
	  "7cb9af92cbae90b38ac1d347011a7a584367cacadcec47a8a1f5eab879b2dcee" },
	{ "scenery", "3D Scenery M1", "3d-scenery-for-m1", 4,
	  "3fc490d5cd73be06fd0169adedbcc00bfd0255c0efe4c4c983425ae7eae38b33" },
};

// The game's list (the scenario is known by the time anything asks)
const std::vector<PackInfo>& game_packs()
{
#ifdef DURANDAL_FETCH_HARNESS
	// DURANDAL_FETCH_GAME=inf|m1 picks another game's list
	const char* g = getenv("DURANDAL_FETCH_GAME");
	return !g ? kPacksM2 : strcmp(g, "inf") == 0 ? kPacksInfinity : strcmp(g, "m1") == 0 ? kPacksMarathon : kPacksM2;
#else
	switch (DurandalScenario::Current())
	{
		case DurandalScenario::kMarathon2: return kPacksM2;
		case DurandalScenario::kInfinity: return kPacksInfinity;
		default: return kPacksMarathon;
	}
#endif
}
#define kPacks game_packs()

std::mutex lock;
Status status;
std::atomic<bool> cancelled{ false };
NSURLSessionTask* current = nil;	// under lock

NSString* ns(const std::string& s)
{
	return [NSString stringWithUTF8String:s.c_str()];
}

bool installed(const PackInfo& pack)
{
	NSString* path = [ns(PluginsFolder()) stringByAppendingPathComponent:@(pack.folder)];
	return [[NSFileManager defaultManager] fileExistsAtPath:path];
}

void set(size_t i, State state, const std::string& note = std::string())
{
	std::lock_guard<std::mutex> g(lock);
	status.packs[i].state = state;
	if (!note.empty())
		status.packs[i].note = note;
}

// Runs a task to completion on this (background) thread
void run(NSURLSession* session, NSURLSessionTask* task, DurandalFetchDelegate* delegate, size_t i)
{
	{
		std::lock_guard<std::mutex> g(lock);
		current = task;
	}
	[task resume];
	// Wait, passing the progress on
	while (dispatch_semaphore_wait(delegate.done, dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC)) != 0) {
		std::lock_guard<std::mutex> g(lock);
		status.packs[i].received = delegate.received;
		status.packs[i].expected = delegate.expected;
	}
	std::lock_guard<std::mutex> g(lock);
	status.packs[i].received = delegate.received;
	status.packs[i].expected = delegate.expected;
	current = nil;
}

// The pack's download address: the downloads/new redirect, a Google Drive
// page turned into the file behind it
NSURL* find(const PackInfo& pack, size_t i, std::string& why)
{
	if (strncmp(pack.item, "https://", 8) == 0)
		return [NSURL URLWithString:@(pack.item)];
	DurandalFetchDelegate* delegate = [DurandalFetchDelegate new];
	delegate.done = dispatch_semaphore_create(0);
	NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
	config.timeoutIntervalForRequest = 30;
	NSURLSession* session = [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:nil];
	NSString* page = [NSString stringWithFormat:@"https://simplici7y.com/items/%s/downloads/new", pack.item];
	NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:[NSURL URLWithString:page]];
	request.HTTPMethod = @"HEAD";
	run(session, [session dataTaskWithRequest:request], delegate, i);
	[session finishTasksAndInvalidate];
	NSURL* link = delegate.redirect;
	if (!link) {
		why = delegate.error ? "no answer from Simplici7y" : "Simplici7y gave no download link";
		return nil;
	}
	NSString* text = link.absoluteString;
	if ([text containsString:@"drive.google.com"]) {
		NSString* id = nil;
		NSRange r = [text rangeOfString:@"/file/d/"];
		if (r.location != NSNotFound) {
			NSString* rest = [text substringFromIndex:r.location + r.length];
			id = [[rest componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"/?"]] firstObject];
		} else {
			NSURLComponents* parts = [NSURLComponents componentsWithURL:link resolvingAgainstBaseURL:NO];
			for (NSURLQueryItem* item in parts.queryItems)
				if ([item.name isEqualToString:@"id"])
					id = item.value;
		}
		if (id.length)
			link = [NSURL URLWithString:[NSString stringWithFormat:
				@"https://drive.usercontent.google.com/download?id=%@&export=download&confirm=t", id]];
	}
	return link;
}

bool download(NSURL* link, NSString* zip, size_t i, std::string& why)
{
	DurandalFetchDelegate* delegate = [DurandalFetchDelegate new];
	delegate.done = dispatch_semaphore_create(0);
	delegate.destination = zip;
	NSURLSessionConfiguration* config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
	config.timeoutIntervalForRequest = 60;
	config.timeoutIntervalForResource = 3600;
	NSURLSession* session = [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:nil];
	run(session, [session downloadTaskWithURL:link], delegate, i);
	[session finishTasksAndInvalidate];
	if (cancelled)
		return false;
	if (delegate.error || !delegate.moved) {
		why = delegate.error ? std::string("the download failed: ") + delegate.error.localizedDescription.UTF8String
							 : "the download could not be kept";
		return false;
	}
	if (delegate.httpStatus >= 400) {
		why = "the host refused the download (HTTP " + std::to_string(int(delegate.httpStatus)) + ")";
		return false;
	}
	return true;
}

int tool(NSString* path, NSArray<NSString*>* arguments)
{
	NSTask* task = [NSTask new];
	task.launchPath = path;
	task.arguments = arguments;
	task.standardOutput = [NSFileHandle fileHandleWithNullDevice];
	task.standardError = [NSFileHandle fileHandleWithNullDevice];
	@try {
		[task launch];
		[task waitUntilExit];
	} @catch (NSException*) {
		return -1;
	}
	return task.terminationStatus;
}

std::string sha256(NSString* path)
{
	FILE* f = fopen(path.fileSystemRepresentation, "rb");
	if (!f)
		return std::string();
	CC_SHA256_CTX ctx;
	CC_SHA256_Init(&ctx);
	std::vector<unsigned char> buffer(1 << 20);
	size_t n;
	while ((n = fread(buffer.data(), 1, buffer.size(), f)) > 0)
		CC_SHA256_Update(&ctx, buffer.data(), CC_LONG(n));
	fclose(f);
	unsigned char digest[CC_SHA256_DIGEST_LENGTH];
	CC_SHA256_Final(digest, &ctx);
	char hex[2 * CC_SHA256_DIGEST_LENGTH + 1];
	for (int k = 0; k < CC_SHA256_DIGEST_LENGTH; ++k)
		snprintf(hex + 2 * k, 3, "%02x", digest[k]);
	return hex;
}

// The plugin inside an unpacked archive: the folder holding Plugin.xml
// nearest the top (not in a __MACOSX copy)
NSString* plugin_folder(NSString* root)
{
	NSString* best = nil;
	for (NSString* entry in [[NSFileManager defaultManager] enumeratorAtPath:root]) {
		if (![entry.lastPathComponent isEqualToString:@"Plugin.xml"] || [entry containsString:@"__MACOSX"])
			continue;
		if (!best || entry.pathComponents.count < best.pathComponents.count)
			best = entry;
	}
	return best ? [[root stringByAppendingPathComponent:best] stringByDeletingLastPathComponent] : nil;
}

void fetch()
{
	@autoreleasepool {
		NSFileManager* files = [NSFileManager defaultManager];
		NSString* plugins = ns(PluginsFolder());
		[files createDirectoryAtPath:plugins withIntermediateDirectories:YES attributes:nil error:nil];
		NSString* work = [NSTemporaryDirectory() stringByAppendingPathComponent:
			[NSString stringWithFormat:@"durandal-hd-art-%d", getpid()]];
		[files createDirectoryAtPath:work withIntermediateDirectories:YES attributes:nil error:nil];
		for (size_t i = 0; i < kPacks.size(); ++i) {
			const PackInfo& pack = kPacks[i];
			if (cancelled) {
				set(i, State::kCancelled);
				continue;
			}
			if (installed(pack)) {
				set(i, State::kInstalledAlready);
				continue;
			}
			std::string why;
			set(i, State::kFinding);
			NSURL* link = find(pack, i, why);
			if (!link) {
				set(i, cancelled ? State::kCancelled : State::kFailed, why);
				continue;
			}
			set(i, State::kDownloading);
			NSString* zip = [work stringByAppendingPathComponent:[NSString stringWithFormat:@"%s.zip", pack.key]];
			if (!download(link, zip, i, why)) {
				set(i, cancelled ? State::kCancelled : State::kFailed, why);
				[files removeItemAtPath:zip error:nil];
				continue;
			}
			set(i, State::kChecking);
			// A zip, or a 7z archive (Rock's packs), which macOS's own tar reads
			bool seven = false;
			if (FILE* f = fopen(zip.fileSystemRepresentation, "rb")) {
				unsigned char head[6] = {};
				seven = fread(head, 1, 6, f) == 6 && memcmp(head, "7z\xBC\xAF\x27\x1C", 6) == 0;
				fclose(f);
			}
			if (seven ? tool(@"/usr/bin/tar", @[ @"-tf", zip ]) != 0 : tool(@"/usr/bin/unzip", @[ @"-tq", zip ]) != 0) {
				set(i, State::kFailed, "what arrived is not an archive (the host may want a browser)");
				[files removeItemAtPath:zip error:nil];
				continue;
			}
			if (*pack.sha256 && sha256(zip) != pack.sha256) {
				std::lock_guard<std::mutex> g(lock);
				status.packs[i].updated = true;	// its authors have updated it: installed all the same
			}
			set(i, State::kUnpacking);
			NSString* out = [work stringByAppendingPathComponent:@(pack.key)];
			[files removeItemAtPath:out error:nil];
			[files createDirectoryAtPath:out withIntermediateDirectories:YES attributes:nil error:nil];
			const bool unpacked = seven ? tool(@"/usr/bin/tar", @[ @"-xf", zip, @"-C", out ]) == 0
										: tool(@"/usr/bin/ditto", @[ @"-x", @"-k", zip, out ]) == 0;
			// Some archives carry read-only folders, which cannot be moved
			tool(@"/bin/chmod", @[ @"-R", @"u+w", out ]);
			[files removeItemAtPath:zip error:nil];
			if (!unpacked) {
				set(i, State::kFailed, "could not unpack it");
				continue;
			}
			[files removeItemAtPath:[out stringByAppendingPathComponent:@"__MACOSX"] error:nil];
			NSString* folder = plugin_folder(out);
			if (!folder) {
				set(i, State::kFailed, "no Plugin.xml inside");
				[files removeItemAtPath:out error:nil];
				continue;
			}
			NSString* destination = [plugins stringByAppendingPathComponent:@(pack.folder)];
			if (cancelled || installed(pack) || ![files moveItemAtPath:folder toPath:destination error:nil]) {
				set(i, cancelled ? State::kCancelled : State::kFailed, cancelled ? "" : "could not move it into the Plugins folder");
				[files removeItemAtPath:out error:nil];
				continue;
			}
			[files removeItemAtPath:out error:nil];
			{
				std::lock_guard<std::mutex> g(lock);
				status.packs[i].state = State::kInstalled;
				status.new_folders.push_back(destination.UTF8String);
			}
			logNote("Durandal HD art: installed %s", pack.folder);
		}
		[files removeItemAtPath:work error:nil];
		std::lock_guard<std::mutex> g(lock);
		status.running = false;
		status.finished = true;
	}
}

}

const std::vector<PackInfo>& Packs()
{
	return kPacks;
}

std::string PluginsFolder()
{
	if (const char* dir = getenv("DURANDAL_PLUGINS_DIR"))
		return dir;
#ifdef DURANDAL_FETCH_HARNESS
	return std::string(getenv("HOME")) + "/Library/Application Support/Durandal/Plugins";
#else
	return (local_data_dir + "Plugins").GetPath();
#endif
}

bool IsInstalled(const PackInfo& pack)
{
	return installed(pack);
}

int Missing(int* megabytes)
{
	int count = 0, mb = 0;
	for (const PackInfo& pack : kPacks)
		if (!installed(pack)) {
			++count;
			mb += pack.megabytes;
		}
	if (megabytes)
		*megabytes = mb;
	return count;
}

int64_t FreeMegabytes()
{
	NSString* path = ns(PluginsFolder());
	while (path.length > 1 && ![[NSFileManager defaultManager] fileExistsAtPath:path])
		path = path.stringByDeletingLastPathComponent;
	NSDictionary* attributes = [[NSFileManager defaultManager] attributesOfFileSystemForPath:path error:nil];
	NSNumber* free = attributes[NSFileSystemFreeSize];
	return free ? free.longLongValue / (1024 * 1024) : -1;
}

void Start()
{
	{
		std::lock_guard<std::mutex> g(lock);
		if (status.running)
			return;
		status = Status();
		status.running = true;
		status.packs.resize(kPacks.size());
	}
	cancelled = false;
	std::thread(fetch).detach();
}

void Cancel()
{
	cancelled = true;
	std::lock_guard<std::mutex> g(lock);
	[current cancel];
}

Status Snapshot()
{
	std::lock_guard<std::mutex> g(lock);
	return status;
}

std::string Describe(const PackInfo& pack, const PackStatus& s)
{
	char line[160];
	switch (s.state) {
		case State::kWaiting:
			snprintf(line, sizeof(line), "%s: waiting (about %d MB)", pack.folder, pack.megabytes);
			break;
		case State::kInstalledAlready:
			snprintf(line, sizeof(line), "%s: already installed, left alone", pack.folder);
			break;
		case State::kFinding:
			snprintf(line, sizeof(line), "%s: finding the authors' download", pack.folder);
			break;
		case State::kDownloading:
			if (s.expected > 0)
				snprintf(line, sizeof(line), "%s: downloading, %lld of %lld MB", pack.folder,
						 (long long)(s.received >> 20), (long long)(s.expected >> 20));
			else
				snprintf(line, sizeof(line), "%s: downloading, %lld MB", pack.folder, (long long)(s.received >> 20));
			break;
		case State::kChecking:
			snprintf(line, sizeof(line), "%s: checking the archive", pack.folder);
			break;
		case State::kUnpacking:
			snprintf(line, sizeof(line), "%s: unpacking", pack.folder);
			break;
		case State::kInstalled:
			snprintf(line, sizeof(line), "%s: installed%s", pack.folder, s.updated ? " (a newer version than tested)" : "");
			break;
		case State::kFailed:
			snprintf(line, sizeof(line), "%s: not installed: %s", pack.folder, s.note.c_str());
			break;
		case State::kCancelled:
			snprintf(line, sizeof(line), "%s: cancelled", pack.folder);
			break;
	}
	return line;
}

}
