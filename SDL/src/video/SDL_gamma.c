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

/* Gamma correction support */

#include "SDL_sysvideo.h"


static void
CalculateGammaRamp(float gamma, Uint16 * ramp)
{
    int i;

    /* 0.0 gamma is all black */
    if (gamma <= 0.0f) {
        for (i = 0; i < 256; ++i) {
            ramp[i] = 0;
        }
        return;
    } else
        /* 1.0 gamma is identity */
    if (gamma == 1.0f) {
        for (i = 0; i < 256; ++i) {
            ramp[i] = (i << 8) | i;
        }
        return;
    } else
        /* Calculate a real gamma ramp */
    {
        int value;
        gamma = 1.0f / gamma;
        for (i = 0; i < 256; ++i) {
            value =
                (int) (SDL_pow((double) i / 256.0, gamma) * 65535.0 + 0.5);
            if (value > 65535) {
                value = 65535;
            }
            ramp[i] = (Uint16) value;
        }
    }
}

static void
CalculateGammaFromRamp(float *gamma, Uint16 * ramp)
{
    /* The following is adapted from a post by Garrett Bass on OpenGL
       Gamedev list, March 4, 2000.
     */
    float sum = 0.0f;
    int i, count = 0;

    *gamma = 1.0;
    for (i = 1; i < 256; ++i) {
        if ((ramp[i] != 0) && (ramp[i] != 65535)) {
            double B = (double) i / 256.0;
            double A = ramp[i] / 65535.0;
            sum += (float) (SDL_log(A) / SDL_log(B));
            count++;
        }
    }
    if (count && sum > 0.0f) {
        *gamma = 1.0f / (sum / count);
    }
}

int
SDL_SetGamma(float red, float green, float blue)
{
    Uint16 ramp[3][256];

    CalculateGammaRamp(red, ramp[0]);
    CalculateGammaRamp(green, ramp[1]);
    CalculateGammaRamp(blue, ramp[2]);

    return SDL_SetGammaRamp(ramp[0], ramp[1], ramp[2]);
}

/* Calculating the gamma by integrating the gamma ramps isn't exact,
   so this function isn't officially supported.
*/
int
SDL_GetGamma(float *red, float *green, float *blue)
{
    int succeeded;
    Uint16 ramp[3][256];

    succeeded = SDL_GetGammaRamp(ramp[0], ramp[1], ramp[2]);
    if (succeeded >= 0) {
        CalculateGammaFromRamp(red, ramp[0]);
        CalculateGammaFromRamp(green, ramp[1]);
        CalculateGammaFromRamp(blue, ramp[2]);
    }
    return succeeded;
}

static void
SDL_UninitializedVideo()
{
    SDL_SetError("Video subsystem has not been initialized");
}

int
SDL_SetGammaRampForDisplay(SDL_VideoDisplay * display, const Uint16 * red, const Uint16 * green, const Uint16 * blue)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    int succeeded;

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }

    /* Lazily allocate the gamma tables */
    if (!display->gamma) {
        if (SDL_GetGammaRampForDisplay(display, NULL, NULL, NULL) < 0) {
            return -1;
        }
    }

    /* Fill the gamma table with the new values */
    if (red) {
        SDL_memcpy(&display->gamma[0 * 256], red, 256 * sizeof(*display->gamma));
    }
    if (green) {
        SDL_memcpy(&display->gamma[1 * 256], green, 256 * sizeof(*display->gamma));
    }
    if (blue) {
        SDL_memcpy(&display->gamma[2 * 256], blue, 256 * sizeof(*display->gamma));
    }

    /* Try to set the gamma ramp in the driver */
    succeeded = -1;
    if (_this && _this->SetDisplayGammaRamp) {
        if (SDL_GetFocusWindow()) {
            succeeded =
                _this->SetDisplayGammaRamp(_this, display, display->gamma);
        } else {
            succeeded = 0;
        }
    } else {
        SDL_SetError("Gamma ramp manipulation not supported");
    }
    return succeeded;
}

int
SDL_SetGammaRamp(const Uint16 * red, const Uint16 * green, const Uint16 * blue)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    return SDL_SetGammaRampForDisplay(SDL_CurrentDisplay, red, green, blue);
}

int
SDL_GetGammaRampForDisplay(SDL_VideoDisplay * display, Uint16 * red, Uint16 * green, Uint16 * blue)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }

    /* Lazily allocate the gamma table */
    if (!display->gamma) {
        size_t rampsize = (3 * 256 * sizeof(*display->gamma));

        display->gamma = SDL_malloc(rampsize * 2);
        if (!display->gamma) {
            SDL_OutOfMemory();
            return -1;
        }
        if (_this && _this->GetDisplayGammaRamp) {
            /* Get the real hardware gamma */
            _this->GetDisplayGammaRamp(_this, display, display->gamma);
        } else {
            /* Assume an identity gamma */
            int i;
            for (i = 0; i < 256; ++i) {
                display->gamma[0 * 256 + i] = (i << 8) | i;
                display->gamma[1 * 256 + i] = (i << 8) | i;
                display->gamma[2 * 256 + i] = (i << 8) | i;
            }
        }
        display->saved_gamma = display->gamma + (3 * 256);
        SDL_memcpy(display->saved_gamma, display->gamma, rampsize);
    }

    /* Just copy from our internal table */
    if (red) {
        SDL_memcpy(red, &display->gamma[0 * 256], 256 * sizeof(*red));
    }
    if (green) {
        SDL_memcpy(green, &display->gamma[1 * 256], 256 * sizeof(*green));
    }
    if (blue) {
        SDL_memcpy(blue, &display->gamma[2 * 256], 256 * sizeof(*blue));
    }
    return 0;
}

int
SDL_GetGammaRamp(Uint16 * red, Uint16 * green, Uint16 * blue)
{
    SDL_VideoDevice *_this = SDL_GetVideoDevice();
    if (!_this) {
        SDL_UninitializedVideo();
        return -1;
    }
    return SDL_GetGammaRampForDisplay(SDL_CurrentDisplay, red, green, blue);
}

/* vi: set ts=4 sw=4 expandtab: */
