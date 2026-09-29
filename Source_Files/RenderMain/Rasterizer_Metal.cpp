/*
	Rasterizer_Metal.cpp — Durandal project

	See Rasterizer_Metal.h. SetView() mirrors Rasterizer_Shader_Class::SetView().

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#include "OGL_Headers.h"

#include "Rasterizer_Metal.h"
#include "DurandalMetal.h"

#include "OGL_Render.h"
#include "OGL_Setup.h"
#include "fades.h"
#include "preferences.h"
#include "screen.h"
#include "ViewControl.h"

#include <cmath>

#include "DurandalGLShim.h"	// the frame border and overlays are GL calls
#include "DurandalGL.h"

#ifdef HAVE_OPENGL

namespace DurandalMatrix {

simd_float4x4 identity()
{
	return matrix_identity_float4x4;
}

simd_float4x4 rotate(simd_float4x4 m, double degrees, double x, double y, double z)
{
	const double len = std::sqrt(x * x + y * y + z * z);
	if (len == 0)
		return m;
	x /= len; y /= len; z /= len;
	const double a = degrees * M_PI / 180.0;
	const double c = std::cos(a), s = std::sin(a), t = 1 - c;
	// glRotate's matrix, column by column
	const simd_float4x4 r = simd_matrix(
		simd_make_float4(x * x * t + c, y * x * t + z * s, x * z * t - y * s, 0),
		simd_make_float4(x * y * t - z * s, y * y * t + c, y * z * t + x * s, 0),
		simd_make_float4(x * z * t + y * s, y * z * t - x * s, z * z * t + c, 0),
		simd_make_float4(0, 0, 0, 1));
	return simd_mul(m, r);
}

simd_float4x4 translate(simd_float4x4 m, double x, double y, double z)
{
	simd_float4x4 t = matrix_identity_float4x4;
	t.columns[3] = simd_make_float4(x, y, z, 1);
	return simd_mul(m, t);
}

simd_float4x4 frustum(double l, double r, double b, double t, double n, double f)
{
	return simd_matrix(
		simd_make_float4(2 * n / (r - l), 0, 0, 0),
		simd_make_float4(0, 2 * n / (t - b), 0, 0),
		simd_make_float4((r + l) / (r - l), (t + b) / (t - b), -(f + n) / (f - n), -1),
		simd_make_float4(0, 0, -2 * f * n / (f - n), 0));
}

simd_float4x4 from_gl(const double m[16])
{
	return simd_matrix(
		simd_make_float4(m[0], m[1], m[2], m[3]),
		simd_make_float4(m[4], m[5], m[6], m[7]),
		simd_make_float4(m[8], m[9], m[10], m[11]),
		simd_make_float4(m[12], m[13], m[14], m[15]));
}

}

using namespace DurandalMatrix;

static const double kViewBaseMatrix[16] = {
	0,	0,	-1,	0,
	1,	0,	0,	0,
	0,	1,	0,	0,
	0,	0,	0,	1
};

static const float FixedAngleToDegrees = 360.0 / (float(FIXED_ONE) * float(FULL_CIRCLE));

void Rasterizer_Metal_Class::setupGL()
{
	view_width = 0;
	view_height = 0;
	smear_the_void = !TEST_FLAG(Get_OGL_ConfigureData().Flags, OGL_Flag_VoidColor);
}

void Rasterizer_Metal_Class::SetView(view_data& view)
{
	OGL_SetView(view);

	view_width = view.screen_width;
	view_height = view.screen_height;

	const float aspect = view.screen_width / float(view.screen_height);
	const float deg2rad = 8.0 * atan(1.0) / 360.0;
	float xtan, ytan;
	if (View_FOV_FixHorizontalNotVertical()) {
		xtan = tan(view.field_of_view * deg2rad / 2.0);
		ytan = xtan / aspect;
	} else {
		ytan = tan(view.field_of_view * deg2rad / 2.0) / 2.0;
		xtan = ytan * aspect;
	}

	// Adjust for view distortion during teleport effect
	ytan *= view.real_world_to_screen_y / double(view.world_to_screen_y);
	xtan *= view.real_world_to_screen_x / double(view.world_to_screen_x);

	const double yaw = view.virtual_yaw * FixedAngleToDegrees;
	double pitch = view.virtual_pitch * FixedAngleToDegrees;
	pitch = (pitch > 180.0 ? pitch - 360.0 : pitch);

	const float near_value = 64.0;
	const float far_value = 128.0 * 1024.0;
	const float x = xtan * near_value;
	const float y = ytan * near_value;
	const float yoff = view.mimic_sw_perspective ? tan(pitch * deg2rad) * near_value : 0;
	projection = frustum(-x, x, -y + yoff, y + yoff, near_value, far_value);

	simd_float4x4 m = from_gl(kViewBaseMatrix);
	if (!view.mimic_sw_perspective) {
		// Durandal (True Look): the camera rotates instead of shearing the
		// screen. Roll (Sidestep Sway) about the view axis, applied after
		// the pitch; positive rolls the view clockwise, as Quake
		if (view.durandal_roll != 0)
			m = rotate(m, view.durandal_roll, 1.0, 0.0, 0.0);
		m = rotate(m, pitch, 0.0, 1.0, 0.0);
	}
	m = rotate(m, -yaw, 0.0, 0.0, 1.0);
	m = translate(m, -view.origin.x, -view.origin.y, -view.origin.z);
	modelview = m;

	// Leave the same matrices in the GL state as Rasterizer_Shader_Class
	// does: the overlays drawn after the world (e.g. the frame border in
	// End()) depend on them.
	GLdouble p[16], v[16];
	for (int c = 0; c < 4; ++c)
		for (int r = 0; r < 4; ++r) {
			p[c * 4 + r] = projection.columns[c][r];
			v[c * 4 + r] = modelview.columns[c][r];
		}
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixd(p);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixd(v);
}

void Rasterizer_Metal_Class::Begin()
{
	// OGL_StartMain: fog set-up and GL state for the overlays drawn later
	Rasterizer_OGL_Class::Begin();

	simd_float4 clear = simd_make_float4(0, 0, 0, 0);
	OGL_ConfigureData& config = Get_OGL_ConfigureData();
	if (TEST_FLAG(config.Flags, OGL_Flag_VoidColor))
	{
		const RGBColor& c = config.VoidColor;
		clear = simd_make_float4(c.red / 65535.f, c.green / 65535.f, c.blue / 65535.f, 0);
		if (OGL_GetCurrFogData())
		{
			const GLfloat* fog = OGL_GetCurrFogColor();
			clear = simd_make_float4(fog[0], fog[1], fog[2], 0);
		}
	}

	const float scale = MainScreenPixelScale();
	began = DurandalMetal::BeginWorld(int(view_width * scale), int(view_height * scale), smear_the_void, clear);
}

void Rasterizer_Metal_Class::End()
{
	if (began)
	{
		DurandalMetal::EndWorld(get_actual_gamma_adjust(graphics_preferences->screen_mode.gamma_level));
		began = false;
	}

	// As the shader rasteriser: frame border, then OGL_EndMain (fades)
	SetForeground();
	glColor3f(0, 0, 0);
	OGL_RenderFrame(0, 0, view_width, view_height, 1);

	Rasterizer_OGL_Class::End();
}

#endif
