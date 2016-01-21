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

    $Id: tap_echo.c,v 1.7 2004/12/06 09:32:41 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin: */

#define ID_STEREO       2143

/* The port numbers for the plugin: */

#define DELAYTIME_L 0
#define FEEDBACK_L  1
#define DELAYTIME_R 2
#define FEEDBACK_R  3
#define STRENGTH_L  4
#define STRENGTH_R  5
#define DRYLEVEL    6
#define MODE        7
#define HAAS        8
#define REV_OUTCH   9

#define INPUT_L     10
#define OUTPUT_L    11
#define INPUT_R     12
#define OUTPUT_R    13

/* Total number of ports */

#define PORTCOUNT_STEREO 14


/* Maximum delay (ms) */

#define MAX_DELAY        2000


/* The structure used to hold port connection information and state */

typedef struct {
	float * delaytime_L;
	float * delaytime_R;
	float * feedback_L;
	float * feedback_R;
	float * strength_L;
	float smoothstrength_L; //for parametersmoothing
	float * strength_R;
	float smoothstrength_R; //for parametersmoothing
	float * drylevel;
	float smoothdry; //for parametersmoothing
	float * mode;
	float * haas;
	float * rev_outch;

	float * input_L;
	float * output_L;
	float * input_R;
	float * output_R;

	double sample_rate;
	float mpx_out_L;
	float mpx_out_R;

	float * ringbuffer_L;
	float * ringbuffer_R;
	unsigned long * buffer_pos_L;
	unsigned long * buffer_pos_R;
} Echo;




/* Construct a new plugin instance. */
LV2_Handle
instantiate_Echo(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

	LV2_Handle * ptr;

	if ((ptr = malloc(sizeof(Echo))) != NULL) {
		((Echo *)ptr)->sample_rate = SampleRate;
		((Echo *)ptr)->smoothdry = -4.0f;
		((Echo *)ptr)->smoothstrength_L = -4.0f;
		((Echo *)ptr)->smoothstrength_R = -4.0f;

		/* allocate memory for ringbuffers and related dynamic vars */
		if ((((Echo *)ptr)->ringbuffer_L =
		     calloc(MAX_DELAY * ((Echo *)ptr)->sample_rate / 1000,
			    sizeof(float))) == NULL)
			exit(1);
		if ((((Echo *)ptr)->ringbuffer_R =
		     calloc(MAX_DELAY * ((Echo *)ptr)->sample_rate / 1000,
			    sizeof(float))) == NULL)
			exit(1);
		if ((((Echo *)ptr)->buffer_pos_L = calloc(1, sizeof(unsigned long))) == NULL)
			exit(1);
		if ((((Echo *)ptr)->buffer_pos_R = calloc(1, sizeof(unsigned long))) == NULL)
			exit(1);

		*(((Echo *)ptr)->buffer_pos_L) = 0;
		*(((Echo *)ptr)->buffer_pos_R) = 0;

		return ptr;
	}

	return NULL;
}


/* activate a plugin instance */
void
activate_Echo(LV2_Handle Instance) {

	Echo * ptr = (Echo *)Instance;
	int i;

	ptr->mpx_out_L = 0;
	ptr->mpx_out_R = 0;

	*(ptr->buffer_pos_L) = 0;
	*(ptr->buffer_pos_R) = 0;

	for (i = 0; i < MAX_DELAY * ptr->sample_rate / 1000; i++) {
		ptr->ringbuffer_L[i] = 0.0f;
		ptr->ringbuffer_R[i] = 0.0f;
	}
}


void
deactivate_Echo(LV2_Handle Instance) {


}


/* Connect a port to a data location. */
void
connect_port_Echo(LV2_Handle Instance,
		   uint32_t Port,
		   void * DataLocation) {

	Echo * ptr;

	ptr = (Echo *)Instance;
	switch (Port) {
	case DELAYTIME_L:
		ptr->delaytime_L = (float*) DataLocation;
		break;
	case DELAYTIME_R:
		ptr->delaytime_R = (float*) DataLocation;
		break;
	case FEEDBACK_L:
		ptr->feedback_L = (float*) DataLocation;
		break;
	case FEEDBACK_R:
		ptr->feedback_R = (float*) DataLocation;
		break;
	case STRENGTH_L:
		ptr->strength_L = (float*) DataLocation;
		break;
	case STRENGTH_R:
		ptr->strength_R = (float*) DataLocation;
		break;
	case MODE:
		ptr->mode = (float*) DataLocation;
		break;
	case HAAS:
		ptr->haas = (float*) DataLocation;
		break;
	case REV_OUTCH:
		ptr->rev_outch = (float*) DataLocation;
		break;
	case DRYLEVEL:
		ptr->drylevel = (float*) DataLocation;
		break;
	case INPUT_L:
		ptr->input_L = (float*) DataLocation;
		break;
	case OUTPUT_L:
		ptr->output_L = (float*) DataLocation;
		break;
	case INPUT_R:
		ptr->input_R = (float*) DataLocation;
		break;
	case OUTPUT_R:
		ptr->output_R = (float*) DataLocation;
		break;
	}
}


