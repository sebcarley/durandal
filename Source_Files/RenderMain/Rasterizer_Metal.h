/*
	Rasterizer_Metal.h — Durandal project

	The Metal counterpart of Rasterizer_Shader_Class: sets up the view
	(the same projection and view matrices the shader renderer loads into
	OpenGL, composed on the CPU) and brackets the Metal world pass. It
	derives from the OpenGL rasteriser so OGL_StartMain/OGL_EndMain still
	run: they set up fog and draw the fades over the finished world.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef RASTERIZER_METAL_H
#define RASTERIZER_METAL_H

#include "cseries.h"
#include "map.h"
#include "Rasterizer_OGL.h"
#include <simd/simd.h>

#ifdef HAVE_OPENGL

class Rasterizer_Metal_Class : public Rasterizer_OGL_Class {
	friend class RenderRasterize_Metal;

protected:
	short view_width = 0;
	short view_height = 0;
	bool smear_the_void = false;
	bool began = false;

	// OpenGL conventions: world -> eye, and eye -> clip
	simd_float4x4 projection;
	simd_float4x4 modelview;

public:
	virtual void SetView(view_data& View);
	virtual void setupGL();
	virtual void Begin();
	virtual void End();
};

// Column-major 4x4 helpers with OpenGL's matrix-call semantics
namespace DurandalMatrix {
simd_float4x4 identity();
simd_float4x4 rotate(simd_float4x4 m, double degrees, double x, double y, double z);
simd_float4x4 translate(simd_float4x4 m, double x, double y, double z);
simd_float4x4 frustum(double left, double right, double bottom, double top, double near_value, double far_value);
simd_float4x4 from_gl(const double m[16]);
}

#endif
#endif
