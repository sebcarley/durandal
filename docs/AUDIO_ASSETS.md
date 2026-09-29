# Music and sound for Marathon 2: what exists, and how Aleph One's audio works

Research for Durandal, 27 September 2026. Nothing here is engine code; it is
the reference for the round that gives the native engine gapless music,
spatial audio, and geometry-driven reverb and occlusion, while loading the
community's music and sound plugins unchanged. It follows
`docs/HD_ASSETS.md` and the same rules: no third-party audio in git
(`Assets/` is ignored), packs stay user-installed.

Method: every source in the brief was fetched and checked (Simplici7y tag
and item pages by HTTP, Aaron Freed's site, Bandcamp, GitHub); every pack
Simplici7y hosts directly was downloaded into `Assets/audio/`, unpacked and
inspected (Plugin.xml, MML, Lua, file formats with `file` and `afinfo`, the
sounds-file headers with a 12-byte parser). Checksums are in
`Assets/audio/SHA256SUMS.txt`. Engine claims cite this checkout (branch
`durandal/hd-assets`); a set of the citations was re-read line by line.
Where a site could not be fetched (ModDB behind Cloudflare, Steam Workshop
rate-limited, OneDrive and Dropbox folders that need a browser) the item is
in section 6 to fetch by hand. Four of them were fetched by hand on 27
September (Feel the Noise from the Workshop, both of Aaron Freed's, the
ModDB M2SE OST) and they are inspected below.

## Summary

- Marathon 2 ships no level music: the data has one file, `Music.ogg`, the
  93-second title loop, and no `Music/` folder or `<music>` MML (section
  3.3). Every soundtrack below is fan work that a plugin adds.
- Three complete Marathon 2 soundtracks exist and all three load through
  Aleph One 1.7 mechanisms: the M2SE tracks (2005-07, five composers) as
  treellama's map-patch plugin; Talashar's *Feel the Noise* (2024) built on
  the same plugin (22 Vorbis tracks at 48 kHz); and Solar-Tron's *Solar
  Soundtrack* (2024-25), the "fully dynamic" one, which is a solo-Lua
  plugin driving the Lua Music API with 185 Opus segments. Aaron Freed's
  own Marathon 2 soundtrack is a work in progress (his fifth prototype: 28
  FLAC tracks, 1.4 GB, net levels unassigned); his "upmastered FLAC
  plugin" is a remaster of the M2SE tracks. The brief's item 3 conflated
  these: the 2024-09-16 release is Talashar's. All four map-patch plugins
  share one Plugin.xml shape (the two M2 map checksums, TEXT 128).
- Sound-effect packs for Marathon 2 do exist, contrary to the initial
  search: The Man's *Remastered Sounds for Marathon Infinity* (a whole
  sounds file that also works with M2 on Aleph One), djkontraktor's *M2I
  HD Sounds Mod* (the one MML sound-replacement plugin: 291 AIFFs at
  44.1 kHz), and two novelty sounds files. None is a "remaster from CD
  masters"; The Man's page promised one for 2021 that has not appeared.
