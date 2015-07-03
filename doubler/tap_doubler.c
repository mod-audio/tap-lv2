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

    $Id: tap_doubler.c,v 1.4 2004/08/13 18:34:31 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <lv2.h>
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_STEREO       2156

/* The port numbers for the plugin: */

#define TIME            0
#define PITCH           1
#define DRYLEVEL        2
#define DRYPOSL         3
#define DRYPOSR         4
#define WETLEVEL        5
#define WETPOSL         6
#define WETPOSR         7
#define INPUT_L         8
#define INPUT_R         9
#define OUTPUT_L       10
#define OUTPUT_R       11

/* Total number of ports */


#define PORTCOUNT_STEREO 12


/* Number of pink noise samples to be generated at once */
#define NOISE_LEN 1024

/*
 * Largest buffer length needed (at 192 kHz).
 */
#define BUFLEN 11520



/* The structure used to hold port connection information and state */

typedef struct {
	float * time;
	float * pitch;
	float * drylevel;
	float * dryposl;
	float * dryposr;
	float * wetlevel;
	float * wetposl;
	float * wetposr;
	float * input_L;
	float * input_R;
	float * output_L;
	float * output_R;

	float old_time;
	float old_pitch;

	float * ring_L;
	unsigned long buflen_L;
	unsigned long pos_L;

	float * ring_R;
	unsigned long buflen_R;
	unsigned long pos_R;

	float * ring_pnoise;
	unsigned long buflen_pnoise;
	unsigned long pos_pnoise;

	float * ring_dnoise;
	unsigned long buflen_dnoise;
	unsigned long pos_dnoise;

	float delay;
	float d_delay;
	float p_delay;
	unsigned long n_delay;

	float pitchmod;
	float d_pitch;
	float p_pitch;
	unsigned long n_pitch;

	unsigned long p_stretch;
	unsigned long d_stretch;

	double sample_rate;
} Doubler;


/* generate fractal pattern using Midpoint Displacement Method
 * v: buffer of floats to output fractal pattern to
 * N: length of v, MUST be integer power of 2 (ie 128, 256, ...)
 * H: Hurst constant, between 0 and 0.9999 (fractal dimension)
 */
