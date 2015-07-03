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

    $Id: tap_vibrato.c,v 1.3 2004/02/21 17:33:36 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_MONO         2148

/* The port numbers for the plugin: */

#define FREQ            0
#define DEPTH           1
#define DRYLEVEL        2
#define WETLEVEL        3
#define LATENCY         4
#define INPUT           5
#define OUTPUT          6


/* Total number of ports */


/*
 * This has to be bigger than 0.2f * sample_rate / (2*PI) for any sample rate.
 * At 192 kHz 6238 is needed so this should be enough.
 */
#define PM_DEPTH 6300


#define PM_FREQ 30.0f


#define COS_TABLE_SIZE 1024
float cos_table[COS_TABLE_SIZE];
int flag = 0;


/* The structure used to hold port connection information and state */

typedef struct {
    float * depth;
    float * freq;
    float * drylevel;
    float * wetlevel;
    float * latency;
    float * input;
    float * output;

    float * ringbuffer;
    unsigned long buflen;
    unsigned long pos;
    float phase;

    double sample_rate;
} Vibrato;



/* Construct a new plugin instance. */
LV2_Handle
instantiate_Vibrato(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

    Vibrato *plugin;

    plugin = (Vibrato *) malloc(sizeof(Vibrato));

    if (plugin)
    {
        plugin->sample_rate = SampleRate;

        plugin->ringbuffer = calloc(2 * PM_DEPTH, sizeof(float));
        if (!plugin) return NULL;
        plugin->buflen = ceil(0.2f * SampleRate / M_PI);
        plugin->pos = 0;

        if(flag == 0)
        {
        int i;

        for (i = 0; i < COS_TABLE_SIZE; i++)
            cos_table[i] = cosf(i * 2.0f * M_PI / COS_TABLE_SIZE);

        flag++;
        }

        return plugin;
    }

    return NULL;
}


void
activate_Vibrato(LV2_Handle Instance) {

    Vibrato * ptr = (Vibrato *)Instance;
    unsigned long i;

    for (i = 0; i < 2 * PM_DEPTH; i++)
        ptr->ringbuffer[i] = 0.0f;

    ptr->phase = 0.0f;
}

void
deactivate_Vibrato(LV2_Handle Instance) {


}


/* Connect a port to a data location. */
void
connect_port_Vibrato(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    Vibrato * ptr = (Vibrato *)Instance;

    switch (Port) {
    case DEPTH:
        ptr->depth = (float *) DataLocation;
        break;
    case FREQ:
        ptr->freq = (float *) DataLocation;
        break;
    case DRYLEVEL:
        ptr->drylevel = (float *) DataLocation;
        break;
    case WETLEVEL:
        ptr->wetlevel = (float *) DataLocation;
        break;
    case LATENCY:
        ptr->latency = (float *) DataLocation;
        // *(ptr->latency) = ptr->buflen / 2;  /* IS THIS LEGAL? */
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
run_Vibrato(LV2_Handle Instance,
        uint32_t SampleCount) {

    Vibrato * ptr = (Vibrato *)Instance;

    float freq = LIMIT(*(ptr->freq),0.0f,PM_FREQ);
    float depth =
        LIMIT(LIMIT(*(ptr->depth),0.0f,20.0f) * ptr->sample_rate / 200.0f / M_PI / freq,
              0, ptr->buflen / 2);
    float drylevel = db2lin(LIMIT(*(ptr->drylevel),-90.0f,20.0f));
    float wetlevel = db2lin(LIMIT(*(ptr->wetlevel),-90.0f,20.0f));
    float * input = ptr->input;
    float * output = ptr->output;

    unsigned long sample_index;
    unsigned long sample_count = SampleCount;

    float in = 0.0f;
    float phase = 0.0f;
    float fpos = 0.0f;
    float n = 0.0f;
    float rem = 0.0f;
    float s_a, s_b;


    if (freq == 0.0f)
        depth = 0.0f;
    for (sample_index = 0; sample_index < sample_count; sample_index++) {

        in = *(input++);

        phase = COS_TABLE_SIZE * freq * sample_index / ptr->sample_rate + ptr->phase;
        while (phase >= COS_TABLE_SIZE)
                phase -= COS_TABLE_SIZE;

        push_buffer(in, ptr->ringbuffer, ptr->buflen, &(ptr->pos));

        fpos = depth * (1.0f - cos_table[(unsigned long) phase]);
        n = floorf(fpos);
        rem = fpos - n;

        s_a = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n);
        s_b = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n + 1);

        *(output++) = wetlevel * ((1 - rem) * s_a + rem * s_b) +
            drylevel * read_buffer(ptr->ringbuffer, ptr->buflen,
                           ptr->pos, ptr->buflen / 2);

    }

    ptr->phase += COS_TABLE_SIZE * freq * sample_index / ptr->sample_rate;
    while (ptr->phase >= COS_TABLE_SIZE)
        ptr->phase -= COS_TABLE_SIZE;

    *(ptr->latency) = ptr->buflen / 2;

}

/* Throw away a Vibrato effect instance. */
void
cleanup_Vibrato(LV2_Handle Instance) {

    Vibrato * ptr = (Vibrato *)Instance;
    free(ptr->ringbuffer);
    free(Instance);
}

const void*
extension_data_Vibrato(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/vibrato",
    instantiate_Vibrato,
    connect_port_Vibrato,
    activate_Vibrato,
    run_Vibrato,
    deactivate_Vibrato,
    cleanup_Vibrato,
    extension_data_Vibrato
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

