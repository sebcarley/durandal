#ifndef DURANDAL_REVERB_H
#define DURANDAL_REVERB_H
/*
	DurandalReverb.h — Durandal project

	Room reverb (roadmap A1). A few times a second, the room around the
	listener is sized up from the map: a walk from the listener's polygon
	through open doorways (closed doors and small gaps stop it), out to a
	fixed reach, adds up its volume, its hard surfaces and what is open to
	the sky (landscape surfaces) or leads further on. Sabine's formula
	gives a decay time; the mean free path gives the timing and strength of
	the early reflections; open sky soaks reverb up. Under a liquid,
	OpenAL's underwater reverb, and world sounds lose their highs.

	OpenALManager glides one shared reverb towards the estimate, and world
	sounds (not the interface or music) send to it. Sound only: reads the
	map on the main thread and changes nothing.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "world.h"

namespace DurandalReverb {

// EAX reverb properties (OpenAL EFX), the ones the estimate sets
struct Reverb {
	bool active = false;		// off: the reverb is silent
	bool underwater = false;	// the listener is under a liquid
	float density = 1.0f;
	float diffusion = 1.0f;
	float gain = 0.3f;
	float gain_hf = 0.89f;
	float decay = 1.0f;			// seconds
	float decay_hf_ratio = 0.83f;
	float reflections_gain = 0.3f;
	float reflections_delay = 0.01f;
	float late_gain = 1.0f;
	float late_delay = 0.02f;
	float air_absorption_hf = 0.994f;

	bool operator==(const Reverb& o) const {
		return active == o.active && underwater == o.underwater && density == o.density && diffusion == o.diffusion &&
			gain == o.gain && gain_hf == o.gain_hf && decay == o.decay && decay_hf_ratio == o.decay_hf_ratio &&
			reflections_gain == o.reflections_gain && reflections_delay == o.reflections_delay &&
			late_gain == o.late_gain && late_delay == o.late_delay && air_absorption_hf == o.air_absorption_hf;
	}
	bool operator!=(const Reverb& o) const { return !(*this == o); }
};

// Main thread: the reverb for the room around `listener` (inactive when
// Room Reverb is off or the listener is nowhere).
Reverb Estimate(const world_location3d& listener);

// Audio thread: `current` moved towards `target` for `seconds`, so
// walking between rooms changes the sound smoothly.
Reverb Glide(const Reverb& current, const Reverb& target, float seconds);

}

#endif
