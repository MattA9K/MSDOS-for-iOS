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

/* General (mostly internal) pixel/color manipulation routines for SDL */

#include "SDL_endian.h"
#include "SDL_video.h"
#include "SDL_sysvideo.h"
#include "SDL_blit.h"
#include "SDL_pixels_c.h"
#include "SDL_RLEaccel_c.h"

struct SDL_PaletteWatch
{
    SDL_PaletteChangedFunc callback;
    void *userdata;
    struct SDL_PaletteWatch *next;
};

/* Helper functions */

SDL_bool
SDL_PixelFormatEnumToMasks(Uint32 format, int *bpp, Uint32 * Rmask,
                           Uint32 * Gmask, Uint32 * Bmask, Uint32 * Amask)
{
    Uint32 masks[4];

    /* Initialize the values here */
    if (SDL_BYTESPERPIXEL(format) <= 2) {
        *bpp = SDL_BITSPERPIXEL(format);
    } else {
        *bpp = SDL_BYTESPERPIXEL(format) * 8;
    }
    *Rmask = *Gmask = *Bmask = *Amask = 0;

    if (format == SDL_PIXELFORMAT_RGB24) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        *Rmask = 0x00FF0000;
        *Gmask = 0x0000FF00;
        *Bmask = 0x000000FF;
#else
        *Rmask = 0x000000FF;
        *Gmask = 0x0000FF00;
        *Bmask = 0x00FF0000;
#endif
        return SDL_TRUE;
    }

    if (format == SDL_PIXELFORMAT_BGR24) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        *Rmask = 0x000000FF;
        *Gmask = 0x0000FF00;
        *Bmask = 0x00FF0000;
#else
        *Rmask = 0x00FF0000;
        *Gmask = 0x0000FF00;
        *Bmask = 0x000000FF;
#endif
        return SDL_TRUE;
    }

    if (SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED8 &&
        SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED16 &&
        SDL_PIXELTYPE(format) != SDL_PIXELTYPE_PACKED32) {
        /* Not a format that uses masks */
        return SDL_TRUE;
    }

    switch (SDL_PIXELLAYOUT(format)) {
    case SDL_PACKEDLAYOUT_332:
        masks[0] = 0x00000000;
        masks[1] = 0x000000E0;
        masks[2] = 0x0000001C;
        masks[3] = 0x00000003;
        break;
    case SDL_PACKEDLAYOUT_4444:
        masks[0] = 0x0000F000;
        masks[1] = 0x00000F00;
        masks[2] = 0x000000F0;
        masks[3] = 0x0000000F;
        break;
    case SDL_PACKEDLAYOUT_1555:
        masks[0] = 0x00008000;
        masks[1] = 0x00007C00;
        masks[2] = 0x000003E0;
        masks[3] = 0x0000001F;
        break;
    case SDL_PACKEDLAYOUT_5551:
        masks[0] = 0x0000F800;
        masks[1] = 0x000007C0;
        masks[2] = 0x0000003E;
        masks[3] = 0x00000001;
        break;
    case SDL_PACKEDLAYOUT_565:
        masks[0] = 0x00000000;
        masks[1] = 0x0000F800;
        masks[2] = 0x000007E0;
        masks[3] = 0x0000001F;
        break;
    case SDL_PACKEDLAYOUT_8888:
        masks[0] = 0xFF000000;
        masks[1] = 0x00FF0000;
        masks[2] = 0x0000FF00;
        masks[3] = 0x000000FF;
        break;
    case SDL_PACKEDLAYOUT_2101010:
        masks[0] = 0xC0000000;
        masks[1] = 0x3FF00000;
        masks[2] = 0x000FFC00;
        masks[3] = 0x000003FF;
        break;
    case SDL_PACKEDLAYOUT_1010102:
        masks[0] = 0xFFC00000;
        masks[1] = 0x003FF000;
        masks[2] = 0x00000FFC;
        masks[3] = 0x00000003;
        break;
    default:
        SDL_SetError("Unknown pixel format");
        return SDL_FALSE;
    }

    switch (SDL_PIXELORDER(format)) {
    case SDL_PACKEDORDER_XRGB:
        *Rmask = masks[1];
        *Gmask = masks[2];
        *Bmask = masks[3];
        break;
    case SDL_PACKEDORDER_RGBX:
        *Rmask = masks[0];
        *Gmask = masks[1];
        *Bmask = masks[2];
        break;
    case SDL_PACKEDORDER_ARGB:
        *Amask = masks[0];
        *Rmask = masks[1];
        *Gmask = masks[2];
        *Bmask = masks[3];
        break;
    case SDL_PACKEDORDER_RGBA:
        *Rmask = masks[0];
        *Gmask = masks[1];
        *Bmask = masks[2];
        *Amask = masks[3];
        break;
    case SDL_PACKEDORDER_XBGR:
        *Bmask = masks[1];
        *Gmask = masks[2];
        *Rmask = masks[3];
        break;
    case SDL_PACKEDORDER_BGRX:
        *Bmask = masks[0];
        *Gmask = masks[1];
        *Rmask = masks[2];
        break;
    case SDL_PACKEDORDER_BGRA:
        *Bmask = masks[0];
        *Gmask = masks[1];
        *Rmask = masks[2];
        *Amask = masks[3];
        break;
    case SDL_PACKEDORDER_ABGR:
        *Amask = masks[0];
        *Bmask = masks[1];
        *Gmask = masks[2];
        *Rmask = masks[3];
        break;
    default:
        SDL_SetError("Unknown pixel format");
        return SDL_FALSE;
    }
    return SDL_TRUE;
}

