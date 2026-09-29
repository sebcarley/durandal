#ifndef DURANDAL_GL_H
#define DURANDAL_GL_H
/*
	DurandalGL.h — Durandal project

	Metal display mode (roadmap F4/F5, milestone 2b). When the Metal renderer
	is on, the window has no OpenGL context at all: it is a Metal window with
	a CAMetalLayer, the world is drawn natively by DurandalMetal, and the
	engine's 2D code (HUD, Lua HUD, automap, fonts, terminals, menus,
	dialogs, fades, load screens, screenshots) runs unchanged on top of this
	small OpenGL 1.x subset implemented over Metal.

	Files that draw 2D include DurandalGLShim.h, which routes their gl*
	calls to the dgl_* functions below. Each one passes straight through to
	real OpenGL unless Metal display mode is active, so the OpenGL renderer
	is untouched.

	The subset is exactly what those files use: matrix stacks, client
	vertex/texcoord/colour arrays with glDrawArrays/glDrawElements (polygons,
	triangles, strips), 2D textures (RGBA, luminance-alpha, S3TC), blending,
	alpha test, scissor, stencil, user clip planes, face culling, the XOR
	logic op, display lists, attribute stacks and read-back.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl.h>
#include <OpenGL/glext.h>
#include <OpenGL/glu.h>

#include <cstdint>
#include <vector>

struct SDL_Window;

namespace DurandalGL {

// Whether this session should use Metal display mode: the "Metal Renderer"
// setting (or DURANDAL_METAL_DISPLAY=1/0), decided once at first window
// creation and fixed for the session.
bool Wanted();
// After a failed Metal display creation: use OpenGL for the rest of the session.
void Disable();

// True once the Metal display has been created for the window.
bool Active();

// Window life cycle (screen.cpp)
bool CreateDisplay(SDL_Window* window, bool vsync);
void DestroyDisplay();
void SetVSync(bool vsync);
void DrawableSize(int& width, int& height);
void Present();		// replaces SDL_GL_SwapWindow

// Pacing: take the next drawable now, at the top of a game frame, before
// input is read. Blocks while the display holds them all (vsync).
void WaitForFrame();

// HDR: the display's current EDR headroom (1.0 = SDR only).
float EDRHeadroom();

// Benchmark: collect each drawable's actual presentation time (seconds).
void SetCollectPresentTimes(bool on);
// Drawables in the swap chain (0 when the Metal display is off)
int DrawableCount();
// The refresh rate (Hz) of the display the window is on, 0 if unknown
float DisplayRefreshRate();
void TakePresentTimes(std::vector<double>& out);

// World compositing (DurandalMetal.mm). The world renders into its own
// texture inside the current frame's command buffer, then is drawn into
// the current viewport.
void* FrameCommandBuffer();		// id<MTLCommandBuffer>, beginning the frame if needed
void EndScreenPass();			// ends the current screen encoder, if any
// glow: this frame's glow image to show above SDR white, and bloom: its
// bloom (id<MTLTexture>s, E1), or null
// Ambient shadows (Round 12): `ao` (half resolution) and `distance` (the
// world's distance image) darken the world image in the blit, faded out by
// `ao_far`; null for none. ao_view shows the occlusion instead (development).
// distance_shade (Round 12, 0..1) darkens far surfaces from the distance
// image, so long halls recede.
void DrawWorldImage(void* texture, float gamma, void* glow, void* bloom,
					void* ao = nullptr, void* distance = nullptr, float ao_far = 0, bool ao_view = false,
					float distance_shade = 0);	// id<MTLTexture>
// Liquids (W1): waver the next world image (0: none, 1: underwater)
void SetWorldDistortion(float amount, float time_seconds);
// Glow (E1): whether the output can show glow (HDR output is on)
bool GlowWanted();
void FlushAndWait();			// commit, wait, and continue the frame in a new command buffer

// Screen pixels per unit of the current modelview/projection/viewport
// (geometric mean of x and y); 0 while a display list is being compiled.
float PixelsPerUnit();

// Development: capture the next presented output (after the output pass,
// so glow and bloom are included) as RGBA8 rows top-down; EDR values
// above 1 are clipped.
void RequestOutputCapture();
bool TakeOutputCapture(std::vector<uint8_t>& rgba, int& width, int& height);

// Reads the frame being built (OpenGL: the back buffer), RGBA, rows bottom-up.
void ReadFramebuffer(int width, int height, uint8_t* rgba);

}

// ---- The OpenGL subset (signatures as OpenGL's) ----

void dgl_glEnable(GLenum cap);
void dgl_glDisable(GLenum cap);
void dgl_glEnableClientState(GLenum array);
void dgl_glDisableClientState(GLenum array);
void dgl_glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void dgl_glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void dgl_glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void dgl_glDrawArrays(GLenum mode, GLint first, GLsizei count);
void dgl_glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid* indices);

void dgl_glMatrixMode(GLenum mode);
void dgl_glLoadIdentity(void);
void dgl_glLoadMatrixd(const GLdouble* m);
void dgl_glMultMatrixd(const GLdouble* m);
void dgl_glPushMatrix(void);
void dgl_glPopMatrix(void);
void dgl_glTranslatef(GLfloat x, GLfloat y, GLfloat z);
void dgl_glTranslated(GLdouble x, GLdouble y, GLdouble z);
void dgl_glScalef(GLfloat x, GLfloat y, GLfloat z);
void dgl_glScaled(GLdouble x, GLdouble y, GLdouble z);
void dgl_glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void dgl_glRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void dgl_glOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

void dgl_glColor3f(GLfloat r, GLfloat g, GLfloat b);
void dgl_glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void dgl_glColor3fv(const GLfloat* v);
void dgl_glColor4fv(const GLfloat* v);
void dgl_glColor3ub(GLubyte r, GLubyte g, GLubyte b);
void dgl_glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void dgl_glColor3us(GLushort r, GLushort g, GLushort b);
void dgl_glColor4us(GLushort r, GLushort g, GLushort b, GLushort a);
void dgl_glColor3usv(const GLushort* v);
void dgl_glColor4usv(const GLushort* v);

void dgl_glBlendFunc(GLenum sfactor, GLenum dfactor);
void dgl_glAlphaFunc(GLenum func, GLclampf ref);
void dgl_glLogicOp(GLenum opcode);
void dgl_glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a);
void dgl_glCullFace(GLenum mode);
void dgl_glFrontFace(GLenum mode);
void dgl_glDepthFunc(GLenum func);
void dgl_glDepthRange(GLclampd n, GLclampd f);
void dgl_glFogf(GLenum pname, GLfloat param);
void dgl_glFogfv(GLenum pname, const GLfloat* params);
void dgl_glClipPlane(GLenum plane, const GLdouble* equation);
void dgl_glStencilFunc(GLenum func, GLint ref, GLuint mask);
void dgl_glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
void dgl_glStencilMask(GLuint mask);
void dgl_glClearStencil(GLint s);
void dgl_glClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a);
void dgl_glClear(GLbitfield mask);
void dgl_glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
void dgl_glScissor(GLint x, GLint y, GLsizei w, GLsizei h);
void dgl_glPushAttrib(GLbitfield mask);
void dgl_glPopAttrib(void);
void dgl_glPolygonStipple(const GLubyte* mask);

void dgl_glGenTextures(GLsizei n, GLuint* textures);
void dgl_glDeleteTextures(GLsizei n, const GLuint* textures);
void dgl_glBindTexture(GLenum target, GLuint texture);
void dgl_glTexParameteri(GLenum target, GLenum pname, GLint param);
void dgl_glTexParameterf(GLenum target, GLenum pname, GLfloat param);
void dgl_glTexEnvi(GLenum target, GLenum pname, GLint param);
void dgl_glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
					  GLint border, GLenum format, GLenum type, const GLvoid* pixels);
void dgl_glCompressedTexImage2DARB(GLenum target, GLint level, GLenum internalformat, GLsizei width,
								   GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data);
GLint dgl_gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height,
							GLenum format, GLenum type, const void* data);
GLint dgl_gluScaleImage(GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn, const void* dataIn,
						GLsizei wOut, GLsizei hOut, GLenum typeOut, GLvoid* dataOut);
void dgl_glPixelStorei(GLenum pname, GLint param);
void dgl_glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid* pixels);

GLuint dgl_glGenLists(GLsizei range);
void dgl_glDeleteLists(GLuint list, GLsizei range);
void dgl_glNewList(GLuint list, GLenum mode);
void dgl_glEndList(void);
void dgl_glCallList(GLuint list);

const GLubyte* dgl_glGetString(GLenum name);
void dgl_glGetIntegerv(GLenum pname, GLint* params);
void dgl_glGetFloatv(GLenum pname, GLfloat* params);
void dgl_glGetDoublev(GLenum pname, GLdouble* params);
void dgl_glBlitFramebufferEXT(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0,
							  GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);

#endif