void
fractal(float * v, int N, float H) {

        int l = N;
        int k;
        float r = 1.0f;
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
instantiate_Doubler(const LV2_Descriptor * Descriptor, double sample_rate, const char* bundle_path, const LV2_Feature* const* features) {

        LV2_Handle * ptr;

	if ((ptr = malloc(sizeof(Doubler))) != NULL) {
		((Doubler *)ptr)->sample_rate = sample_rate;

		if ((((Doubler *)ptr)->ring_L =
		     calloc(BUFLEN * sample_rate / 192000, sizeof(float))) == NULL)
			return NULL;
		((Doubler *)ptr)->buflen_L = BUFLEN * sample_rate / 192000;
		((Doubler *)ptr)->pos_L = 0;

		if ((((Doubler *)ptr)->ring_R =
		     calloc(BUFLEN * sample_rate / 192000, sizeof(float))) == NULL)
			return NULL;
		((Doubler *)ptr)->buflen_R = BUFLEN * sample_rate / 192000;
		((Doubler *)ptr)->pos_R = 0;

		if ((((Doubler *)ptr)->ring_pnoise =
		     calloc(NOISE_LEN, sizeof(float))) == NULL)
			return NULL;
		((Doubler *)ptr)->buflen_pnoise = NOISE_LEN;
		((Doubler *)ptr)->pos_pnoise = 0;

		if ((((Doubler *)ptr)->ring_dnoise =
		     calloc(NOISE_LEN, sizeof(float))) == NULL)
			return NULL;
		((Doubler *)ptr)->buflen_dnoise = NOISE_LEN;
		((Doubler *)ptr)->pos_dnoise = 0;

		((Doubler *)ptr)->d_stretch = sample_rate / 10;
		((Doubler *)ptr)->p_stretch = sample_rate / 1000;

		((Doubler *)ptr)->delay = 0.0f;
		((Doubler *)ptr)->d_delay = 0.0f;
		((Doubler *)ptr)->p_delay = 0.0f;
		((Doubler *)ptr)->n_delay = ((Doubler *)ptr)->d_stretch;

		((Doubler *)ptr)->pitchmod = 0.0f;
		((Doubler *)ptr)->d_pitch = 0.0f;
		((Doubler *)ptr)->p_pitch = 0.0f;
		((Doubler *)ptr)->n_pitch = ((Doubler *)ptr)->p_stretch;

		return ptr;
	}
       	return NULL;
}


void
activate_Doubler(LV2_Handle Instance) {

	Doubler * ptr = (Doubler *)Instance;
	unsigned long i;

	for (i = 0; i < BUFLEN * ptr->sample_rate / 192000; i++) {
		ptr->ring_L[i] = 0.0f;
		ptr->ring_R[i] = 0.0f;
	}

	ptr->old_time = -1.0f;
	ptr->old_pitch = -1.0f;
}

void
deactivate_Doubler(LV2_Handle Instance) {

}



/* Connect a port to a data location. */
void
connect_port_Doubler(LV2_Handle Instance,
		     uint32_t Port,
		     void * data) {

	Doubler * ptr = (Doubler *)Instance;

	switch (Port) {
	case TIME:
		ptr->time = (float*) data;
		break;
	case PITCH:
		ptr->pitch = (float*) data;
		break;
	case DRYLEVEL:
		ptr->drylevel = (float*) data;
		break;
	case DRYPOSL:
		ptr->dryposl = (float*) data;
		break;
	case DRYPOSR:
		ptr->dryposr = (float*) data;
		break;
	case WETLEVEL:
		ptr->wetlevel = (float*) data;
		break;
	case WETPOSL:
		ptr->wetposl = (float*) data;
		break;
	case WETPOSR:
		ptr->wetposr = (float*) data;
		break;
	case INPUT_L:
		ptr->input_L = (float*) data;
		break;
	case INPUT_R:
		ptr->input_R = (float*) data;
		break;
	case OUTPUT_L:
		ptr->output_L = (float*) data;
		break;
	case OUTPUT_R:
		ptr->output_R = (float*) data;
		break;
	}
}



void
run_Doubler(LV2_Handle Instance,
	    uint32_t SampleCount) {

	Doubler * ptr = (Doubler *)Instance;

	float pitch = LIMIT(*(ptr->pitch),0.0f,1.0f) + 0.75f;
	float depth = LIMIT(((1.0f - LIMIT(*(ptr->pitch),0.0f,1.0f)) * 1.75f + 0.25f) *
				  ptr->sample_rate / 6000.0f / M_PI,
				  0, ptr->buflen_L / 2);
	float time = LIMIT(*(ptr->time), 0.0f, 1.0f) + 0.5f;
	float drylevel = db2lin(LIMIT(*(ptr->drylevel),-90.0f,20.0f));
	float wetlevel = db2lin(LIMIT(*(ptr->wetlevel),-90.0f,20.0f));
	float dryposl = 1.0f - LIMIT(*(ptr->dryposl), 0.0f, 1.0f);
	float dryposr = LIMIT(*(ptr->dryposr), 0.0f, 1.0f);
	float wetposl = 1.0f - LIMIT(*(ptr->wetposl), 0.0f, 1.0f);
	float wetposr = LIMIT(*(ptr->wetposr), 0.0f, 1.0f);
	float * input_L = ptr->input_L;
	float * input_R = ptr->input_R;
	float * output_L = ptr->output_L;
	float * output_R = ptr->output_R;

	unsigned long sample_index;
	unsigned long sample_count = SampleCount;

	float in_L = 0.0f;
	float in_R = 0.0f;
	float out_L = 0.0f;
	float out_R = 0.0f;

	float fpos = 0.0f;
	float n = 0.0f;
	float rem = 0.0f;
	float s_a_L, s_a_R, s_b_L, s_b_R;
	float prev_p_pitch = 0.0f;
	float prev_p_delay = 0.0f;
	float delay;

	float drystream_L = 0.0f;
	float drystream_R = 0.0f;
	float wetstream_L = 0.0f;
	float wetstream_R = 0.0f;

	if (ptr->old_pitch != pitch) {
		ptr->pitchmod = ptr->p_pitch;
		prev_p_pitch = ptr->p_pitch;
		fractal(ptr->ring_pnoise, NOISE_LEN, pitch);
		ptr->pos_pnoise = 0;
		ptr->p_pitch = push_buffer(0.0f, ptr->ring_pnoise,
					   ptr->buflen_pnoise, &(ptr->pos_pnoise));
		ptr->d_pitch = (ptr->p_pitch - prev_p_pitch) / (float)(ptr->p_stretch);
		ptr->n_pitch = 0;

		ptr->old_pitch = pitch;
	}

	if (ptr->old_time != time) {
		ptr->delay = ptr->p_delay;
		prev_p_delay = ptr->p_delay;
		fractal(ptr->ring_dnoise, NOISE_LEN, time);
		ptr->pos_dnoise = 0;
		ptr->p_delay = push_buffer(0.0f, ptr->ring_dnoise,
					   ptr->buflen_dnoise, &(ptr->pos_dnoise));
		ptr->d_delay = (ptr->p_delay - prev_p_delay) / (float)(ptr->d_stretch);
		ptr->n_delay = 0;

		ptr->old_time = time;
	}


	for (sample_index = 0; sample_index < sample_count; sample_index++) {

		in_L = *(input_L++);
		in_R = *(input_R++);

		push_buffer(in_L, ptr->ring_L, ptr->buflen_L, &(ptr->pos_L));
		push_buffer(in_R, ptr->ring_R, ptr->buflen_R, &(ptr->pos_R));

		if (ptr->n_pitch < ptr->p_stretch) {
			ptr->pitchmod += ptr->d_pitch;
			ptr->n_pitch++;
		} else {
			ptr->pitchmod = ptr->p_pitch;
			prev_p_pitch = ptr->p_pitch;
			if (!ptr->pos_pnoise) {
				fractal(ptr->ring_pnoise, NOISE_LEN, pitch);
			}
			ptr->p_pitch = push_buffer(0.0f, ptr->ring_pnoise,
						   ptr->buflen_pnoise, &(ptr->pos_pnoise));
			ptr->d_pitch = (ptr->p_pitch - prev_p_pitch) / (float)(ptr->p_stretch);
			ptr->n_pitch = 0;
		}

		if (ptr->n_delay < ptr->d_stretch) {
			ptr->delay += ptr->d_delay;
			ptr->n_delay++;
		} else {
			ptr->delay = ptr->p_delay;
			prev_p_delay = ptr->p_delay;
			if (!ptr->pos_dnoise) {
				fractal(ptr->ring_dnoise, NOISE_LEN, time);
			}
			ptr->p_delay = push_buffer(0.0f, ptr->ring_dnoise,
						   ptr->buflen_dnoise, &(ptr->pos_dnoise));
			ptr->d_delay = (ptr->p_delay - prev_p_delay) / (float)(ptr->d_stretch);
			ptr->n_delay = 0;
		}

		delay = (12.5f * ptr->delay + 37.5f) * ptr->sample_rate / 1000.0f;
		fpos = ptr->buflen_L - depth * (1.0f - ptr->pitchmod) - delay - 1.0f;
		n = floorf(fpos);
		rem = fpos - n;

		s_a_L = read_buffer(ptr->ring_L, ptr->buflen_L,
				    ptr->pos_L, (unsigned long) n);
		s_b_L = read_buffer(ptr->ring_L, ptr->buflen_L,
				    ptr->pos_L, (unsigned long) n + 1);

		s_a_R = read_buffer(ptr->ring_R, ptr->buflen_R,
				    ptr->pos_R, (unsigned long) n);
		s_b_R = read_buffer(ptr->ring_R, ptr->buflen_R,
				    ptr->pos_R, (unsigned long) n + 1);

		drystream_L = drylevel * in_L;
		drystream_R = drylevel * in_R;
		wetstream_L = wetlevel * ((1 - rem) * s_a_L + rem * s_b_L);
		wetstream_R = wetlevel * ((1 - rem) * s_a_R + rem * s_b_R);

		out_L = dryposl * drystream_L + (1.0f - dryposr) * drystream_R +
			wetposl * wetstream_L + (1.0f - wetposr) * wetstream_R;
		out_R = (1.0f - dryposl) * drystream_L + dryposr * drystream_R +
			(1.0f - wetposl) * wetstream_L + wetposr * wetstream_R;

		*(output_L++) = out_L;
		*(output_R++) = out_R;
	}
}


/* Throw away a Doubler effect instance. */
void
cleanup_Doubler(LV2_Handle Instance) {

  	Doubler * ptr = (Doubler *)Instance;
	free(ptr->ring_L);
	free(ptr->ring_R);
	free(ptr->ring_pnoise);
	free(ptr->ring_dnoise);
	free(Instance);
}

const void*
extension_data_Doubler(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/doubler",
    instantiate_Doubler,
    connect_port_Doubler,
    activate_Doubler,
    run_Doubler,
    deactivate_Doubler,
    cleanup_Doubler,
    extension_data_Doubler
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