Uint32
SDL_MasksToPixelFormatEnum(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask,
                           Uint32 Amask)
{
    switch (bpp) {
    case 8:
        switch (Rmask) {
        case 0:
            return SDL_PIXELFORMAT_INDEX8;
        case 0xE0:
            return SDL_PIXELFORMAT_RGB332;
        }
        break;
    case 12:
        switch (Rmask) {
        case 0x0F00:
            return SDL_PIXELFORMAT_RGB444;
        }
        break;
    case 15:
        switch (Rmask) {
        case 0x001F:
            return SDL_PIXELFORMAT_BGR555;
        case 0x7C00:
            return SDL_PIXELFORMAT_RGB555;
        }
        break;
    case 16:
        switch (Rmask) {
        case 0x000F:
            return SDL_PIXELFORMAT_ABGR4444;
        case 0x001F:
            if (Gmask == 0x07E0) {
                return SDL_PIXELFORMAT_BGR565;
            }
            return SDL_PIXELFORMAT_ABGR1555;
        case 0x0F00:
            return SDL_PIXELFORMAT_ARGB4444;
        case 0x7C00:
            return SDL_PIXELFORMAT_ARGB1555;
        case 0xF800:
            return SDL_PIXELFORMAT_RGB565;
        }
        break;
    case 24:
        switch (Rmask) {
        case 0x00FF0000:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            return SDL_PIXELFORMAT_RGB24;
#else
            return SDL_PIXELFORMAT_BGR24;
#endif
        case 0x000000FF:
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
            return SDL_PIXELFORMAT_BGR24;
#else
            return SDL_PIXELFORMAT_RGB24;
#endif
        case 0x00000000:
            /* FIXME: At this point we can't distinguish */
            /* if this format is RGB24 or BGR24          */
            return SDL_PIXELFORMAT_RGB24;
        }
    case 32:
        switch (Rmask) {
        case 0xFF000000:
            if (Amask == 0x000000FF) {
                return SDL_PIXELFORMAT_RGBA8888;
            }
            break;
        case 0x00FF0000:
            if (Amask == 0xFF000000) {
                return SDL_PIXELFORMAT_ARGB8888;
            } else {
                return SDL_PIXELFORMAT_RGB888;
            }
            break;
        case 0x0000FF00:
            if (Amask == 0x000000FF) {
                return SDL_PIXELFORMAT_BGRA8888;
            }
            break;
        case 0x000000FF:
            if (Amask == 0xFF000000) {
                return SDL_PIXELFORMAT_ABGR8888;
            } else {
                return SDL_PIXELFORMAT_BGR888;
            }
            break;
        case 0x3FF00000:
            return SDL_PIXELFORMAT_ARGB2101010;
        }
    }
    return SDL_PIXELFORMAT_UNKNOWN;
}