#define EPS 0.00000001f

static inline float
M(float x) {

        if ((x > EPS) || (x < -EPS))
                return x;
        else
                return 0.0f;
}

void
run_Echo(LV2_Handle Instance,
	 uint32_t SampleCount) {

	Echo * ptr;
	unsigned long sample_index;

	float delaytime_L;
	float delaytime_R;
	float feedback_L;
	float feedback_R;
	float strength_L;
	float strength_R;
	float drylevel;
	float mode;
	float haas;
	float rev_outch;

	float * input_L;
	float * output_L;
	float * input_R;
	float * output_R;

	unsigned long sample_rate;
	unsigned long buflen_L;
	unsigned long buflen_R;

	float out_L = 0;
	float out_R = 0;
	float in_L = 0;
	float in_R = 0;

	ptr = (Echo *)Instance;

	delaytime_L = LIMIT(*(ptr->delaytime_L),0.0f,2000.0f);
	delaytime_R = LIMIT(*(ptr->delaytime_R),0.0f,2000.0f);
	feedback_L = LIMIT(*(ptr->feedback_L) / 100.0, 0.0f, 100.0f);
	feedback_R = LIMIT(*(ptr->feedback_R) / 100.0, 0.0f, 100.0f);

	ptr->smoothstrength_L = (*(ptr->strength_L)+ptr->smoothstrength_L)*0.5; //smoothing
	strength_L = db2lin(LIMIT(ptr->smoothstrength_L,-70.0f,10.0f)); //convert to db and influence the actual audiobuffer

	ptr->smoothstrength_R = (*(ptr->strength_R)+ptr->smoothstrength_R)*0.5; //smoothing
	strength_R = db2lin(LIMIT(ptr->smoothstrength_R,-70.0f,10.0f)); //convert to db and influence the actual audiobuffer

	ptr->smoothdry = (*(ptr->drylevel)+ptr->smoothdry)*0.5; //smoothing
	drylevel = db2lin(LIMIT(ptr->smoothdry,-70.0f,10.0f));//convert to db and influence the actual audiobuffer

	mode = LIMIT(*(ptr->mode),-2.0f,2.0f);
	haas = LIMIT(*(ptr->haas),-2.0f,2.0f);
	rev_outch = LIMIT(*(ptr->rev_outch),-2.0f,2.0f);

      	input_L = ptr->input_L;
	output_L = ptr->output_L;
      	input_R = ptr->input_R;
	output_R = ptr->output_R;

	sample_rate = ptr->sample_rate;
	buflen_L = delaytime_L * sample_rate / 1000;
	buflen_R = delaytime_R * sample_rate / 1000;


	for (sample_index = 0; sample_index < SampleCount; sample_index++) {

		in_L = *(input_L++);
		in_R = *(input_R++);

		out_L = in_L * drylevel + ptr->mpx_out_L * strength_L;
		out_R = in_R * drylevel + ptr->mpx_out_R * strength_R;

		if (haas > 0.0f)
			in_R = 0.0f;

		if (mode <= 0.0f) {
			ptr->mpx_out_L =
				M(push_buffer(in_L + ptr->mpx_out_L * feedback_L,
					      ptr->ringbuffer_L, buflen_L, ptr->buffer_pos_L));
			ptr->mpx_out_R =
				M(push_buffer(in_R + ptr->mpx_out_R * feedback_R,
					      ptr->ringbuffer_R, buflen_R, ptr->buffer_pos_R));
		} else {
			ptr->mpx_out_R =
				M(push_buffer(in_L + ptr->mpx_out_L * feedback_L,
					      ptr->ringbuffer_L, buflen_L, ptr->buffer_pos_L));
			ptr->mpx_out_L =
				M(push_buffer(in_R + ptr->mpx_out_R * feedback_R,
					      ptr->ringbuffer_R, buflen_R, ptr->buffer_pos_R));
		}

		if (rev_outch <= 0.0f) {
			*(output_L++) = out_L;
			*(output_R++) = out_R;
		} else {
			*(output_L++) = out_R;
			*(output_R++) = out_L;
		}
	}
}


/* Throw away an Echo effect instance. */
void
cleanup_Echo(LV2_Handle Instance) {

	Echo * ptr = (Echo *)Instance;

	free(ptr->ringbuffer_L);
	free(ptr->ringbuffer_R);
	free(ptr->buffer_pos_L);
	free(ptr->buffer_pos_R);

	free(Instance);
}


const void*
extension_data_Echo(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/echo",
    instantiate_Echo,
    connect_port_Echo,
    activate_Echo,
    run_Echo,
    deactivate_Echo,
    cleanup_Echo,
    extension_data_Echo
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
