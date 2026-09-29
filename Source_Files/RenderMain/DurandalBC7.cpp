/*
	Durandal: BC7 mode 6 encoder and decoder (DurandalBC7.h).

	Per block: the pixels' principal axis in RGBA space gives the first
	pair of endpoints (its extremes); the indices along the segment are
	refit by least squares twice; the endpoints are quantised to 7 bits
	plus the p-bit that best reconstructs them; the final index of each
	pixel is chosen against the palette the decoder will build. Bit
	layout (LSB first): mode 0000001, R0 R1 G0 G1 B0 B1 A0 A1 (7 bits
	each), P0, P1, then 4-bit indices, the first with its top bit implied.

	Fits weigh a pixel by its alpha (never below a fifth): a transparent
	pixel's colour hardly shows, so the opaque pixels of a block at a
	sprite's edge get the precision.
*/

#include "DurandalBC7.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <simd/simd.h>

namespace DurandalBC7 {

namespace {

// The 4-bit interpolation weights (BPTC), symmetric: w[15 - i] = 64 - w[i]
const int kWeights[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

// The index whose weight is nearest each position along the segment, 0..64
struct IndexTable
{
	uint8_t at[65];
	IndexTable()
	{
		for (int w = 0; w <= 64; ++w)
		{
			int best = 0;
			for (int i = 1; i < 16; ++i)
				if (std::abs(kWeights[i] - w) < std::abs(kWeights[best] - w))
					best = i;
			at[w] = uint8_t(best);
		}
	}
};
const IndexTable kIndexOfWeight;

inline int interpolate(int e0, int e1, int w)
{
	return ((64 - w) * e0 + w * e1 + 32) >> 6;
}

inline int index_along(float t)	// t: position along the segment, 0..1
{
	const int w = int(std::min(1.f, std::max(0.f, t)) * 64.f + 0.5f);
	return kIndexOfWeight.at[w];
}

// 7-bit endpoint and the p-bit its four channels share: the pair that
// reconstructs closest to the wanted 8-bit values
void quantise(simd_float4 v, int q[4], int& p)
{
	float best = 1e30f;
	for (int pb = 0; pb < 2; ++pb)
	{
		simd_float4 t;
		for (int c = 0; c < 4; ++c)
			t[c] = std::min(127.f, std::max(0.f, std::floor((v[c] - float(pb)) * 0.5f + 0.5f)));
		const simd_float4 d = v - (t * 2.f + float(pb));
		// Alpha weighs most: a transparent texel left at 1/255 by the
		// shared p-bit would still pass the sprites' alpha test and draw
		// its whole quad into the distance image
		const float err = simd_reduce_add(d * d * simd_make_float4(1.f, 1.f, 1.f, 64.f));
		if (err < best)
		{
			best = err;
			p = pb;
			for (int c = 0; c < 4; ++c)
				q[c] = int(t[c]);
		}
	}
}

struct BitWriter
{
	uint8_t b[16];
	int pos;
	BitWriter() : pos(0) { memset(b, 0, sizeof(b)); }
	void put(uint32_t v, int n)
	{
		for (int i = 0; i < n; ++i, ++pos)
			if ((v >> i) & 1)
				b[pos >> 3] |= uint8_t(1 << (pos & 7));
	}
};

struct BitReader
{
	const uint8_t* b;
	int pos;
	explicit BitReader(const uint8_t* bytes) : b(bytes), pos(0) {}
	uint32_t get(int n)
	{
		uint32_t v = 0;
		for (int i = 0; i < n; ++i, ++pos)
			v |= uint32_t((b[pos >> 3] >> (pos & 7)) & 1) << i;
		return v;
	}
};

void encode_block(const simd_float4 px[16], uint8_t out[16])
{
	// Weights: alpha, never below a fifth
	float wt[16];
	float wsum = 0;
	bool uniform = true;
	for (int i = 0; i < 16; ++i)
	{
		wt[i] = 0.2f + 0.8f * px[i].w * (1.f / 255);
		wsum += wt[i];
		uniform = uniform && simd_all(px[i] == px[0]);
	}

	// Mean and covariance of the block
	simd_float4 mean = 0;
	for (int i = 0; i < 16; ++i)
		mean += wt[i] * px[i];
	mean /= wsum;
	simd_float4 d[16];
	simd_float4 cov[4] = {0, 0, 0, 0};	// cov[a] = row a
	for (int i = 0; i < 16; ++i)
	{
		d[i] = px[i] - mean;
		const simd_float4 wd = wt[i] * d[i];
		cov[0] += wd.x * d[i];
		cov[1] += wd.y * d[i];
		cov[2] += wd.z * d[i];
		cov[3] += wd.w * d[i];
	}

	// Principal axis by power iteration, from the channel that varies most
	simd_float4 axis = 0;
	bool flat = uniform;
	if (!flat)
	{
		const simd_float4 diag = {cov[0].x, cov[1].y, cov[2].z, cov[3].w};
		int start = 0;
		for (int c = 1; c < 4; ++c)
			if (diag[c] > diag[start])
				start = c;
		axis = cov[start];
		for (int it = 0; it < 6; ++it)
		{
			const simd_float4 n = {simd_reduce_add(cov[0] * axis), simd_reduce_add(cov[1] * axis),
								   simd_reduce_add(cov[2] * axis), simd_reduce_add(cov[3] * axis)};
			const float len = simd_length(n);
			if (len < 1e-8f)
			{
				flat = true;
				break;
			}
			axis = n / len;
		}
	}

	// First endpoints: the extremes along the axis
	simd_float4 e0, e1;
	if (flat)
	{
		e0 = e1 = mean;
	}
	else
	{
		float tmin = 1e30f, tmax = -1e30f;
		for (int i = 0; i < 16; ++i)
		{
			const float t = simd_reduce_add(d[i] * axis);
			tmin = std::min(tmin, t);
			tmax = std::max(tmax, t);
		}
		e0 = simd_clamp(mean + axis * tmin, 0.f, 255.f);
		e1 = simd_clamp(mean + axis * tmax, 0.f, 255.f);
	}

	// Indices along the segment, then the endpoints refit by least squares
	int idx[16];
	for (int pass = 0; pass < 3; ++pass)
	{
		const simd_float4 dir = e1 - e0;
		const float len2 = simd_length_squared(dir);
		if (len2 < 1e-8f)
		{
			for (int i = 0; i < 16; ++i)
				idx[i] = 0;
			break;
		}
		const simd_float4 dirn = dir / len2;
		bool changed = pass == 0;
		for (int i = 0; i < 16; ++i)
		{
			const int k = index_along(simd_reduce_add((px[i] - e0) * dirn));
			changed = changed || k != idx[i];
			idx[i] = k;
		}
		if (pass == 2 || !changed)
			break;
		float aa = 0, ab = 0, bb = 0;
		simd_float4 ap = 0, bp = 0;
		for (int i = 0; i < 16; ++i)
		{
			const float w = kWeights[idx[i]] * (1.f / 64), a = 1 - w;
			aa += wt[i] * a * a;
			ab += wt[i] * a * w;
			bb += wt[i] * w * w;
			ap += (wt[i] * a) * px[i];
			bp += (wt[i] * w) * px[i];
		}
		const float det = aa * bb - ab * ab;
		if (std::fabs(det) < 1e-6f)
			break;
		e0 = simd_clamp((bb * ap - ab * bp) / det, 0.f, 255.f);
		e1 = simd_clamp((aa * bp - ab * ap) / det, 0.f, 255.f);
	}

	// Quantise the endpoints
	int q0[4], q1[4], p0 = 0, p1 = 0;
	quantise(e0, q0, p0);
	quantise(e1, q1, p1);
	int E0[4], E1[4];
	for (int c = 0; c < 4; ++c)
	{
		E0[c] = (q0[c] << 1) | p0;
		E1[c] = (q1[c] << 1) | p1;
	}

	// Final indices against the palette the decoder builds: the position
	// along the quantised segment and its neighbours
	{
		simd_float4 palette[16];
		for (int k = 0; k < 16; ++k)
			palette[k] = simd_make_float4(float(interpolate(E0[0], E1[0], kWeights[k])),
										  float(interpolate(E0[1], E1[1], kWeights[k])),
										  float(interpolate(E0[2], E1[2], kWeights[k])),
										  float(interpolate(E0[3], E1[3], kWeights[k])));
		const simd_float4 dir = palette[15] - palette[0];
		const float len2 = simd_length_squared(dir);
		const simd_float4 dirn = len2 > 0 ? dir / len2 : simd_float4(0);
		for (int i = 0; i < 16; ++i)
		{
			const int guess = len2 > 0 ? index_along(simd_reduce_add((px[i] - palette[0]) * dirn)) : 0;
			int best = guess;
			float best_err = 1e30f;
			for (int k = std::max(0, guess - 1); k <= std::min(15, guess + 1); ++k)
			{
				const float err = simd_length_squared(px[i] - palette[k]);
				if (err < best_err)
				{
					best_err = err;
					best = k;
				}
			}
			idx[i] = best;
		}
	}

	// The first index's top bit is implied zero: swap the endpoints if needed
	if (idx[0] >= 8)
	{
		std::swap(q0, q1);
		std::swap(p0, p1);
		for (int i = 0; i < 16; ++i)
			idx[i] = 15 - idx[i];
	}

	BitWriter w;
	w.put(0x40, 7);	// mode 6
	for (int c = 0; c < 4; ++c)
	{
		w.put(uint32_t(q0[c]), 7);
		w.put(uint32_t(q1[c]), 7);
	}
	w.put(uint32_t(p0), 1);
	w.put(uint32_t(p1), 1);
	w.put(uint32_t(idx[0]), 3);
	for (int i = 1; i < 16; ++i)
		w.put(uint32_t(idx[i]), 4);
	memcpy(out, w.b, 16);
}

bool decode_block(const uint8_t in[16], uint8_t out[16][4])
{
	BitReader r(in);
	if (r.get(7) != 0x40)
	{
		memset(out, 0, 64);
		return false;
	}
	int q0[4], q1[4];
	for (int c = 0; c < 4; ++c)
	{
		q0[c] = int(r.get(7));
		q1[c] = int(r.get(7));
	}
	const int p0 = int(r.get(1)), p1 = int(r.get(1));
	int idx[16];
	idx[0] = int(r.get(3));
	for (int i = 1; i < 16; ++i)
		idx[i] = int(r.get(4));
	for (int i = 0; i < 16; ++i)
		for (int c = 0; c < 4; ++c)
			out[i][c] = uint8_t(interpolate((q0[c] << 1) | p0, (q1[c] << 1) | p1, kWeights[idx[i]]));
	return true;
}

}  // namespace

size_t Bytes(int width, int height)
{
	return size_t((width + 3) / 4) * size_t((height + 3) / 4) * 16;
}

void Encode(const uint8_t* rgba, int width, int height, uint8_t* out)
{
	const int bw = (width + 3) / 4, bh = (height + 3) / 4;
	for (int by = 0; by < bh; ++by)
	{
		for (int bx = 0; bx < bw; ++bx)
		{
			simd_float4 px[16];
			for (int y = 0; y < 4; ++y)
			{
				const int sy = std::min(height - 1, by * 4 + y);	// partial blocks repeat the edge
				for (int x = 0; x < 4; ++x)
				{
					const int sx = std::min(width - 1, bx * 4 + x);
					const uint8_t* p = rgba + (size_t(sy) * width + sx) * 4;
					px[y * 4 + x] = simd_make_float4(float(p[0]), float(p[1]), float(p[2]), float(p[3]));
				}
			}
			encode_block(px, out + (size_t(by) * bw + bx) * 16);
		}
	}
}

bool Decode(const uint8_t* blocks, int width, int height, uint8_t* rgba)
{
	const int bw = (width + 3) / 4, bh = (height + 3) / 4;
	bool ok = true;
	for (int by = 0; by < bh; ++by)
	{
		for (int bx = 0; bx < bw; ++bx)
		{
			uint8_t px[16][4];
			ok &= decode_block(blocks + (size_t(by) * bw + bx) * 16, px);
			for (int y = 0; y < 4; ++y)
			{
				const int dy = by * 4 + y;
				if (dy >= height)
					break;
				for (int x = 0; x < 4; ++x)
				{
					const int dx = bx * 4 + x;
					if (dx >= width)
						break;
					memcpy(rgba + (size_t(dy) * width + dx) * 4, px[y * 4 + x], 4);
				}
			}
		}
	}
	return ok;
}

void Blacken(uint8_t* blocks, size_t bytes)
{
	// Mode 6: the colour endpoints are bits 7..48; the p-bits (63, 64)
	// stay, so the colour is 0 or 1 of 255
	for (size_t i = 0; i + 16 <= bytes; i += 16)
	{
		uint8_t* b = blocks + i;
		if ((b[0] & 0x7f) != 0x40)
			continue;
		b[0] &= 0x7f;
		memset(b + 1, 0, 5);
		b[6] &= 0xfe;
	}
}

void Halve(const uint8_t* rgba, int width, int height, uint8_t* out)
{
	const int nw = std::max(1, width >> 1), nh = std::max(1, height >> 1);
	for (int y = 0; y < nh; ++y)
	{
		const int y0 = std::min(height - 1, y * 2), y1 = std::min(height - 1, y * 2 + 1);
		for (int x = 0; x < nw; ++x)
		{
			const int x0 = std::min(width - 1, x * 2), x1 = std::min(width - 1, x * 2 + 1);
			const uint8_t* a = rgba + (size_t(y0) * width + x0) * 4;
			const uint8_t* b = rgba + (size_t(y0) * width + x1) * 4;
			const uint8_t* c = rgba + (size_t(y1) * width + x0) * 4;
			const uint8_t* d = rgba + (size_t(y1) * width + x1) * 4;
			uint8_t* o = out + (size_t(y) * nw + x) * 4;
			for (int k = 0; k < 4; ++k)
				o[k] = uint8_t((a[k] + b[k] + c[k] + d[k] + 2) >> 2);
		}
	}
}

std::vector<uint8_t> EncodeMipChain(const uint8_t* rgba, int width, int height, int& mip_count, bool& opaque)
{
	opaque = true;
	for (size_t i = 0, n = size_t(width) * height; i < n && opaque; ++i)
		opaque = rgba[i * 4 + 3] == 255;

	mip_count = 1;
	while ((std::max(width, height) >> mip_count) >= 1)
		++mip_count;

	std::vector<uint8_t> out;
	std::vector<uint8_t> level(rgba, rgba + size_t(width) * height * 4), next;
	int w = width, h = height;
	for (int m = 0; m < mip_count; ++m)
	{
		const size_t at = out.size();
		out.resize(at + Bytes(w, h));
		Encode(level.data(), w, h, out.data() + at);
		if (m + 1 < mip_count)
		{
			const int nw = std::max(1, w >> 1), nh = std::max(1, h >> 1);
			next.resize(size_t(nw) * nh * 4);
			Halve(level.data(), w, h, next.data());
			level.swap(next);
			w = nw;
			h = nh;
		}
	}
	return out;
}

}