SDL_Palette *
SDL_AllocPalette(int ncolors)
{
    SDL_Palette *palette;

    palette = (SDL_Palette *) SDL_malloc(sizeof(*palette));
    if (!palette) {
        SDL_OutOfMemory();
        return NULL;
    }
    palette->colors =
        (SDL_Color *) SDL_malloc(ncolors * sizeof(*palette->colors));
    if (!palette->colors) {
        SDL_free(palette);
        return NULL;
    }
    palette->ncolors = ncolors;
    palette->watch = NULL;
    palette->refcount = 1;

    SDL_memset(palette->colors, 0xFF, ncolors * sizeof(*palette->colors));

    return palette;
}

int
SDL_AddPaletteWatch(SDL_Palette * palette, SDL_PaletteChangedFunc callback,
                    void *userdata)
{
    SDL_PaletteWatch *watch;

    if (!palette) {
        return -1;
    }

    watch = (SDL_PaletteWatch *) SDL_malloc(sizeof(*watch));
    if (!watch) {
        SDL_OutOfMemory();
        return -1;
    }

    watch->callback = callback;
    watch->userdata = userdata;
    watch->next = palette->watch;
    palette->watch = watch;
    ++palette->refcount;
    return 0;
}

void
SDL_DelPaletteWatch(SDL_Palette * palette, SDL_PaletteChangedFunc callback,
                    void *userdata)
{
    SDL_PaletteWatch *prev, *watch;

    if (!palette) {
        return;
    }

    for (prev = NULL, watch = palette->watch; watch;
         prev = watch, watch = watch->next) {
        if (watch->callback == callback && watch->userdata == userdata) {
            if (prev) {
                prev->next = watch->next;
            } else {
                palette->watch = watch->next;
            }
            SDL_free(watch);
            SDL_FreePalette(palette);
            return;
        }
    }
}

int
SDL_SetPaletteColors(SDL_Palette * palette, const SDL_Color * colors,
                     int firstcolor, int ncolors)
{
    SDL_PaletteWatch *watch;
    int status = 0;

    /* Verify the parameters */
    if (!palette) {
        return -1;
    }
    if (ncolors > (palette->ncolors - firstcolor)) {
        ncolors = (palette->ncolors - firstcolor);
        status = -1;
    }

    if (colors != (palette->colors + firstcolor)) {
        SDL_memcpy(palette->colors + firstcolor, colors,
                   ncolors * sizeof(*colors));
    }

    for (watch = palette->watch; watch; watch = watch->next) {
        if (watch->callback(watch->userdata, palette) < 0) {
            status = -1;
        }
    }

    return status;
}

void
SDL_FreePalette(SDL_Palette * palette)
{
    if (!palette) {
        return;
    }
    if (--palette->refcount > 0) {
        return;
    }
    if (palette->colors) {
        SDL_free(palette->colors);
    }
    SDL_free(palette);
}

/*
 * Allocate a pixel format structure and fill it according to the given info.
 */
SDL_PixelFormat *
SDL_AllocFormat(int bpp,
                Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask)
{
    SDL_PixelFormat *format;

    /* Allocate an empty pixel format structure */
    format = SDL_malloc(sizeof(*format));
    if (format == NULL) {
        SDL_OutOfMemory();
        return (NULL);
    }

    /* Set up the format */
    return SDL_InitFormat(format, bpp, Rmask, Gmask, Bmask, Amask);
}

