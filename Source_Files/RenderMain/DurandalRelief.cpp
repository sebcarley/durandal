/*
	DurandalRelief.cpp — Durandal project

	See DurandalRelief.h.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "DurandalRelief.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace DurandalRelief {

void Derive(uint8_t* texels, int width, int height, const uint32_t* colours, bool flat)
{
	const size_t n = size_t(width) * height;
	if (flat || width <= 0 || height <= 0)
	{
		for (size_t i = 0; i < n; ++i)
			texels[4 * i + 3] = 0;
		return;
	}

	// Brightness of each texel's colour
	std::vector<float> lum(n);
	for (size_t i = 0; i < n; ++i)
	{
		const uint8_t* c = reinterpret_cast<const uint8_t*>(colours + texels[4 * i]);
		lum[i] = (0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2]) / 255.0f;
	}

	// A light 3x3 blur (the art tiles, so it wraps), so single-texel noise
	// does not read as bumps
	std::vector<float> soft(n);
	double sum = 0, sum2 = 0;
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x)
		{
			float s = 0;
			for (int j = -1; j <= 1; ++j)
				for (int i = -1; i <= 1; ++i)
				{
					const int xx = (x + i + width) % width, yy = (y + j + height) % height;
					s += lum[size_t(yy) * width + xx] * ((i == 0 && j == 0) ? 4.0f : (i == 0 || j == 0) ? 2.0f : 1.0f);
				}
			s /= 16.0f;
			soft[size_t(y) * width + x] = s;
			sum += s;
			sum2 += double(s) * s;
		}

	// Around the texture's own mean, contrast set so its spread fills about
	// two thirds of the range: dark seams low, bright ridges high, whatever
	// the texture's overall brightness
	const double mean = sum / n;
	const double spread = std::sqrt(std::max(sum2 / n - mean * mean, 1e-6));
	const float gain = float(std::min(0.33 / spread, 6.0));
	for (size_t i = 0; i < n; ++i)
	{
		const float h = 0.5f + float(soft[i] - mean) * gain;
		texels[4 * i + 3] = uint8_t(std::clamp(int(std::lround(1 + 254 * std::clamp(h, 0.0f, 1.0f))), 1, 255));
	}
}

}
