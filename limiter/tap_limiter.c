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

    $Id: tap_limiter.c,v 1.6 2012/07/08 14:19:35 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin: */

#define ID_MONO         2145

/* The port numbers for the plugin: */

#define LIMIT_VOL       0
#define OUT_VOL         1
#define LATENCY         2
#define INPUT           3
#define OUTPUT          4

/* Total number of ports */

#define PORTCOUNT_MONO   5


/* Size of a ringbuffer that must be large enough to hold audio
 * between two zero-crosses in any case (or you'll hear
 * distortion). 40 Hz sound at 192kHz yields a half-period of 2400
 * samples, so this should be enough.
 */
#define RINGBUF_SIZE 2500


/* The structure used to hold port connection information and state */

typedef struct {
	float * limit_vol;
	float * out_vol;
	float * latency;
	float * input;
	float * output;

	float * ringbuffer;
	unsigned long buflen;
	unsigned long pos;
	unsigned long ready_num;

	double sample_rate;
} Limiter;




/* Construct a new plugin instance. */
LV2_Handle
instantiate_Limiter(const LV2_Descriptor * Descriptor, double sample_rate, const char* bundle_path, const LV2_Feature* const* features) {

	LV2_Handle * ptr;

	if ((ptr = malloc(sizeof(Limiter))) != NULL) {
		((Limiter *)ptr)->sample_rate = sample_rate;

		if ((((Limiter *)ptr)->ringbuffer =
		     calloc(RINGBUF_SIZE, sizeof(float))) == NULL)
			return NULL;

		/* 80 Hz is the lowest frequency with which zero-crosses were
		 * observed to occur (this corresponds to 40 Hz signal frequency).
		 */
		((Limiter *)ptr)->buflen = ((Limiter *)ptr)->sample_rate / 80;

		((Limiter *)ptr)->pos = 0;
		((Limiter *)ptr)->ready_num = 0;

		return ptr;
	}
       	return NULL;
}


void
activate_Limiter(LV2_Handle Instance) {

	Limiter * ptr = (Limiter *)Instance;
	unsigned long i;

	for (i = 0; i < RINGBUF_SIZE; i++)
		ptr->ringbuffer[i] = 0.0f;
}

void
deactivate_Limiter(LV2_Handle Instance) {


}



/* Connect a port to a data location. */
void
connect_port_Limiter(LV2_Handle Instance,
		     uint32_t Port,
		     void * DataLocation) {

	Limiter * ptr = (Limiter *)Instance;

	switch (Port) {
	case LIMIT_VOL:
		ptr->limit_vol = (float*) DataLocation;
		break;
	case OUT_VOL:
		ptr->out_vol = (float*) DataLocation;
		break;
	case LATENCY:
		ptr->latency = (float*) DataLocation;
		// *(ptr->latency) = ptr->buflen;  /* IS THIS LEGAL? */
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
run_Limiter(LV2_Handle Instance,
	    uint32_t  SampleCount) {

	Limiter * ptr = (Limiter *)Instance;

	float * input = ptr->input;
	float * output = ptr->output;
	float limit_vol = db2lin(LIMIT(*(ptr->limit_vol),-30.0f,20.0f));
	float out_vol = db2lin(LIMIT(*(ptr->out_vol),-30.0f,20.0f));
	unsigned long sample_index;
	unsigned long sample_count = SampleCount;
	unsigned long index_offs = 0;
	unsigned long i;
	float max_value = 0;
	float section_gain = 0;
	unsigned long run_length;
	unsigned long total_length = 0;


	while (total_length < sample_count) {

		run_length = ptr->buflen;
		if (total_length + run_length > sample_count)
			run_length = sample_count - total_length;

		while (ptr->ready_num < run_length) {
			if (read_buffer(ptr->ringbuffer, ptr->buflen,
					ptr->pos, ptr->ready_num) >= 0.0f) {
				index_offs = 0;
				do {
					index_offs++;
					if (ptr->ready_num + index_offs == run_length) {
						/*
						 * No more zero-crossing point in this chunk.
						 * Fetch more samples unless we are at the last one.
						 */
						if (ptr->ready_num != 0) {
							run_length = ptr->ready_num;
							goto push;
						}
						break;
					}
				} while (read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos,
						     ptr->ready_num + index_offs) >= 0.0f);
			} else {
				index_offs = 0;
				do {
					index_offs++;
					if (ptr->ready_num + index_offs == run_length) {
						/*
						 * No more zero-crossing point in this chunk.
						 * Fetch more samples unless we are at the last one.
						 */
						if (ptr->ready_num != 0) {
							run_length = ptr->ready_num;
							goto push;
						}
						break;
					}
				} while (read_buffer(ptr->ringbuffer, ptr->buflen, ptr->pos,
						     ptr->ready_num + index_offs) < 0.0f);
			}

			/* search for max value in scanned halfcycle */
			max_value = 0;
			for (i = ptr->ready_num; i < ptr->ready_num + index_offs; i++) {
				if (fabs(read_buffer(ptr->ringbuffer, ptr->buflen,
						     ptr->pos, i)) > max_value)
					max_value = fabs(read_buffer(ptr->ringbuffer,
								     ptr->buflen, ptr->pos, i));
			}
			section_gain = limit_vol / max_value;
			if (max_value > limit_vol)
				for (i = ptr->ready_num; i < ptr->ready_num + index_offs; i++) {
					write_buffer(read_buffer(ptr->ringbuffer, ptr->buflen,
								 ptr->pos, i) * section_gain,
						     ptr->ringbuffer, ptr->buflen, ptr->pos, i);
				}
			ptr->ready_num += index_offs;
		}

	push:
		/* push run_length values out of ringbuffer, feed with input */
		for (sample_index = 0; sample_index < run_length; sample_index++) {
			*(output++) = out_vol *
				push_buffer(*(input++), ptr->ringbuffer,
					    ptr->buflen, &(ptr->pos));
		}
		ptr->ready_num -= run_length;
		total_length += run_length;
	}
	*(ptr->latency) = ptr->buflen;
}


void
cleanup_Limiter(LV2_Handle Instance) {
	free(Instance);
}

const void*
extension_data_Limiter(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/limiter",
    instantiate_Limiter,
    connect_port_Limiter,
    activate_Limiter,
    run_Limiter,
    deactivate_Limiter,
    cleanup_Limiter,
    extension_data_Limiter
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
