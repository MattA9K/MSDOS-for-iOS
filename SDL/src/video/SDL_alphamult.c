/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/
#include "SDL_config.h"

#include "SDL_blit.h"
#include "SDL_alphamult.h"

/* Functions to pre-multiply the alpha channel into the color channels */

#define DEFINE_PREMULTIPLY_FUNC(fmt) \
void \
SDL_PreMultiplyAlpha##fmt(int w, int h, Uint32 *pixels, int pitch) \
{ \
    pitch /= 4; \
    while (h--) { \
        int n; \
        Uint32 *row = pixels; \
        Uint32 pixel; \
        unsigned r, g, b, a; \
 \
        for (n = w; n--; ) { \
            pixel = *row; \
            RGBA_FROM_##fmt(pixel, r, g, b, a); \
            r = (r * a) / 255; \
            g = (g * a) / 255; \
            b = (b * a) / 255; \
            fmt##_FROM_RGBA(*row, r, g, b, a); \
            ++row; \
        } \
        pixels += pitch; \
    } \
}

/* *INDENT-OFF* */
DEFINE_PREMULTIPLY_FUNC(ARGB8888)
DEFINE_PREMULTIPLY_FUNC(RGBA8888)
DEFINE_PREMULTIPLY_FUNC(ABGR8888)
DEFINE_PREMULTIPLY_FUNC(BGRA8888)
/* *INDENT-ON* */

/* vi: set ts=4 sw=4 expandtab: */
