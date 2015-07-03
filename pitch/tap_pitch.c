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

    $Id: tap_pitch.c,v 1.2 2004/02/21 17:33:36 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_MONO         2150

/* The port numbers for the plugin: */

#define SEMITONE        0
#define RATE            1
#define DRYLEVEL        2
#define WETLEVEL        3
#define LATENCY         4
#define INPUT           5
#define OUTPUT          6

/* Total number of ports */


#define PORTCOUNT_MONO   7


/* depth of phase mod (yes, this is a magic number) */
#define PM_DEPTH 3681.0f


/* another magic number, derived from the above one */
#define PM_BUFLEN 16027


/* frequency of the modulation signal (Hz) */
#define PM_FREQ 6.0f


#define COS_TABLE_SIZE 1024
float cos_table[COS_TABLE_SIZE];
int flagcos = 0;


/* \sqrt{12}{2} used for key frequency computing */
#define ROOT_12_2  1.059463094f


/* The structure used to hold port connection information and state */

typedef struct {
    float * rate;
    float * semitone;
    float * drylevel;
    float * wetlevel;
    float * latency;
    float * input;
    float * output;

    float * ringbuffer;
    unsigned long buflen;
    unsigned long pos;
    float phase;

    unsigned long sample_rate;
    float run_adding_gain;
} Pitch;



/* Construct a new plugin instance. */
LV2_Handle
instantiate_Pitch(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

        LV2_Handle * ptr;
        int i;

        if(flagcos == 0)
        {
            for (i = 0; i < COS_TABLE_SIZE; i++)
                cos_table[i] = cosf(i * 2.0f * M_PI / COS_TABLE_SIZE);

            flagcos++;
        }

    
    if ((ptr = malloc(sizeof(Pitch))) != NULL) {
        ((Pitch *)ptr)->sample_rate = SampleRate;
        ((Pitch *)ptr)->run_adding_gain = 1.0f;
        if ((((Pitch *)ptr)->ringbuffer =
             calloc(2 * PM_BUFLEN, sizeof(float))) == NULL)
            return NULL;
        ((Pitch *)ptr)->buflen = 2 * PM_BUFLEN * SampleRate / 192000;
        ((Pitch *)ptr)->pos = 0;
        return ptr;
    }
        return NULL;
}


void
activate_Pitch(LV2_Handle Instance) {

    Pitch * ptr = (Pitch *)Instance;
    unsigned long i;

    for (i = 0; i < ptr->buflen; i++)
        ptr->ringbuffer[i] = 0.0f;

    ptr->phase = 0.0f;
}