SDL_PixelFormat *
SDL_InitFormat(SDL_PixelFormat * format, int bpp, Uint32 Rmask, Uint32 Gmask,
               Uint32 Bmask, Uint32 Amask)
{
    Uint32 mask;

    /* Set up the format */
    SDL_zerop(format);
    format->BitsPerPixel = bpp;
    format->BytesPerPixel = (bpp + 7) / 8;
    if (Rmask || Bmask || Gmask) {      /* Packed pixels with custom mask */
        format->Rshift = 0;
        format->Rloss = 8;
        if (Rmask) {
            for (mask = Rmask; !(mask & 0x01); mask >>= 1)
                ++format->Rshift;
            for (; (mask & 0x01); mask >>= 1)
                --format->Rloss;
        }
        format->Gshift = 0;
        format->Gloss = 8;
        if (Gmask) {
            for (mask = Gmask; !(mask & 0x01); mask >>= 1)
                ++format->Gshift;
            for (; (mask & 0x01); mask >>= 1)
                --format->Gloss;
        }
        format->Bshift = 0;
        format->Bloss = 8;
        if (Bmask) {
            for (mask = Bmask; !(mask & 0x01); mask >>= 1)
                ++format->Bshift;
            for (; (mask & 0x01); mask >>= 1)
                --format->Bloss;
        }
        format->Ashift = 0;
        format->Aloss = 8;
        if (Amask) {
            for (mask = Amask; !(mask & 0x01); mask >>= 1)
                ++format->Ashift;
            for (; (mask & 0x01); mask >>= 1)
                --format->Aloss;
        }
        format->Rmask = Rmask;
        format->Gmask = Gmask;
        format->Bmask = Bmask;
        format->Amask = Amask;
    } else if (bpp > 8) {       /* Packed pixels with standard mask */
        /* R-G-B */
        if (bpp > 24)
            bpp = 24;
        format->Rloss = 8 - (bpp / 3);
        format->Gloss = 8 - (bpp / 3) - (bpp % 3);
        format->Bloss = 8 - (bpp / 3);
        format->Rshift = ((bpp / 3) + (bpp % 3)) + (bpp / 3);
        format->Gshift = (bpp / 3);
        format->Bshift = 0;
        format->Rmask = ((0xFF >> format->Rloss) << format->Rshift);
        format->Gmask = ((0xFF >> format->Gloss) << format->Gshift);
        format->Bmask = ((0xFF >> format->Bloss) << format->Bshift);
    } else {
        /* Palettized formats have no mask info */
        format->Rloss = 8;
        format->Gloss = 8;
        format->Bloss = 8;
        format->Aloss = 8;
        format->Rshift = 0;
        format->Gshift = 0;
        format->Bshift = 0;
        format->Ashift = 0;
        format->Rmask = 0;
        format->Gmask = 0;
        format->Bmask = 0;
        format->Amask = 0;
    }
    format->palette = NULL;

    return format;
}

/*
 * Change any previous mappings from/to the new surface format
 */
void
SDL_FormatChanged(SDL_Surface * surface)
{
    static int format_version = 0;
    ++format_version;
    if (format_version < 0) {   /* It wrapped... */
        format_version = 1;
    }
    surface->format_version = format_version;
    SDL_InvalidateMap(surface->map);
}

/*
 * Free a previously allocated format structure
 */
void
SDL_FreeFormat(SDL_PixelFormat * format)
{
    if (!format) {
        return;
    }
    SDL_free(format);
}

/*
 * Calculate an 8-bit (3 red, 3 green, 2 blue) dithered palette of colors
 */
