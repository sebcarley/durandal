/*
	DurandalGLShim.h — Durandal project

	Include after the OpenGL headers in a file whose drawing should also work
	in Metal display mode. It routes that file's gl* calls to DurandalGL's
	dgl_* functions, which pass through to real OpenGL unless Metal display
	mode is active (see DurandalGL.h). Deliberately no include guard
	interplay with OpenGL: it only defines macros.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.
*/

#ifndef DURANDAL_GL_SHIM_H
#define DURANDAL_GL_SHIM_H

#include "DurandalGL.h"

#define glEnable dgl_glEnable
#define glDisable dgl_glDisable
#define glEnableClientState dgl_glEnableClientState
#define glDisableClientState dgl_glDisableClientState
#define glVertexPointer dgl_glVertexPointer
#define glTexCoordPointer dgl_glTexCoordPointer
#define glColorPointer dgl_glColorPointer
#define glDrawArrays dgl_glDrawArrays
#define glDrawElements dgl_glDrawElements
#define glMatrixMode dgl_glMatrixMode
#define glLoadIdentity dgl_glLoadIdentity
#define glLoadMatrixd dgl_glLoadMatrixd
#define glMultMatrixd dgl_glMultMatrixd
#define glPushMatrix dgl_glPushMatrix
#define glPopMatrix dgl_glPopMatrix
#define glTranslatef dgl_glTranslatef
#define glTranslated dgl_glTranslated
#define glScalef dgl_glScalef
#define glScaled dgl_glScaled
#define glRotatef dgl_glRotatef
#define glRotated dgl_glRotated
#define glOrtho dgl_glOrtho
#define glColor3f dgl_glColor3f
#define glColor4f dgl_glColor4f
#define glColor3fv dgl_glColor3fv
#define glColor4fv dgl_glColor4fv
#define glColor3ub dgl_glColor3ub
#define glColor4ub dgl_glColor4ub
#define glColor3us dgl_glColor3us
#define glColor4us dgl_glColor4us
#define glColor3usv dgl_glColor3usv
#define glColor4usv dgl_glColor4usv
#define glBlendFunc dgl_glBlendFunc
#define glAlphaFunc dgl_glAlphaFunc
#define glLogicOp dgl_glLogicOp
#define glColorMask dgl_glColorMask
#define glCullFace dgl_glCullFace
#define glFrontFace dgl_glFrontFace
#define glDepthFunc dgl_glDepthFunc
#define glDepthRange dgl_glDepthRange
#define glFogf dgl_glFogf
#define glFogfv dgl_glFogfv
#define glClipPlane dgl_glClipPlane
#define glStencilFunc dgl_glStencilFunc
#define glStencilOp dgl_glStencilOp
#define glStencilMask dgl_glStencilMask
#define glClearStencil dgl_glClearStencil
#define glClearColor dgl_glClearColor
#define glClear dgl_glClear
#define glViewport dgl_glViewport
#define glScissor dgl_glScissor
#define glPushAttrib dgl_glPushAttrib
#define glPopAttrib dgl_glPopAttrib
#define glPolygonStipple dgl_glPolygonStipple
#define glGenTextures dgl_glGenTextures
#define glDeleteTextures dgl_glDeleteTextures
#define glBindTexture dgl_glBindTexture
#define glTexParameteri dgl_glTexParameteri
#define glTexParameterf dgl_glTexParameterf
#define glTexEnvi dgl_glTexEnvi
#define glTexImage2D dgl_glTexImage2D
#define glCompressedTexImage2DARB dgl_glCompressedTexImage2DARB
#define gluBuild2DMipmaps dgl_gluBuild2DMipmaps
#define gluScaleImage dgl_gluScaleImage
#define glPixelStorei dgl_glPixelStorei
#define glReadPixels dgl_glReadPixels
#define glGenLists dgl_glGenLists
#define glDeleteLists dgl_glDeleteLists
#define glNewList dgl_glNewList
#define glEndList dgl_glEndList
#define glCallList dgl_glCallList
#define glGetString dgl_glGetString
#define glGetIntegerv dgl_glGetIntegerv
#define glGetFloatv dgl_glGetFloatv
#define glGetDoublev dgl_glGetDoublev
#define glBlitFramebufferEXT dgl_glBlitFramebufferEXT

#endif