/* Connect a port to a data location. */
void
connect_port_Pitch(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    Pitch * ptr = (Pitch *)Instance;

    switch (Port) {
    case RATE:
        ptr->rate = (float *) DataLocation;
        break;
    case SEMITONE:
        ptr->semitone = (float *) DataLocation;
        break;
    case DRYLEVEL:
        ptr->drylevel = (float *) DataLocation;
        break;
    case WETLEVEL:
        ptr->wetlevel = (float *) DataLocation;
        break;
    case LATENCY:
        ptr->latency = (float *) DataLocation;
        //*(ptr->latency) = ptr->buflen / 2; /* IS THIS LEGAL? */
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
run_Pitch(LV2_Handle Instance,
        uint32_t SampleCount) {

    Pitch * ptr = (Pitch *)Instance;
    float * input = ptr->input;
    float * output = ptr->output;
    float drylevel = db2lin(LIMIT(*(ptr->drylevel),-90.0f,20.0f));
    float wetlevel = 0.333333f * db2lin(LIMIT(*(ptr->wetlevel),-90.0f,20.0f));
    float buflen = ptr->buflen / 2.0f;
    float semitone = LIMIT(*(ptr->semitone),-12.0f,12.0f);
    float rate;
    float r;
    float depth;

    unsigned long sample_index;
    unsigned long sample_count = SampleCount;

    float in = 0.0f;
    float sign = 1.0f;
    float phase_0 = 0.0f;
    float phase_am_0 = 0.0f;
    float phase_1 = 0.0f;
    float phase_am_1 = 0.0f;
    float phase_2 = 0.0f;
    float phase_am_2 = 0.0f;
    float fpos_0 = 0.0f, fpos_1 = 0.0f, fpos_2 = 0.0f;
    float n_0 = 0.0f, n_1 = 0.0f, n_2 = 0.0f;
    float rem_0 = 0.0f, rem_1 = 0.0f, rem_2 = 0.0f;
    float sa_0, sb_0, sa_1, sb_1, sa_2, sb_2;


    if (semitone == 0.0f)
        rate = LIMIT(*(ptr->rate),-50.0f,100.0f);
    else
        rate = 100.0f * (powf(ROOT_12_2,semitone) - 1.0f);

    r = -1.0f * ABS(rate);
    depth = buflen * LIMIT(ABS(r) / 100.0f, 0.0f, 1.0f);


    if (rate > 0.0f)
        sign = -1.0f;

    for (sample_index = 0; sample_index < sample_count; sample_index++) {

        in = *(input++);

        phase_0 = COS_TABLE_SIZE * PM_FREQ * sample_index / ptr->sample_rate + ptr->phase;
        while (phase_0 >= COS_TABLE_SIZE)
                phase_0 -= COS_TABLE_SIZE;
        phase_am_0 = phase_0 + COS_TABLE_SIZE/2;
        while (phase_am_0 >= COS_TABLE_SIZE)
            phase_am_0 -= COS_TABLE_SIZE;

        phase_1 = phase_0 + COS_TABLE_SIZE/3.0f;
        while (phase_1 >= COS_TABLE_SIZE)
                phase_1 -= COS_TABLE_SIZE;
        phase_am_1 = phase_1 + COS_TABLE_SIZE/2;
        while (phase_am_1 >= COS_TABLE_SIZE)
            phase_am_1 -= COS_TABLE_SIZE;

        phase_2 = phase_0 + 2.0f*COS_TABLE_SIZE/3.0f;
        while (phase_2 >= COS_TABLE_SIZE)
                phase_2 -= COS_TABLE_SIZE;
        phase_am_2 = phase_2 + COS_TABLE_SIZE/2;
        while (phase_am_2 >= COS_TABLE_SIZE)
            phase_am_2 -= COS_TABLE_SIZE;

        push_buffer(in, ptr->ringbuffer, ptr->buflen, &(ptr->pos));

        fpos_0 = depth * (1.0f - sign * (2.0f * phase_0 / COS_TABLE_SIZE - 1.0f));
        n_0 = floorf(fpos_0);
        rem_0 = fpos_0 - n_0;

        fpos_1 = depth * (1.0f - sign * (2.0f * phase_1 / COS_TABLE_SIZE - 1.0f));
        n_1 = floorf(fpos_1);
        rem_1 = fpos_1 - n_1;

        fpos_2 = depth * (1.0f - sign * (2.0f * phase_2 / COS_TABLE_SIZE - 1.0f));
        n_2 = floorf(fpos_2);
        rem_2 = fpos_2 - n_2;

        sa_0 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_0);
        sb_0 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_0 + 1);

        sa_1 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_1);
        sb_1 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_1 + 1);

        sa_2 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_2);
        sb_2 = read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) n_2 + 1);

        *(output++) =
            wetlevel *
            ((1.0f + cos_table[(unsigned long) phase_am_0]) *
             ((1 - rem_0) * sa_0 + rem_0 * sb_0) +
             (1.0f + cos_table[(unsigned long) phase_am_1]) *
             ((1 - rem_1) * sa_1 + rem_1 * sb_1) +
             (1.0f + cos_table[(unsigned long) phase_am_2]) *
             ((1 - rem_2) * sa_2 + rem_2 * sb_2)) +
            drylevel *
            read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos, (unsigned long) depth);

    }

    ptr->phase += COS_TABLE_SIZE * PM_FREQ * sample_index / ptr->sample_rate;
    while (ptr->phase >= COS_TABLE_SIZE)
        ptr->phase -= COS_TABLE_SIZE;

    *(ptr->latency) = buflen - (unsigned long) depth;
}


/* Throw away a Pitch effect instance. */
void
cleanup_Pitch(LV2_Handle Instance) {

    Pitch * ptr = (Pitch *)Instance;
    free(ptr->ringbuffer);
    free(Instance);
}


const void*
extension_data_Pitch(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/pitch",
    instantiate_Pitch,
    connect_port_Pitch,
    activate_Pitch,
    run_Pitch,
    NULL,
    cleanup_Pitch,
    extension_data_Pitch
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