void
SDL_DitherColors(SDL_Color * colors, int bpp)
{
    int i;
    if (bpp != 8)
        return;                 /* only 8bpp supported right now */

    for (i = 0; i < 256; i++) {
        int r, g, b;
        /* map each bit field to the full [0, 255] interval,
           so 0 is mapped to (0, 0, 0) and 255 to (255, 255, 255) */
        r = i & 0xe0;
        r |= r >> 3 | r >> 6;
        colors[i].r = r;
        g = (i << 3) & 0xe0;
        g |= g >> 3 | g >> 6;
        colors[i].g = g;
        b = i & 0x3;
        b |= b << 2;
        b |= b << 4;
        colors[i].b = b;
        colors[i].unused = SDL_ALPHA_OPAQUE;
    }
}

/* 
 * Calculate the pad-aligned scanline width of a surface
 */
int
SDL_CalculatePitch(SDL_Surface * surface)
{
    int pitch;

    /* Surface should be 4-byte aligned for speed */
    pitch = surface->w * surface->format->BytesPerPixel;
    switch (surface->format->BitsPerPixel) {
    case 1:
        pitch = (pitch + 7) / 8;
        break;
    case 4:
        pitch = (pitch + 1) / 2;
        break;
    default:
        break;
    }
    pitch = (pitch + 3) & ~3;   /* 4-byte aligning */
    return (pitch);
}

/*
 * Match an RGB value to a particular palette index
 */
Uint8
SDL_FindColor(SDL_Palette * pal, Uint8 r, Uint8 g, Uint8 b)
{
    /* Do colorspace distance matching */
    unsigned int smallest;
    unsigned int distance;
    int rd, gd, bd;
    int i;
    Uint8 pixel = 0;

    smallest = ~0;
    for (i = 0; i < pal->ncolors; ++i) {
        rd = pal->colors[i].r - r;
        gd = pal->colors[i].g - g;
        bd = pal->colors[i].b - b;
        distance = (rd * rd) + (gd * gd) + (bd * bd);
        if (distance < smallest) {
            pixel = i;
            if (distance == 0) {        /* Perfect match! */
                break;
            }
            smallest = distance;
        }
    }
    return (pixel);
}

/* Find the opaque pixel value corresponding to an RGB triple */
Uint32
SDL_MapRGB(const SDL_PixelFormat * format, Uint8 r, Uint8 g, Uint8 b)
{
    if (format->palette == NULL) {
        return (r >> format->Rloss) << format->Rshift
            | (g >> format->Gloss) << format->Gshift
            | (b >> format->Bloss) << format->Bshift | format->Amask;
    } else {
        return SDL_FindColor(format->palette, r, g, b);
    }
}

/* Find the pixel value corresponding to an RGBA quadruple */
Uint32
SDL_MapRGBA(const SDL_PixelFormat * format, Uint8 r, Uint8 g, Uint8 b,
            Uint8 a)
{
    if (format->palette == NULL) {
        return (r >> format->Rloss) << format->Rshift
            | (g >> format->Gloss) << format->Gshift
            | (b >> format->Bloss) << format->Bshift
            | ((a >> format->Aloss) << format->Ashift & format->Amask);
    } else {
        return SDL_FindColor(format->palette, r, g, b);
    }
}

void
SDL_GetRGB(Uint32 pixel, const SDL_PixelFormat * format, Uint8 * r, Uint8 * g,
           Uint8 * b)
{
    if (format->palette == NULL) {
        /*
         * This makes sure that the result is mapped to the
         * interval [0..255], and the maximum value for each
         * component is 255. This is important to make sure
         * that white is indeed reported as (255, 255, 255).
         * This only works for RGB bit fields at least 4 bit
         * wide, which is almost always the case.
         */
        unsigned v;
        v = (pixel & format->Rmask) >> format->Rshift;
        *r = (v << format->Rloss) + (v >> (8 - (format->Rloss << 1)));
        v = (pixel & format->Gmask) >> format->Gshift;
        *g = (v << format->Gloss) + (v >> (8 - (format->Gloss << 1)));
        v = (pixel & format->Bmask) >> format->Bshift;
        *b = (v << format->Bloss) + (v >> (8 - (format->Bloss << 1)));
    } else {
        *r = format->palette->colors[pixel].r;
        *g = format->palette->colors[pixel].g;
        *b = format->palette->colors[pixel].b;
    }
}

