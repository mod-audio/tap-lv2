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

    $Id: tap_deesser.c,v 1.7 2004/05/01 16:15:06 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin: */

#define ID_MONO         2147

/* The port numbers for the plugin: */

#define THRESHOLD       0
#define FREQ            1
#define SIDECHAIN       2
#define MONITOR         3
#define ATTENUAT        4
#define INPUT           5
#define OUTPUT          6


/* Total number of ports */

#define PORTCOUNT_MONO   7


/* Bandwidth of sidechain lowpass/highpass filters */
#define SIDECH_BW       0.3f

/* Used to hold 10 ms gain data, enough for sample rates up to 192 kHz */
#define RINGBUF_SIZE    2000



/* 4 digits precision from 1.000 to 9.999 */
float log10_table[9000];
int flag = 0;


/* The structure used to hold port connection information and state */

typedef struct {
	float * threshold;
	float * audiomode;
	float * freq;
	float * sidechain;
	float * monitor;
	float * attenuat;
	float * input;
	float * output;

	biquad sidech_lo_filter;
	biquad sidech_hi_filter;
	float * ringbuffer;
	unsigned long buflen;
	unsigned long pos;
	float sum;
	float old_freq;

	double sample_rate;
} DeEsser;


/* fast linear to decibel conversion using log10_table[] */
float fast_lin2db(float lin) {

        unsigned long k;
        int exp = 0;
        float mant = ABS(lin);

	/* sanity checks */
	if (mant == 0.0f)
		return(-1.0f/0.0f); /* -inf */
	if (mant == 1.0f/0.0f) /* +inf */
		return(mant);

        while (mant < 1.0f) {
                mant *= 10;
                exp --;
        }
        while (mant >= 10.0f) {
                mant /= 10;
                exp ++;
        }

        k = (mant - 0.999999f) * 1000.0f;
        return 20.0f * (log10_table[k] + exp);
}



/* Construct a new plugin instance. */
LV2_Handle
instantiate_DeEsser(const LV2_Descriptor * Descriptor,double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

	LV2_Handle * ptr;
	int i;

	if ((ptr = malloc(sizeof(DeEsser))) != NULL) {
		((DeEsser *)ptr)->sample_rate = SampleRate;

		/* init filters */
		biquad_init(&((DeEsser *)ptr)->sidech_lo_filter);
		biquad_init(&((DeEsser *)ptr)->sidech_hi_filter);

		/* alloc mem for ringbuffer */
		if ((((DeEsser *)ptr)->ringbuffer =
		     calloc(RINGBUF_SIZE, sizeof(float))) == NULL)
			return NULL;

                /* 10 ms attenuation data is stored */
		((DeEsser *)ptr)->buflen = ((DeEsser *)ptr)->sample_rate / 100;

		((DeEsser *)ptr)->pos = 0;
		((DeEsser *)ptr)->sum = 0.0f;
		((DeEsser *)ptr)->old_freq = 0;
		if (flag == 0)
		{
                for (i = 0; i < 9000; i++)
                    log10_table[i] = log10f(1.0f + i / 1000.0f);
                flag++;
		}

		return ptr;
	}
	return NULL;
}


void
activate_DeEsser(LV2_Handle Instance) {

	DeEsser * ptr = (DeEsser *)Instance;
	unsigned long i;

	for (i = 0; i < RINGBUF_SIZE; i++)
		ptr->ringbuffer[i] = 0.0f;
}

void
deactivate_DeEsser(LV2_Handle Instance) {


}

/* Connect a port to a data location. */
void
connect_port_DeEsser(LV2_Handle Instance,
		     uint32_t Port,
		     void * DataLocation) {

	DeEsser * ptr;

	ptr = (DeEsser *)Instance;
	switch (Port) {
	case THRESHOLD:
		ptr->threshold = (float*) DataLocation;
		break;
	case FREQ:
		ptr->freq = (float*) DataLocation;
		break;
	case SIDECHAIN:
		ptr->sidechain = (float*) DataLocation;
		break;
	case MONITOR:
		ptr->monitor = (float*) DataLocation;
		break;
	case ATTENUAT:
		ptr->attenuat = (float*) DataLocation;
		// *(ptr->attenuat) = 0.0f;
		break;
	case INPUT:
		ptr->input = (float*) DataLocation;
		break;
	case OUTPUT:
		ptr->output = (float*) DataLocation;
		break;
	}
}



void
run_DeEsser(LV2_Handle Instance,
	    uint32_t SampleCount) {

	DeEsser * ptr = (DeEsser *)Instance;

	float * input = ptr->input;
	float * output = ptr->output;
	float threshold = LIMIT(*(ptr->threshold),-50.0f,10.0f);
	float freq = LIMIT(*(ptr->freq),2000.0f,16000.0f);
	float sidechain = LIMIT(*(ptr->sidechain),0.0f,1.0f);
	float monitor = LIMIT(*(ptr->monitor),0.0f,1.0f);
	unsigned long sample_index;

	float in = 0;
	float out = 0;
	float sidech = 0;
	float ampl_db = 0.0f;
	float attn = 0.0f;
	float max_attn = 0.0f;


	if (ptr->old_freq != freq) {
		lp_set_params(&ptr->sidech_lo_filter, freq, SIDECH_BW, ptr->sample_rate);
		hp_set_params(&ptr->sidech_hi_filter, freq, SIDECH_BW, ptr->sample_rate);
		ptr->old_freq = freq;
	}

	for (sample_index = 0; sample_index < SampleCount; sample_index++) {

		in = *(input++);

		/* process sidechain filters */
		sidech = biquad_run(&ptr->sidech_hi_filter, in);
		if (sidechain > 0.1f)
			sidech = biquad_run(&ptr->sidech_lo_filter, sidech);

		ampl_db = fast_lin2db(sidech);
		if (ampl_db <= threshold)
			attn = 0.0f;
		else
			attn = -0.5f * (ampl_db - threshold);

		ptr->sum += attn;
		ptr->sum -= push_buffer(attn, ptr->ringbuffer, ptr->buflen, &ptr->pos);

		if (-1.0f * ptr->sum > max_attn)
			max_attn = -0.01f * ptr->sum;

		in *= db2lin(ptr->sum / 100.0f);


		/* output selector */
		if (monitor > 0.1f)
			out = sidech;
		else
			out = in;

		*(output++) = out;
		*(ptr->attenuat) = LIMIT(max_attn,0,10);
	}
}

/* Throw away a DeEsser effect instance. */
void
cleanup_DeEsser(LV2_Handle Instance) {

	DeEsser * ptr = (DeEsser *)Instance;
	free(ptr->ringbuffer);
	free(Instance);
}

const void*
extension_data_DeEsser(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://portalmod.com/plugins/tap/deesser",
    instantiate_DeEsser,
    connect_port_DeEsser,
    activate_DeEsser,
    run_DeEsser,
    deactivate_DeEsser,
    cleanup_DeEsser,
    extension_data_DeEsser
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