- The engine decodes every external sound and music file with one
  library, libsndfile (built here with Vorbis, Opus, FLAC and MP3), and
  streams music on the SDL audio callback thread through four 8 KB OpenAL
  buffers. A single looping track loops sample-exactly; a playlist of
  several tracks leaves 25-70 ms of silence between them and never
  crossfades, although a crossfade engine exists unused in `MusicPlayer`
  (section 3.4). Sound positioning is either Marathon's 1995 stereo pan
  (the owner's setting) or OpenAL inverse-distance 3D with optional HRTF; both
  are fed by a listener updated once per game tick, obstruction is a
  top-down polygon walk, and Durandal's graded occlusion and room reverb
  already hook the points a spatial system would replace (section 3.5).

## 1. Resource table

Sizes are the download archive. "Loops" is what the pack does in the
engine today. Licence: none of the fan music is under an open licence;
Bandcamp releases are "all rights reserved", the M2SE tracks carry Team
Unpfhorgiven's and Iain McLaughlin's copyright per the readme. Loading a
user's own copy is fine; shipping any of it is not.

### Marathon 2 music

| # | Name | Author | Version, date | Target | Type | Format | Install | Loops | Licence / credit | Inspected |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | M2SE Music Plugin | treellama; tracks by Mark Sumner (Zipper Cat), Mike Gorczynski (The Punisher), Iain McLaughlin (Cannibal Whore Feast), Julian Zielke (Mercenary), MuShoo | 1.0.1, 12 Oct 2023 | M2 (map checksums 762432692 Mac, 1577821584 Win95) | music | 27 Ogg Vorbis 44.1 kHz stereo ~128 kbps + 1 MP3 (the "Splash" intro), 72 MB | plugin: `<map_patch>` injecting TEXT 128 with `<level index=n><music file=.../>` for all 41 levels | one track per level, so it loops | readme: OGGs copyright Team Unpfhorgiven / by Iain McLaughlin; "do not use or distribute without authors' permission" | yes |
| 2 | M2 Fan Soundtrack: Feel The Noise (plugin name "Feel The Noise - M2 OST") | Talashar (Austin Anderson, "Throkgaar") | 1.0, 16 Sep 2024 | M2 (checksums as #1) | music | 22 Ogg Vorbis, **48 kHz** stereo ~397 kbps, 24 s to 16.8 min each, 394 MB unpacked (407 MB 7z from the Workshop) | plugin: `<map_patch>` with TEXT 128, "based on M2SE by Treellama": 41 `<level>` entries, one `<music>` each (four tracks reused on two levels, net levels covered); Simplici7y's download is a OneDrive file, Steam Workshop id 3332007811 | one track per level, so it loops in the engine; the in-game cuts differ from the Bandcamp album (22 tracks, 164 min, FLAC/MP3, 16-bit/44.1 kHz) | readme: "all music composed by Talashar", themes after Seropian, O'Donnell, Salvatori, Sumner; Bandcamp all rights reserved, name-your-price | yes |
| 3 | The Solar Soundtrack for M2 | Solar-Tron (Justin B, "Flippant Sol") | 1.2.1, 14 Jan 2025 (1.0 21 Dec 2024) | M2 | music, dynamic | 185 Ogg Opus segments, 48 kHz stereo 128 kbps, 152 MB unpacked; FLAC on Bandcamp (69 tracks, 24-bit) | plugin: `Plugin.xml` with legacy `solo_lua="Dynamic Music.lua"` (3,265 lines), `minimum_version="20231125"` (1.7); disables achievements (world access) | segments are started, faded and swapped by Lua triggers; readme says tracks may be longer than the scenes | Bandcamp: all rights reserved; readme credits Roland/Apple QuickTime instruments and Bruce Cockburn's "If I Had a Rocket Launcher" | yes |
| 4 | M2SE Music Remastered (Aaron Freed's upmaster of #1) | Aaron Freed (The Man); composers as #1 | 1.0, undated | M2 (checksums as #1) | music | 27 FLAC 44.1 kHz 16-bit + the "Splash" MP3, 475 MB unpacked (498 MB zip from OneDrive) | plugin: `<map_patch>` with TEXT 128, the same 41-level list as #1 (a `128_alt.txt` swaps Hopper and Dreamscape on two levels); readme is the original 2000s M2-SE readme (Tophet, orbitalarm@bungie.org) | one track per level, loops | as #1; readme adds "all original Marathon music by Alexander Seropian, copyright Bungie" | yes |
| 5 | Aaron Freed - To maratho (Τὼ μᾰρᾰ́θω, "Two Marathons") | Aaron Freed | 1.0 in Plugin.xml; "2025-WIP #5" | M2 (checksums as #1) | music | 28 FLAC 44.1 kHz 16-bit in `Music/`, 1.4 GB unpacked (1.53 GB zip from Dropbox) | plugin: `<map_patch>` with TEXT 128: 41 `<level>` entries, `<music>` on the 28 solo levels only ("I currently have not specified music for any net levels"); info.txt asks for at least 120% music volume ("I mastered this quietly") and no other soundtrack plugin | one track per level, loops | "freely available, no part may be reproduced without my permission" except tracks he released CC BY-NC-SA for Endless Sky; two tracks co-written with Chris Christodoulou, used with permission | yes |
| 6 | Marathon 2 SE Tracks | fracai (uploader); composers as #1 | 1.5, 3 Aug 2011 | M2 | music (files only) | 27 Ogg + 1 MP3, 72 MB, the same files as #1 without a plugin | none: loose tracks (the 2011 way was a map with a music playlist) | n/a | as #1 | yes |
| 7 | Marathon 2 Special Edition soundtrack in MP3 Format | Marathoner325 | 2011 | M2 | music | MP3 | Megaupload link, dead since 2013 | n/a | "THIS IS NOT MY MUSIC" | link dead |
| 8 | Marathon 2 Map with Special Edition Soundtrack | Marathoner325 | 2011 | M2 | music + map | unknown | Megaupload link, dead | n/a | as above | link dead |
| 9 | Lh'owon Album (Marathon Groove Session I) | The Thug (organiser); ukimalefu, Ravenpulse, CryoS, $lave | 1.0, 13 Apr 2009 | M2-themed | music (listening) | 12 MP3 128 kbps, 31 MB | none: an album, no plugin | n/a | contest entries, no licence stated | yes |
| 10 | Volunteers Album (Groove Session II) | The Thug; Cryos, ukimalefu | 1.0, 17 Jun 2009 | M2-themed | music | 6 MP3 320 kbps, 26 MB | none | n/a | none stated | yes |
| 11 | Garrison Album (Groove Session III) | The Thug; ukimalefu, SynthNinja, WastedJamacan | 1.0, 9 Dec 2009 | M2-themed | music | 7 MP3 192 kbps, 18 MB | none | n/a | none stated | yes |
| 12 | Marathon 2: Special Edition Soundtrack (ModDB add-on) | AnchorCross (per ModDB); composers as #1 | for Aleph One 1.6.1, undated | M2 | music | 27 Ogg Vorbis 44.1 kHz ~122 kbps in `Music/01.ogg`-`27.ogg`, 92 MB unpacked (85 MB zip) | **not a plugin**: a modified map `Map-OST-1.6.1.sceA` (20,460,406 bytes; the stock map is 20,478,336) with TEXT 128 embedded, plus the Music folder, both dropped into the Marathon 2 directory and the map chosen in Environment > Map; a "Manual Update" folder carries the TEXT 128 to merge into a newer map with treellama's Atque. The pre-1.7 approach; a replaced map has a different checksum, so checksum-keyed plugins and net play would not match | one track per level, loops | as #1 (the same readme) | yes |
| 13 | ModDB derivative: "intended soundtrack of M2 by Psykosonik and a Death Grips soundtrack", built from #12 | unknown | unknown | M2 | music | unknown | unknown | unknown | commercial artists' music: unlicensed | no (section 6) |

### Marathon 2 sound effects

| # | Name | Author | Version, date | Target | Type | Format | Install | Licence / credit | Inspected |
|---|---|---|---|---|---|---|---|---|---|
| 14 | Remastered Sounds for Marathon Infinity | The Man (Aaron Freed) | 1.1, 10 Apr 2020 | Infinity; "also works with Marathon 2 on Aleph One" | SFX, whole sounds file | `Sounds remastered v1.1.sndA`: `snd2`, 2 sources, 215 sounds, 25 MB; all 16-bit, placed in the 8-bit slot too; about half as loud as the originals; PDF of changes | drop into the data folder, choose in Preferences > Environment > Sounds | Bungie's sounds remastered; none stated | yes |
| 15 | M2I HD Sounds Mod | djkontraktor | 0.1, 29 Jul 2020 | M2 and Infinity | SFX, MML replacement plugin | 291 AIFF, 44.1 kHz 16-bit stereo, 81 MB; "noise suppression, widened, equalised, reverb and echo at appropriate times" | plugin: `<sounds>` with 291 `<sound index slot file>` entries (213 indices, slots 0-4; 12 indices >= 203 are Infinity-only and ignored in M2); readme: drop the folder into Plugins | Bungie's sounds processed; none stated; author lists known faults (platform loop too loud, loop pops) | yes |
| 16 | Realistic Sounds | Jickets | 1, 10 Nov 2021 | Infinity ("all") | SFX, whole sounds file (novelty) | `RealisticSounds.sndA`: 2 sources, 216 sounds, 31 MB | as #14 | none stated | yes |
| 17 | Google Translated Bob and VacBob Lines | danrocno; voices Soron, Ssadke | 1.0, 4 Feb 2022 | Infinity, "does work in 2" | SFX, whole sounds file (novelty) | `.sndA`: 2 sources, 215 sounds, 24 MB; 16-bit slot only | as #14; needs the 16-bit preference | none stated | yes |
| 18 | Scuba Sounds | Quartz | 1.1, 25 Aug 2011 | any | SFX, MML plugin | 2 WAV (16-bit stereo, 44.1 and 22.05 kHz), 0.9 MB | plugin: `<sounds terminal_logon="141" terminal_logoff="141" breathing="142">` with `<sound index="94">` (underwater ambient) and `<sound index="142">` (bubbles): it steals a duplicate terminal-sound slot because MML cannot add sounds | samples from pond5.com | yes |
| 19 | Calle de Las Sombras | Zott | 1.01, 2011 | Infinity net map | map with optional echo-y sound variants | unknown | map + sounds; a 2011 review reports Aleph One crashing with the sounds | none stated | no |
| 20 | Alex Jones BOB Voice-over; Female BoB and VacBoB sounds (Marathon: Freedom) | Vice; CubicCircle | 2018; 2009 | Infinity | SFX novelty / voice sets | sounds files | as #14 | none stated | no |

### Marathon 1 and other packs (noted only)

Eupfhoria (Throkgaar, 2024-25, M1 soundtrack remake, plugin version July
2025), Marathon OST 2021 (ckt1138), Marathon Music Remasterd
(herecomethej2000, fluidsynth), Chibi Usa Soundtrack, See You Starside (The
Man, 2023, arrangement album), Marathon QT2 Tracks for M1A1 (Hopper, the
1994 QuickTime renders), M1A1-SE tracks (fracai), Marathon 1 Remastered
Sounds (The Man, 2020, a `.sndz` sounds file chosen in Environment >
Sounds), M1A1 HardCore Sounds, PID Remastered Sounds. Infinity: Strange
Aeons (Talashar, 2024). Scenario soundtracks: Divine Twilight OST plugin
(Talashar, Dec 2025), Rubicon X Fan Soundtrack (Aug 2026), Eternal X's
expanded soundtrack. None loads in Marathon 2 as is.

## 2. Recommended baseline for development

- **Music loading, the plain path:** the **M2SE Music Plugin (#1)**. It is
  the reference every other Marathon 2 soundtrack plugin copies (Talashar's
  and Aaron Freed's are built on it), the smallest (72 MB), and exercises
  exactly the mechanism the engine must keep: a `<map_patch>` matched by
  map checksum injecting TEXT 128, one `<music file>` per level, Ogg Vorbis
  at 44.1 kHz. One track per level also makes it the test for gapless
  looping of a single file (section 3.4). `Assets/audio/x_M2SE_Music_Plugin/M2SE
  Music` is ready to link into `~/Library/Application Support/Durandal/
  Plugins`.
- **Lua-driven dynamic music:** the **Solar Soundtrack for M2 (#3)**. It is
  the only dynamic soundtrack that exists and uses the whole Lua Music API:
  `Music.new` (190 calls), `Music.play`, `Music.fade`, `track:play()`,
  `track:fade()`, `track.active`, `track.volume`, `Music.stop`, in
  `Triggers.init/postidle/player_killed/monster_damaged/platform_switch/
  tag_switch/light_activated/projectile_switch/terminal_enter/terminal_exit`
  keyed on `Level.index`, `Level.calculate_completion_state` and
  `Game.ticks`. Opus at 48 kHz, so it also tests the Opus decoder and a
  sample rate different from the sounds file. Being a solo-Lua plugin with
  world access, it cannot run alongside another solo-Lua plugin and turns
  achievements off; it is not a net-game or film case.
- **Sound replacement, MML path:** the **M2I HD Sounds Mod (#15)**: the
  only `<sounds><sound index slot file>` pack for M2, with permutation
  slots and a sample rate (44.1 kHz stereo) unlike the sounds file's
  22 kHz mono.
- **Sound replacement, sounds-file path:** **Remastered Sounds for
  Marathon Infinity (#14)**: a second `.sndA` selected in Environment, all
  16-bit, to test the source selection and 16-bit slot rules of section
  3.1.
- **Format coverage for the music pipeline:** #2 for Vorbis at 48 kHz
  (a rate different from #1's 44.1 kHz and from the sounds file's 22 kHz),
  #4 or #5 for FLAC, #3 for Opus, and #1's "Splash" track for MP3. All
  four map-patch plugins are interchangeable at the loader level: same
  Plugin.xml shape, same checksums, one `<music>` per level.
- Listening-only albums (#9-11), the ModDB map variant (#12, superseded by
  #1) and dead links (#7-8) need no engine work.

## 3. Audio formats and plugin behaviour the new engine must support

File references are to this checkout, 27 September 2026. The sound and
music code is upstream Aleph One 1.7-era (the OpenAL Soft rewrite of 2022)
with Durandal's occlusion and reverb additions marked "Durandal (A1)/(A2)".

### 3.1 The Sounds file (item 7)

**Container** (`Source_Files/Sound/SoundFile.cpp:376-419`, struct in
`sound_definitions.h:103-115`): a 260-byte header, `version` (0 or 1),
tag `'snd2'`, `source_count` (int16, "usually 2: 8-bit, 16-bit"),
`sound_count`, then `source_count x sound_count` definitions of 64 bytes,
all of source 0 first. Marathon 2's `Sounds.sndA` is version 1, 2 sources,
203 sounds, 14.2 MB; Infinity files have 215-216. A file with
`sound_count == 0` is treated as old-format single-source (391-395). The
file stays open; sample data is read lazily (421-422; `M2SoundFile::
GetSoundData`, 440-443).

**Definition** (`SoundDefinition::Unpack`, `SoundFile.cpp:288-316`):
`sound_code` (NONE = empty slot), `behavior_index` (0 quiet, 1 normal, 2
loud), `flags`, `chance`, `low_pitch`/`high_pitch`, `permutations` (1-5,
`MAXIMUM_PERMUTATIONS_PER_SOUND`, `SoundFile.h:104`),
`permutations_played` (a bitmask), `group_offset`, `single_length`,
`total_length`, `sound_offsets[5]`. Flags (`sound_definitions.h:60-69`):
cannot be restarted 0x1, does not self-abort 0x2, resists pitch changes
0x4, cannot change pitch 0x8, cannot be obstructed 0x10, cannot be media
obstructed 0x20, is ambient 0x40 (not loaded unless Ambient Sounds is on,
`SoundManager.cpp:261-264`). Three fields are parsed and never read:
`chance` (its check was removed in the 2022 OpenAL rewrite), `low_pitch`/
`high_pitch`, and the per-sample loop points (below). A new engine must
read them for compatibility but need not honour them to match today's
behaviour.

**Permutations** (`SoundManager::GetRandomSoundPermutation`,
`SoundManager.cpp:994-1019`): with the More Sounds preference, a
"no repeat until all played" bag: random start from `local_random()`, walk
forward to the next unplayed slot, mark it. Without More Sounds, always
slot 0, and only slot 0 is loaded (`254`). `local_random()`
(`GameWorld/world.cpp:440-455`) is the 16-bit LFSR with its own seed,
separate from the networked `global_random()`, so permutation choice does
not affect sync; it is shared with random-sound images and shell casings
(`map.cpp:2766-2774`, `weapons.cpp:4077-4079`). A replacement must
consume it in the same order.

**Pitch** (`CalculatePitchModifier`, `SoundManager.cpp:932-949`): applied
as `AL_PITCH` (`SoundPlayer.cpp:187`); `_sound_cannot_change_pitch` forces
1.0; without `_sound_resists_pitch_changes` the deviation is halved
(faithful to Bungie's code, inverted against the flag comment). The
incoming pitch comes from object, projectile, effect and monster
definitions (`map.cpp:2496-2503`) and from random-sound images, the only
random pitch in the game.

**Sample headers** (`SoundHeader::Load`, `SoundFile.cpp:113-142`): the
classic Mac Sound Manager forms. Standard (0x00): 8-bit unsigned mono,
rate as 16.16 fixed (22254.54 Hz), data at offset 22. Extended (0xFF):
channels, rate, `sample_size` 8 or 16, `num_frames`, data at 64. Compressed
(0xFE) accepted only for `'twos'` uncompressed signed 8-bit. Data is read
into a byte vector; signed 8-bit becomes unsigned, 16-bit is byte-swapped
to native (`144-184`). The rate becomes the OpenAL buffer frequency as
`rate >> 16` (`SoundPlayer.cpp:27, 34`); OpenAL resamples. Formats:
`AL_FORMAT_MONO8/STEREO8`, `MONO16/STEREO16`, `MONO_FLOAT32/STEREO_FLOAT32`
(`AudioPlayer.h:94-101`). `loop_start`/`loop_end` are stored
(`SoundFile.cpp:54-55, 72-73`) and never used: ambient loops are
re-triggered with `soft_rewind` (`SoundManager.cpp:1129-1144`,
`SoundPlayer::Rewind`, `SoundPlayer.cpp:119-146`).

**8-bit or 16-bit** (item 7's question): the preference bit
`_16bit_sound_flag = 0x0010` (`SoundManagerEnums.h:393`), on by default
(`SoundManager.cpp:762-771`), the "Source: 8-bit Slot / 16-bit Slot" toggle
in the Sound dialog (`Misc/preferences.cpp:1654-1657, 1703`). `OpenSoundFile`
sets `sound_source` from it and forces 8-bit when the file has one source
(`SoundManager.cpp:200-202`); `SetStatus(true)` sets it again without that
guard (`816`), a latent silence for single-source files. `GetSoundDefinition`
(`861-882`) tries, in order: a sounds-patch definition for the current
source, a patch for the other source ("they most likely want to hear the
patched sounds"), the file's definition for the current source, and, for
16-bit with zero permutations, the 8-bit one. The 16-bit flag also doubles
the sample cache budget (`801-814`: MORE 600 KB or MIN 300 KB, + 1 MB
ambient, x2 for 16-bit, x16: about 50 MB by default).

**Marathon 1 / Infinity**: `M1SoundFile` reads `'snd '` resources by ID,
permutations as consecutive IDs, behaviour hard-coded loud, one source
(`SoundFile.cpp:460-522`); Infinity uses the same `'snd2'` layout with more
indices. Sounds patches hard-code 215 as the boundary between the 8-bit
and 16-bit halves (`SoundsPatch.cpp:128-133`).

### 3.2 MML sound replacement (item 8)

**Element** `<marathon><sounds>` (`XML/XML_MakeRoot.cpp:108-109`; parser
`SoundManager.cpp:1284-1375`), parsed even for menu-only MML loads. On
`<sounds>` itself, 17 int16 attributes rename the engine's fixed sounds:
`terminal_logon`, `terminal_logoff`, `terminal_page`, `teleport_in`,
`teleport_out`, `got_powerup`, `got_item`, `crunched`, `exploding`,
`breathing`, `oxygen_warning`, `adjust_volume`, `button_success`,
`button_failure`, `button_inoperative`, `ogl_reset`, `center_button`
(`1308-1324`; defaults `1201-1223`; not restored by `reset_mml_sounds`,
`1258-1282`). Children: `<ambient index=0-27 sound=>`, `<random index=0-4
sound=>`, `<dialog index=0-8 sound=>` remap those tables (`1326-1346`;
enums `SoundManagerEnums.h:31-76`); `<sound index="N" slot="0-4"
file="path"/>` replaces one permutation's audio (`1348-1374`; `index`
unbounded, the engine's `_snd_*` index, not `sound_code`; `slot` defaults
to 0); `<sound_clear/>` empties the table. Elements apply in document
order. `file` is resolved at parse time through `FileSpecifier::
SetNameWithPath` (`Files/FileHandler.cpp:759-781`) under the plugin's
`ScopedSearchPath` (`XML/Plugins.cpp:178-186`), so paths are relative to the
plugin folder; a missing file leaves an empty specifier that cancels any
earlier replacement of that slot. Storage: `SoundReplacements`, a map
keyed by (index, slot) (`Sound/ReplacementSounds.h:46-66`); each add,
remove or reset unloads that index from the cache (`ReplacementSounds.cpp:
68-87`).

**Limits**: MML cannot add sounds or permutations. `LoadSound` iterates
only the original definition's `permutations` (`SoundManager.cpp:254,
272`) and refuses a missing or empty definition (`249-259`); this is why
Scuba Sounds steals a duplicate terminal slot. With More Sounds off only
slot 0 plays.

**Decoders** (item 8's formats): one, `SndfileDecoder` over libsndfile via
`sf_open_virtual` on SDL_RWops (`Sound/Decoder.cpp:30-46`,
`SndfileDecoder.cpp:29-97`). It always yields 32-bit float at the file's
rate; `channels == 2` is stereo, anything else is read as mono. This build
links libsndfile 1.2.2 with its `external-libs` and `mpeg` features
(`vcpkg/installed-arm64-osx/vcpkg/status`; `nm` shows `ogg_vorbis_open`,
`ogg_opus_open`, `flac_open`, `mpeg_decoder_init`; link flags
`-lsndfile -lvorbis -lFLAC -lopus -lmpg123 -lmp3lame -logg`, `Xcode/
AlephOne.xcodeproj/project.pbxproj:5455-5466, 5602-5613`). No Vorbis, FFmpeg
or MAD decoder classes exist in the tree, and no `HAVE_*` audio defines.
Readable formats: WAV, AIFF, CAF and the other libsndfile PCM containers,
FLAC, Ogg Vorbis, Ogg Opus, MP3. Not AAC/M4A.

**Loading and caching**: nothing is decoded at parse time. `SoundManager::
LoadSound` (`245-298`) runs at level entry for monster and projectile
sounds (`GameWorld/marathon2.cpp:692`, `monsters.cpp:761-780`,
`projectiles.cpp:635-636`) and lazily on first play (`378, 414`). Per slot
it takes patch data, else the sounds file's data, and if a replacement is
registered decodes the whole file to float (`ExternalSoundHeader::
LoadExternal`, `ReplacementSounds.cpp:26-53`), using the file's header when
that succeeds (`896-930`), so one index can mix float replacement slots
with 8-bit originals. Cache: `SoundMemoryManager` (`49-142`), LRU by
`machine_tick_count`, budget above; replacements count at 4 bytes per
sample per channel (44.1 kHz stereo is 353 KB per second). Cleared on
every `SetStatus(true)`, on non-new-game level loads ("hack to get new
MML-specified sounds loaded", `Files/game_wad.cpp:789-791`), on game exit
and on low memory.

**Sounds patches** (`<sounds_patch file>` in Plugin.xml, `Plugins.cpp:
472-478`; or a map's `'SnPa'` wad tag, `game_wad.cpp:1785-1787`): rebuilt
on every `entering_map` (`marathon2.cpp:687-690`), records of `'sndc'` +
index (>= 215 means the 16-bit source) + a whole 64-byte definition +
Sound Manager headers and data (`SoundsPatch.cpp:109-163`). A patch
replaces the whole definition (flags, behaviour, permutation count), keeps
its data 8/16-bit and resident for the level, and deletes any MML
replacement for the permutations it patches (`165-175`), which stays gone
until the MML is re-parsed. No pack in the table uses a sounds patch.

### 3.3 Music (item 9)

**Formats**: the same `StreamDecoder` as sounds (`Sound/Music.cpp:206`),
so Ogg Vorbis, Opus, FLAC, MP3, WAV and AIFF. Files with more than two
channels are not handled; `Duration()` truncates to whole seconds
(`SndfileDecoder.h:43`).

**Classes**: the `Music` singleton holds slots, 0 `Intro` and 1 `Level`
reserved, Lua tracks from 2 (`Music.h:30-110`); each slot owns a
`MusicPlayer` (priority 5, above every sound, `MusicPlayer.h:76`) with
`MusicParameters{volume, loop}` and the segment, edge and sequence machinery
of section 3.4. `AddTrack` opens the decoder on the main thread
(`Music.cpp:204-210`); `OpenALManager::PlayMusic` hands the player to the
audio thread through a lock-free queue (`OpenALManager.cpp:206-211`).
`StreamPlayer` serves only the intro video's audio.

**Title music**: `Scripts/Filenames.mml:9` maps string 10 to `Music.ogg`
(22.05 kHz stereo, 94 s), found by walking `data_search_path` (`Files/
preprocess_map_sdl.cpp:48-58, 105-108`; the path order is `shell.cpp:
407-457`: bundle data, scenario dir, `ALEPHONE_DATA`, legacy, then
`~/Library/Application Support/Durandal`). It loops on the menu, fades out
over 0.5 s when a game starts (`Misc/interface.cpp:3390-3420`) and plays
again on the epilogue unless `<end_screens music>` names another file.
There is no music-file preference and no "music on" switch: only the volume
slider, `music_db`, -20 to +20 dB (`preferences.cpp:1574-1592`), off at the
minimum.

**Level music assignment**: only two elements exist, `<music file="..."/>`
and `<random_order on="bool"/>`. In the map's own script (TEXT resource
128, root `<marathon_levels>`) they sit inside `<level index=n>`,
`<default>`, `<end>` or `<restore>` (`XML/XML_LevelScript.cpp:600-614,
659-681`); in ordinary MML, plugin MML included, only inside
`<marathon><default_levels>` (`XML_MakeRoot.cpp:155-156`; parser `514-532`),
which feeds the Default pseudo-level that runs on every level. So a plugin
MML file cannot give levels different tracks; per-level music from a plugin
needs a `<map_patch>` that replaces TEXT 128 (`Plugins::get_resource`,
`Plugins.cpp:499-528, 625-638`; asked first by
`get_text_resource_from_scenario`, `RenderOther/images.cpp:1494-1507`), which
is exactly what the M2SE plugin does (its 128.txt is the 41-level list) and
why it requires Aleph One 1.7 ("Support for map-specific resources
plugins", release 20231125). Marathon 1's `Music/%02d.ogg` convention is
used only for `MARATHON_ONE_DATA_VERSION` maps (`Music.cpp:269-287`,
`game_wad.cpp:1702-1730`); an M2 map's `song_index` is its landscape index.
The music path is kept as a raw string and resolved when the level starts
(`XML_LevelScript.cpp:386-391`), after the plugin's search-path scope has
ended, so a plugin's `<default_levels><music file>` must be written
relative to a search-path root, not to the plugin folder; map-patch plugins
escape this because `set_map_checksum` pushes their directory onto the path
for good (`Plugins.cpp:640-669`).

**Playlist lifecycle**: `ResetLevelScript` fades every slot out over 500
ms, waits, clears the playlist and re-parses base and plugin MML
(`XML_LevelScript.cpp:209-230`); `RunLevelScript` runs the Default then the
level script and `SeedLevelMusic` (`235-240`, a private `GM_Random`,
`Music.cpp:262-267`, so films are safe). `LoadLevelMusic` opens the chosen
file with `loop = (playlist.size() == 1)` (`250-255`); a second track turns
looping off (`308-321`); the next track is chosen sequentially or by
`KISS() % N`, which can repeat (`323-339`). `leaving_map` stops music
abruptly and `Pause` drops every Lua slot (`marathon2.cpp:651-655`,
`Music.cpp:59-67`).

**Marathon 2 data**: `Music.ogg`, `Filenames.mml`, and nothing else
mentions music; `Map.sceA` has no `marathon_levels` script; none of the five
bundled plugins (Basic HUD, Enhanced HUD, Stats, Theme, Transparent
Liquids) or the CFP art packs carries audio or music MML.

### 3.4 Why looping is not gapless

- Streaming (`Sound/AudioPlayer.h:71-72`, `AudioPlayer.cpp:69-103`): four
  OpenAL buffers of 8192 **bytes** (`buffer_samples` is a byte count) per
  source. Music is float32, so a buffer is 1024 stereo frames, 23 ms at
  44.1 kHz; the queue holds about 93 ms. All decoding happens on the SDL
  audio callback thread (`OpenALManager.cpp:92-115, 264-267, 499-503`,
  callback size 1024 frames, `SoundManager.h:101-102`), one refill per
  callback, no background decode thread, no pre-decoding of the next file.
- **A single looping file loops sample-exactly**: at end of data with
  `loop` on, `GetNextData` calls `SwitchSegment(nullopt)`, which seeks the
  decoder to 0 and keeps filling the same buffer (`MusicPlayer.cpp:196-216`).
  Whether the join is inaudible then depends on the codec's priming and
  padding: exact for Vorbis, FLAC and WAV; for MP3 it depends on how
  libsndfile's mpg123 path treats the LAME gapless tag (unverified).
- **Between playlist tracks there is a gap and a hard cut**: the tail is
  queued as a partial buffer, the queue drains, OpenAL stops the source,
  the player is retired on the next callback (`AudioPlayer.cpp:123-139`,
  `OpenALManager.cpp:104-110`), and the main thread's `Music::Idle`, which
  runs at most every 16 ms (`shell.cpp:724-781`, `shell_misc.cpp:64-68`),
  opens the next file, parses its header and makes a new player
  (`Music.cpp:113-116, 198-202`); the next callback fills four buffers and
  starts it. Result: one to three callback periods of silence (25-70 ms),
  more if the main thread is busy, no fade, no crossfade.
- **The crossfade engine exists and is unused**: `MusicPlayer` has
  per-sample linear and equal-power fades (`MusicPlayer.cpp:124-159`),
  crossfade mixing (`161-172`, `68-122`), sequence transitions at offsets
  (`174-187, 218-255`) and mid-buffer decoder switching, refused when rate,
  channels or format differ (`228-229`). Nothing outside `Music.*` calls
  `SetSegmentEdge`, `AddSequence`, `AddSegmentToSequence` or
  `SetSequenceTransition`; neither Lua nor MML exposes them. The 1.7 release
  note "new music API for Lua which allows multiple tracks and crossfades"
  is delivered as independent slots plus volume fades, not as sample-level
  crossfades. A format change mid-sequence also waits for the queue to
  drain (`AudioPlayer.cpp:73-83`), which recreates the gap.
- Slot fades (`Music::Fade`, `Music.cpp:69-87, 134-180`): linear or
  sinusoidal against the audio tick, delivered to the audio thread through
  a 5-entry queue that drops when full (`AudioPlayer.h:34-69`), so a fade
  steps every 16-23 ms; the duration parameter is a `short` (`Music.h:59,
  85`), so a fade over 32.767 s wraps and never completes.
- Fork specifics: music never sends to Durandal's reverb and never gets
  the underwater low-pass (`AudioPlayer.cpp:167-170`).

### 3.5 The Lua music API (item 10)

`Source_Files/Lua/lua_music.cpp`; registered for every Lua state (`lua_script.cpp:889`), but the mutating functions only where
`world_mutable() || music_mutable()` (`lua_music.cpp:210-227`). HUD Lua has
no music API (`lua_hud_script.cpp:197-199`).

| Lua | Engine | Lines |
|---|---|---|
| `Music.new(file [, volume=1 [, loop=true]])` -> track | `Music::Add`: a new slot, decoder opened, not started | `93-121`; `Music.cpp:89-96` |
| `Music.play(file, ...)` | `PushBackLevelMusic` per existing file; missing files skipped silently; duplicates dropped | `123-146` |
| `Music.clear()` | `ClearLevelPlaylist` (a single looping track keeps looping) | `4-8` |
| `Music.fade([seconds=1])` | `Music::Fade(0, ms, Linear)` on every slot, then clear the playlist | `10-16` |
| `Music.stop()` | clear the playlist and stop the level slot; Lua slots unaffected | `148-154` |
| `Music.valid(file, ...)` | file exists and `StreamDecoder::Get` succeeds | `156-177` |
| `track.volume` (get/set 0..1), `track.active` (read) | `Slot::SetVolume` / `Slot::Playing` | `55-86` |
| `track:play()` | restart from the beginning; no-op if playing | `35-43` |
| `track:stop()` | `Slot::Pause` -> `AskStop` | `45-53` |
| `track:fade(volume [, seconds=1 [, stop=true]])` | `Slot::Fade(..., Linear, stop)` (docs say stop defaults false; code says true) | `18-33` |

Legacy globals `clear_music`, `fade_music`, `play_music`, `stop_music` wrap
these (`lua_player.cpp:3072-3129`). File paths: a solo-Lua plugin's state has
its search path set to the plugin directory and looks **only** there
(`FileHandler.cpp:783-802`, `lua_script.cpp:808-822, 2065-2107`); map Lua and
netscripts use the global path. This is how the Solar Soundtrack's
`Music.new("The Segment Folder/...opus")` resolves. Exclusivity: a solo-Lua
plugin declares `<solo_lua file><write_access>music</write_access>`; `music`
(0x04) is in the exclusive mask with `world`, `fog`, `overlays`
(`XML/Plugins.h:47-72`, `Plugins.cpp:45-60, 385-431, 674-735`), so one music
plugin can run beside fog, overlay, ephemera and sound plugins but not
beside a world plugin, and later names win. The legacy `solo_lua="..."`
attribute (the Solar plugin uses it) means world access, which also
disables achievements (`lua_script.cpp:2117-2137`). Solo-Lua plugins are
disabled whole in net games or when the user picks their own solo script
(`Plugins.cpp:97-105, 716-724`).

The Solar Soundtrack's pattern: per level, `Triggers.init` creates all
segments with `Music.new` and starts one with `Music.play` or `track:play()`;
`Triggers.postidle` polls `Game.ticks`, positions and
`Level.calculate_completion_state` to fade one segment (`track:fade(0)`) and
start the next; `restoring_game` starts a mid-level segment and calls
`Game.restore_saved()`. Nothing in it needs sample-accurate transitions,
because the engine offers none: each swap is a volume fade over seconds.

### 3.6 Positioning, falloff, obstruction, ambient and random sounds (item 11)

Read as "what a spatial audio system replaces"; DurandalAcoustics (A2) and
DurandalReverb (A1) already sit on these hooks (`CLAUDE.md`, Round 9).

**Sources and listener**: `SoundManager::PlaySound(index, world_location3d*,
identifier, pitch, soft_rewind)` (`SoundManager.cpp:372-410`) fills
`SoundParameters` (`SoundPlayer.h:42-57`): 2D when there is no source; a
copy of the position, plus a pointer to the live position when Active
Panning is on and an identifier is given, which is how an object's sound
follows it; obstruction flags and Durandal occlusion for 3D; pan and gains
for 2D. `DirectPlaySound(index, direction, volume, pitch)` (`412-434`) has
no position: pan from `direction - yaw`, and a `NONE` direction plays at
full gain, ignoring `volume`. World wrappers in `GameWorld/map.cpp:
2477-2544` (`play_object_sound` with the object index as identifier,
`play_polygon_sound` at the polygon centre, `play_side_sound`,
`play_world_sound`); Lua's `play_sound` at a point (`Lua/lua_map.cpp:
1565-1578`). Monsters' sound position is their mid-height
(`monsters.cpp:419-420`), the player's its camera. The listener is
`current_player->camera_location` cast to `world_location3d`
(`map.cpp:2546-2553`; struct layout `player.h:363-370`), written once per
game tick (`physics.cpp:524-527`); its "velocity" field is three unrelated
player fields (`docs/AUDIT.md:669-672`). OpenAL gets it from
`SoundManager::UpdateListener` (`485-492`, only with 3D on) via a lock-free
slot applied every audio callback (`OpenALManager.cpp:118-145`): position
(x, z, y)/WU, orientation from yaw and pitch. `Idle` (`515-524`) runs from
the event poll, every 16 ms or every frame with Durandal's per-frame look.
Durandal's `RenderListener` (`Sound/DurandalAcoustics.cpp:219-237`) feeds
the rendered camera per frame with zero velocity, for the 3D listener only:
the 2D pan, occlusion and reverb still use the tick listener.

**2D path (the owner's setting, 3D off)**: `distance_to_volume`
(`SoundManager.cpp:591-659`) interpolates a depth curve per behaviour
(`sound_definitions.h:156-175`; volumes out of 256, distances in WU): quiet
256 at 0 to 0 at 5; normal 256 to 1 WU then 0 at 10; loud 256 to 2 WU then
32 at 15 (never silent); obstructed curves 0, 128 to 0 at 7, 192 to 0 at
10; media-muffled halves it. Pan: `AngleAndVolumeToStereoVolume`
(`951-992`): ahead L = R = v, behind 0.75 v, at the side 1.5 v near ear and
0.25 v far ear. Gains reach OpenAL as a source placed on a +-30 degree arc
in front of the listener with no distance model
(`SoundPlayer.cpp:191-225, 266-274`); because the gains are absolute the
pan collapses to centre as a sound gets quiet. Only object-attached sounds
are re-panned while playing (`450-483`); polygon, side, world and direct
sounds keep their first pan. Obstruction in 2D changes volume only.

**3D path**: `AL_INVERSE_DISTANCE_CLAMPED`, non-relative sources
(`SoundPlayer.cpp:275-283`), with per-behaviour rows `{reference, max,
rolloff, max gain, hf gain}` in WU (`SoundPlayer.h:141-157`): clear quiet
{0.5, 5, 1, 1, 1}, normal {2.5, 15, 1.7, 1, 1}, loud {3, 20, 1.2, 1, 1};
obstructed-or-muffled and obstructed-and-muffled rows with shorter range,
lower gain and an EFX low-pass of 0.1-0.3. `Simulate` (`76-98`) estimates
gain for culling (below max distance) and priority; changes smooth over
300 ms. No Doppler (no `alDopplerFactor`, no source velocity), no cones
(under `#if 0`, `231-243`). HRTF is allowed with 3D on and stereo output
(`preferences.cpp:1622-1633`); 2D sources under HRTF are duplicated to
stereo and played with direct channels (`SoundPlayer.cpp:285-300, 399-456`),
which by reading (untested) loses the pan of ambient and random sounds.

**Obstruction**: `_sound_obstructed_proc` (`map.cpp:2556-2627`) uses
`line_is_obstructed` (`2207-2285`), the AI's top-down 2D polygon walk that
ignores height, plus a media test (source under a liquid and listener not,
or the reverse: media-obstructed; both under the same: muffled). 3D asks
for both kinds (`GetSoundObstructionFlags`, `SoundManager.cpp:547-576`); 2D
and ambient do not distinguish, so a sound behind a wall and across a
liquid gets only the wall's treatment. Durandal's graded occlusion
(`DurandalAcoustics.cpp:177-217`, Classic tier): a height-aware ray walk
(0.35 per ledge passed, cap 0.8, 1 for a solid line) and a Dijkstra path
over polygon centres through openings of 1/8 WU up to 16 WU for sound
bending round corners, blended into the 2D curves (`613-624`) and the 3D
rows (`SoundPlayer::BehaviorFor`, `SoundPlayer.cpp:53-71`); it reads the map
only.

**Underwater and EFX**: one shared low-pass filter re-parameterised per
source (`OpenALManager.cpp:381-384`), and Durandal's `underwater_filter`
(highs to 25%, `23`) on 2D world sounds (`SoundPlayer.cpp:222-224`); the
"underwater" state comes from the reverb estimate, so the low-pass works
only with Room Reverb on. Reverb: one EAX (or plain) effect in one
auxiliary slot; `in_world` sounds send to it, interface, ambient, random,
music and streams do not (`SoundPlayer.cpp:260-264`, `AudioPlayer.cpp:
168-170`). The estimate (`Sound/DurandalReverb.cpp:91-187`): every 100 ms
a breadth-first walk from the listener's polygon through openings of 1/4
WU to 24 WU or 400 polygons, Sabine RT60 from volume and absorption (hard
0.1, landscape and leaked openings 1), mean free path for the delays,
glided on the audio thread with a 0.4 s time constant (`189-208`,
`OpenALManager.cpp:387-435`).

**Ambient sounds**: 28 ambient types map to sound indices
(`sound_definitions.h:179-210`, MML-overridable). Per Idle, `_sound_add_
ambient_sources_proc` (`map.cpp:2630-2749`) gathers, for the listener's
polygon only: the polygon's ambient image (non-positional), the media's
over/under sound, a moving platform's sound, and each placed sound-source
object near the polygon (precomputed at map load, `map_constructors.cpp:
1143-1187`, with an apples-to-oranges distance test that only matches
sources within about 0.1 WU of an edge), with negative volumes meaning
"from a light's intensity". `AddOneAmbientSoundSource` (`SoundManager.cpp:
667-748`) merges per sound index into 5 candidates, attenuates each with
the unobstructed curve, obstruction and Durandal occlusion, pans and sums,
capped at 384; `UpdateAmbientSoundSources` (`1038-1147`) keeps the loudest 4
(`MAXIMUM_AMBIENT_SOUND_CHANNELS`), soft-stops the rest, and starts new ones
as 2D panning players with fade-in and re-triggered looping. Ambient
sounds are 2D even with 3D on and never reverberate.

**Random sounds**: 5 types (`sound_definitions.h:214-222`; water drip,
surface and underground explosion, owl, creak). `handle_random_sound_image`
(`map.cpp:2751-2787`) runs inside the game tick (`marathon2.cpp:425`) on the
camera polygon's image only: when its phase hits 0 it draws volume,
direction and pitch offsets with `local_random()` and calls
`DirectPlaySound`, then resets the phase with another `local_random()`.
`image->phase` is game-world data mutated in the tick, so a replacement must
keep this code path and its `local_random()` sequence exactly (film
determinism). No world position, no distance falloff, no reverb; a
non-directional image ignores its volume (the `DirectPlaySound` quirk).

**Channels and priority**: with 3D off, or `_sound_cannot_be_restarted`,
each sound index has one voice (`GetSoundPlayer`, `SoundManager.cpp:
337-361`; `UpdateExistingPlayer`, `843-859`), rewound if restartable and the
new one is louder by 1/6 or from the same source, else dropped; with 3D on,
up to 3 voices per sound (`MAX_SOUNDS_FOR_SOURCE`). A rewind is refused
within 83 ms (35 ms for same-source fast rewinds, `SoundPlayer.h:132-133`).
Priority is the simulated gain; music 5, streams 10. The OpenAL source pool
is the device's mono + stereo source count, each with 4 x 8 KB buffers
(`OpenALManager.cpp:437-474`); an empty pool steals the quietest player's
source (`224-236`), and a player with no source is dropped, not queued
(`101-111`). The main-to-audio queue holds 256 entries and its `push`
result is ignored (`OpenALManager.h:127`, `.cpp:202`).

## 4. Upgrade opportunities (observations, no design)

- **Gapless playlists and transitions**: the pieces exist. A level playlist
  as one `MusicPlayer` sequence of N segments with `FadeType::None` edges
  and a last-to-first edge would already concatenate inside a buffer;
  random order needs edges chosen at switch time; tracks of different rate
  or channel count need resampling or the format-change drain reappears;
  MP3 needs encoder-delay trimming. Sample-accurate crossfades need the
  integer `Duration()` fixed and the edge API exposed to Lua and MML. On
  AVAudioEngine the natural shape is a scheduled-buffer or file player per
  track with sample-time scheduling, which removes the audio-callback
  decode and the main-thread file open from the transition path.
- **Dynamic music**: the Solar Soundtrack only needs the fades it uses
  today; a beat-aligned or bar-aligned switch (segment edges at musical
  offsets) would be a new capability that no pack yet asks for. Keeping
  `Music.new/play/fade/stop/active/volume` with the plugin-relative search
  path is the compatibility floor.
- **Spatial audio**: the 2D pan path is what is heard now. An
  AVAudioEnvironmentNode with HRTF would replace `SetUpALSourceIdle/3D`
  and the OpenAL listener, keep the behaviour tables as the distance
  model, and could give ambient, random and 2D sounds real positions
  (ambient sources and random images have none today). The listener should
  be the rendered camera per frame (DurandalAcoustics already provides it)
  for pan and occlusion too, not only for 3D. The `local_random()` order
  in random-sound images and permutation choice must be preserved.
- **Geometry-driven reverb and occlusion**: DurandalReverb's walk gives one
  room; a per-source send (distance and path-dependent wet level, the
  polygon the source is in against the listener's) and a second room for
  what is heard through a doorway are the observed gaps. DurandalAcoustics'
  occlusion already blends obstruction, so the missing pieces are a
  frequency-dependent obstruction filter in the 2D path (today volume only)
  and reverb on ambient loops.
- **Housekeeping the packs expose**: the ambient-source precompute bug
  (0.1 WU), `DirectPlaySound`'s ignored volume for non-directional random
  sounds, the `short` fade duration, the unread `chance` and pitch-range
  fields, and the 8-bit forced-source edge case are all things the new
  engine could either fix or reproduce; each is a visible or audible
  change to weigh against "the game intact".

## 5. Assets on disk

`Assets/audio/` (git-ignored): `M2SE_Music_Plugin.zip` (72 MB),
`SME4M2_Version_1.2.1.zip` (159 MB), `Marathon_2_SE_Tracks.zip` (72 MB),
`Garrison_Album.zip`, `Volunteers_Album.zip`, `Lh_owon_Album.zip`,
`Marathon_Infinity_Remastered_Sounds_v1.1.zip` (22 MB),
`M2I_HD_Sounds_Mod.zip` (81 MB), `realisticsounds.zip`,
`Google_Translated__Marathon_Infinity_Bob_and_Vacbob_lines.zip`,
`scuba_sounds.zip`, and the four manual downloads `M2 OST Feel the
Noise - Talashar.7z` (407 MB, Steam Workshop), `OneDrive_2026-09-27.zip`
(498 MB, the M2SE Music Remastered plugin), `Aaron Freed - 2025-WIP #5 -
To maratho.zip` (1.53 GB) and `Marathon_2_-_Special_Edition_OST.zip`
(85 MB, ModDB), each unpacked beside it as `x_<name>/`; SHA-256 sums in
`SHA256SUMS.txt`. Two are linked into `~/Library/Application Support/
Durandal/Plugins` for comparison: the M2SE Music Plugin (#1) and
the Solar Soundtrack (#3); the DURANDAL dialog's ART tab chooses which one
plays (Soundtrack: None / M2SE Music / The Solar Soundtrack for M2), since
the engine would play both at once.

## 6. Open questions and manual downloads

- Fetched by hand on 27 September and now inspected (rows #2, #4, #5,
  #12): Feel the Noise from the Steam Workshop
  (https://steamcommunity.com/sharedfiles/filedetails/?id=3332007811),
  the M2SE Music Remastered plugin from Aaron Freed's OneDrive
  (https://1drv.ms/f/s!AuD0MykSsmaRgcg-tUdntfK3BjSSDg?e=sEaQJa), his
  To maratho prototype from Dropbox, and the ModDB M2SE OST
  (https://www.moddb.com/games/marathon-2-durandal/addons/marathon-2-special-edition-ost).
  Still unverified: the brief's claim of free looping versions of Feel the
  Noise on Bandcamp (the album there is the 22-track listening version,
  name-your-price; no separate looping release is listed on
  https://talashar.bandcamp.com/music; the plugin's own tracks are the
  in-game cuts, which loop because each level has one track).
- **ModDB add-on list** (Cloudflare challenge blocks fetching):
  https://www.moddb.com/games/marathon-2-durandal/addons . The Psykosonik
  / Death Grips derivative of #12 is known from a search snippet only
  (name, author and date unknown); it uses commercial artists' music.
- **The Man's promised remaster from CD-quality sources** ("expect a major
  new release sometime in 2021") has no Simplici7y entry; his user page
  lists nothing newer for sounds.
- **Feel the Noise's in-game cuts** are not the album's tracks (different
  titles and lengths, 24 s to 16.8 min); whether they were composed to
  loop is not stated; the brief says the soundtrack is "deliberately
  non-looping", so the engine's sample-exact loop will play a hard join.
- **MP3 loop joins**: whether libsndfile's mpg123 path honours the LAME
  gapless tag was not tested; the M2SE plugin's one MP3 is the intro track.
- **Calle de Las Sombras (#19)** and the two novelty voice sets (#20) were
  not downloaded.
- Engine points read but not exercised: the HRTF direct-channel remix
  losing 2D pans; the single-source 16-bit silence after `SetStatus`; these
  are readings of the code, not observed behaviour.
