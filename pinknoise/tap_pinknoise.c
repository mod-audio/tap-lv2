/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id: tap_pinknoise.c,v 1.2 2004/08/13 18:34:31 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin: */
#define ID_MONO         2155


/* The port numbers for the plugin: */
#define HURST  0
#define SIGNAL 1
#define NOISE  2
#define INPUT  3
#define OUTPUT 4


/* Total number of ports */
#define PORTCOUNT_MONO   5


#define NOISE_LEN  1024


/* The structure used to hold port connection information and state */
typedef struct {
    float * hurst;
    float * signal;
    float * noise;
    float * input;
    float * output;

    float * ring;
    unsigned long buflen;
    unsigned long pos;

    unsigned long sample_rate;
    float run_adding_gain;
} Pinknoise;



/* generate fractal pattern using Midpoint Displacement Method
 * v: buffer of floats to output fractal pattern to
 * N: length of v, MUST be integer power of 2 (ie 128, 256, ...)
 * H: Hurst constant, between 0 and 0.9999 (fractal dimension)
 */
void
fractal(float * v, int N, float H) {

    int l = N;
    int k;
    float r = 2.0f * H*H + 0.3f;
    int c;

    v[0] = 0;
    while (l > 1) {
        k = N / l;
        for (c = 0; c < k; c++) {
            v[c*l + l/2] = (v[c*l] + v[((c+1) * l) % N]) / 2.0f +
                2.0f * r * (rand() - (float)RAND_MAX/2.0f) / (float)RAND_MAX;
            v[c*l + l/2] = LIMIT(v[c*l + l/2], -1.0f, 1.0f);
        }
        l /= 2;
        r /= powf(2, H);
    }
}



/* Construct a new plugin instance. */
LV2_Handle
instantiate_Pinknoise(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

    LV2_Handle * ptr;

    if ((ptr = malloc(sizeof(Pinknoise))) != NULL) {
            ((Pinknoise *)ptr)->sample_rate = SampleRate;
            ((Pinknoise *)ptr)->run_adding_gain = 1.0;

                if ((((Pinknoise *)ptr)->ring =
                     calloc(NOISE_LEN, sizeof(float))) == NULL)
                        return NULL;
                ((Pinknoise *)ptr)->buflen = NOISE_LEN;
                ((Pinknoise *)ptr)->pos = 0;

        return ptr;
    }

    return NULL;
}


/* Connect a port to a data location. */
void
connect_port_Pinknoise(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    Pinknoise * ptr;

    ptr = (Pinknoise *)Instance;


    switch (Port) {
    case HURST:
        ptr->hurst = (float *) DataLocation;
        break;
    case SIGNAL:
        ptr->signal = (float *) DataLocation;
        break;
    case NOISE:
        ptr->noise = (float *) DataLocation;
        break;
    case INPUT:
        ptr->input = (float *) DataLocation;
        break;
    case OUTPUT:
        ptr->output = (float *) DataLocation;
        break;
    }
}



void
run_Pinknoise(LV2_Handle Instance,
          uint32_t SampleCount) {

    Pinknoise * ptr = (Pinknoise *)Instance;

    float * input = ptr->input;
    float * output = ptr->output;
    float hurst = LIMIT(*(ptr->hurst), 0.0f, 1.0f);
    float signal = db2lin(LIMIT(*(ptr->signal), -90.0f, 20.0f));
    float noise = db2lin(LIMIT(*(ptr->noise), -90.0f, 20.0f));
    unsigned long sample_index;

    for (sample_index = 0; sample_index < SampleCount; sample_index++) {

        if (!ptr->pos)
            fractal(ptr->ring, NOISE_LEN, hurst);

        *(output++) = signal * *(input++) +
            noise * push_buffer(0.0f, ptr->ring, ptr->buflen, &(ptr->pos));
    }
}


/* Throw away a Pinknoise effect instance. */
void
cleanup_Pinknoise(LV2_Handle Instance) {
        Pinknoise * ptr = (Pinknoise *)Instance;
        free(ptr->ring);
    free(Instance);
}

const void*
extension_data_Pinknoise(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/pinknoise",
    instantiate_Pinknoise,
    connect_port_Pinknoise,
    NULL,
    run_Pinknoise,
    NULL,
    cleanup_Pinknoise,
    extension_data_Pinknoise
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor *
lv2_descriptor(uint32_t Index) {

    switch (Index) {
    case 0:
        return &Descriptor;
    default:
        return NULL;
    }
}
