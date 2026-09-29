#ifndef DURANDAL_BC7_H
#define DURANDAL_BC7_H
/*
	Durandal: BC7 block compression for the texture cache.

	Only mode 6 is written (one subset, 7-bit RGBA endpoints with a p-bit
	each, 4-bit indices, 16 bytes per 4x4 block): the mode every fast
	encoder starts from, with 16 levels between two RGBA endpoints per
	block, so smooth HD art keeps its gradients (BC1 and BC3 have four).
	The decoder reads only what the encoder writes; other modes decode as
	black. Straight (unpremultiplied) alpha throughout; all four channels
	weigh the same, so the colour of transparent pixels survives as the
	filtering needs it to.
*/

#include <cstddef>
#include <cstdint>
#include <vector>

namespace DurandalBC7 {

// Bytes of BC7 for an image (4x4 blocks, partial blocks padded)
size_t Bytes(int width, int height);

// Encodes RGBA8 (row-major, 4 bytes per pixel) into BC7 blocks, block rows
// top to bottom; out must hold Bytes(width, height)
void Encode(const uint8_t* rgba, int width, int height, uint8_t* out);

// Decodes blocks written by Encode into RGBA8; rgba must hold width * height * 4
bool Decode(const uint8_t* blocks, int width, int height, uint8_t* rgba);

// Zeroes the colour endpoints of every block, keeping the alpha: the
// silhouette version of a sprite (OGL_Textures' FindSilhouetteVersion)
void Blacken(uint8_t* blocks, size_t bytes);

// Box-filters RGBA8 to half size (dimensions never below 1)
void Halve(const uint8_t* rgba, int width, int height, uint8_t* out);

// Every mip level of an RGBA8 image, box-filtered down to 1x1 and encoded,
// concatenated level by level. mip_count receives the number of levels and
// opaque whether every level-0 pixel has full alpha
std::vector<uint8_t> EncodeMipChain(const uint8_t* rgba, int width, int height, int& mip_count, bool& opaque);

}

#endif