void
SDL_GetRGBA(Uint32 pixel, const SDL_PixelFormat * format,
            Uint8 * r, Uint8 * g, Uint8 * b, Uint8 * a)
{
    if (format->palette == NULL) {
        /*
         * This makes sure that the result is mapped to the
         * interval [0..255], and the maximum value for each
         * component is 255. This is important to make sure
         * that white is indeed reported as (255, 255, 255),
         * and that opaque alpha is 255.
         * This only works for RGB bit fields at least 4 bit
         * wide, which is almost always the case.
         */
        unsigned v;
        v = (pixel & format->Rmask) >> format->Rshift;
        *r = (v << format->Rloss) + (v >> (8 - (format->Rloss << 1)));
        v = (pixel & format->Gmask) >> format->Gshift;
        *g = (v << format->Gloss) + (v >> (8 - (format->Gloss << 1)));
        v = (pixel & format->Bmask) >> format->Bshift;
        *b = (v << format->Bloss) + (v >> (8 - (format->Bloss << 1)));
        if (format->Amask) {
            v = (pixel & format->Amask) >> format->Ashift;
            *a = (v << format->Aloss) + (v >> (8 - (format->Aloss << 1)));
        } else {
            *a = SDL_ALPHA_OPAQUE;
        }
    } else {
        *r = format->palette->colors[pixel].r;
        *g = format->palette->colors[pixel].g;
        *b = format->palette->colors[pixel].b;
        *a = SDL_ALPHA_OPAQUE;
    }
}

/* Apply gamma to a set of colors - this is easy. :) */
void
SDL_ApplyGamma(Uint16 * gamma, SDL_Color * colors, SDL_Color * output,
               int ncolors)
{
    int i;

    for (i = 0; i < ncolors; ++i) {
        output[i].r = gamma[0 * 256 + colors[i].r] >> 8;
        output[i].g = gamma[1 * 256 + colors[i].g] >> 8;
        output[i].b = gamma[2 * 256 + colors[i].b] >> 8;
    }
}

/* Map from Palette to Palette */
static Uint8 *
Map1to1(SDL_Palette * src, SDL_Palette * dst, int *identical)
{
    Uint8 *map;
    int i;

    if (identical) {
        if (src->ncolors <= dst->ncolors) {
            /* If an identical palette, no need to map */
            if (src == dst
                ||
                (SDL_memcmp
                 (src->colors, dst->colors,
                  src->ncolors * sizeof(SDL_Color)) == 0)) {
                *identical = 1;
                return (NULL);
            }
        }
        *identical = 0;
    }
    map = (Uint8 *) SDL_malloc(src->ncolors);
    if (map == NULL) {
        SDL_OutOfMemory();
        return (NULL);
    }
    for (i = 0; i < src->ncolors; ++i) {
        map[i] = SDL_FindColor(dst,
                               src->colors[i].r, src->colors[i].g,
                               src->colors[i].b);
    }
    return (map);
}

/* Map from Palette to BitField */
static Uint8 *
Map1toN(SDL_PixelFormat * src, Uint8 Rmod, Uint8 Gmod, Uint8 Bmod, Uint8 Amod,
        SDL_PixelFormat * dst)
{
    Uint8 *map;
    int i;
    int bpp;
    SDL_Palette *pal = src->palette;

    bpp = ((dst->BytesPerPixel == 3) ? 4 : dst->BytesPerPixel);
    map = (Uint8 *) SDL_malloc(pal->ncolors * bpp);
    if (map == NULL) {
        SDL_OutOfMemory();
        return (NULL);
    }

    /* We memory copy to the pixel map so the endianness is preserved */
    for (i = 0; i < pal->ncolors; ++i) {
        Uint8 A = Amod;
        Uint8 R = (Uint8) ((pal->colors[i].r * Rmod) / 255);
        Uint8 G = (Uint8) ((pal->colors[i].g * Gmod) / 255);
        Uint8 B = (Uint8) ((pal->colors[i].b * Bmod) / 255);
        ASSEMBLE_RGBA(&map[i * bpp], dst->BytesPerPixel, dst, R, G, B, A);
    }
    return (map);
}

/* Map from BitField to Dithered-Palette to Palette */
static Uint8 *
MapNto1(SDL_PixelFormat * src, SDL_PixelFormat * dst, int *identical)
{
    /* Generate a 256 color dither palette */
    SDL_Palette dithered;
    SDL_Color colors[256];
    SDL_Palette *pal = dst->palette;

    dithered.ncolors = 256;
    SDL_DitherColors(colors, 8);
    dithered.colors = colors;
    return (Map1to1(&dithered, pal, identical));
}

SDL_BlitMap *
SDL_AllocBlitMap(void)
{
    SDL_BlitMap *map;

    /* Allocate the empty map */
    map = (SDL_BlitMap *) SDL_calloc(1, sizeof(*map));
    if (map == NULL) {
        SDL_OutOfMemory();
        return (NULL);
    }
    map->info.r = 0xFF;
    map->info.g = 0xFF;
    map->info.b = 0xFF;
    map->info.a = 0xFF;

    /* It's ready to go */
    return (map);
}

void
SDL_InvalidateMap(SDL_BlitMap * map)
{
    if (!map) {
        return;
    }
    map->dst = NULL;
    map->format_version = (unsigned int) -1;
    if (map->info.table) {
        SDL_free(map->info.table);
        map->info.table = NULL;
    }
}

int
SDL_MapSurface(SDL_Surface * src, SDL_Surface * dst)
{
    SDL_PixelFormat *srcfmt;
    SDL_PixelFormat *dstfmt;
    SDL_BlitMap *map;

    /* Clear out any previous mapping */
    map = src->map;
    if ((src->flags & SDL_RLEACCEL) == SDL_RLEACCEL) {
        SDL_UnRLESurface(src, 1);
    }
    SDL_InvalidateMap(map);

    /* Figure out what kind of mapping we're doing */
    map->identity = 0;
    srcfmt = src->format;
    dstfmt = dst->format;
    switch (srcfmt->BytesPerPixel) {
    case 1:
        switch (dstfmt->BytesPerPixel) {
        case 1:
            /* Palette --> Palette */
            map->info.table =
                Map1to1(srcfmt->palette, dstfmt->palette, &map->identity);
            if (!map->identity) {
                if (map->info.table == NULL) {
                    return (-1);
                }
            }
            if (srcfmt->BitsPerPixel != dstfmt->BitsPerPixel)
                map->identity = 0;
            break;

        default:
            /* Palette --> BitField */
            map->info.table =
                Map1toN(srcfmt, src->map->info.r, src->map->info.g,
                        src->map->info.b, src->map->info.a, dstfmt);
            if (map->info.table == NULL) {
                return (-1);
            }
            break;
        }
        break;
    default:
        switch (dstfmt->BytesPerPixel) {
        case 1:
            /* BitField --> Palette */
            map->info.table = MapNto1(srcfmt, dstfmt, &map->identity);
            if (!map->identity) {
                if (map->info.table == NULL) {
                    return (-1);
                }
            }
            map->identity = 0;  /* Don't optimize to copy */
            break;
        default:
            /* BitField --> BitField */
            if (FORMAT_EQUAL(srcfmt, dstfmt))
                map->identity = 1;
            break;
        }
        break;
    }

    map->dst = dst;
    map->format_version = dst->format_version;

    /* Choose your blitters wisely */
    return (SDL_CalculateBlit(src));
}

void
SDL_FreeBlitMap(SDL_BlitMap * map)
{
    if (map) {
        SDL_InvalidateMap(map);
        SDL_free(map);
    }
}

/* vi: set ts=4 sw=4 expandtab: */
