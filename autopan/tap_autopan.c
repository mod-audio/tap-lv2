/*                                                     -*- linux-c -*-
    Copyright (C) 2004 Tom Szilagyi
    Patches were received from:
        Alexander Koenig <alex@lisas.de>
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
    $Id: tap_autopan.c,v 1.6 2004/02/21 17:33:36 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "lv2.h"
#include "tap_utils.h"

/* The Unique ID of the plugin: */

#define ID_STEREO         2146

/* The port numbers for the plugin: */

#define CONTROL_FREQ    0
#define CONTROL_DEPTH   1
#define CONTROL_GAIN    2
#define INPUT_L         3
#define INPUT_R         4
#define OUTPUT_L        5
#define OUTPUT_R        6


/* Total number of ports */

#define PORTCOUNT_STEREO   7


/* cosine table for fast computations */
float cos_table[1024];
int flag = 0;


/* The structure used to hold port connection information and state */

typedef struct {
	float * freq;
	float * depth;
	float * gain;
	float * input_L;
	float * input_R;
	float * output_L;
	float * output_R;
	double SampleRate;
	float Phase;
	float Ogain;
} AutoPan;



/* Construct a new plugin instance. */
LV2_Handle
instantiate_AutoPan(const LV2_Descriptor * Descriptor,double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

	LV2_Handle * ptr;
	int i;

	if ((ptr = malloc(sizeof(AutoPan))) != NULL) {
		((AutoPan *)ptr)->SampleRate = SampleRate;
		((AutoPan *)ptr)->Ogain = 0.0f;
            if(flag == 0)
	        {
                for (i = 0; i < 1024; i++)
                    cos_table[i] = cosf(i * M_PI / 512.0f);
            flag++;
	        }
		return ptr;
	}

	return NULL;
}

void
activate_AutoPan(LV2_Handle Instance) {

	AutoPan * ptr;

	ptr = (AutoPan *)Instance;
	ptr->Phase = 0.0f;
}

void
deactivate_AutoPan(LV2_Handle Instance) {


}


/* Connect a port to a data location. */
void
connect_port_AutoPan(LV2_Handle Instance,
		     uint32_t Port,
		     void * DataLocation) {

	AutoPan * ptr;

	ptr = (AutoPan *)Instance;
	switch (Port) {
	case CONTROL_FREQ:
		ptr->freq = (float*) DataLocation;
		break;
	case CONTROL_DEPTH:
		ptr->depth = (float*) DataLocation;
		break;
	case CONTROL_GAIN:
		ptr->gain = (float*) DataLocation;
		break;
	case INPUT_L:
		ptr->input_L = (float*) DataLocation;
		break;
	case INPUT_R:
		ptr->input_R = (float*) DataLocation;
		break;
	case OUTPUT_L:
		ptr->output_L = (float*) DataLocation;
		break;
	case OUTPUT_R:
		ptr->output_R = (float*) DataLocation;
		break;
	}
}



void
run_AutoPan(LV2_Handle Instance,
	    uint32_t SampleCount) {

	AutoPan * ptr = (AutoPan *)Instance;

	float * input_L = ptr->input_L;
	float * input_R = ptr->input_R;
	float * output_L = ptr->output_L;
	float * output_R = ptr->output_R;
	float freq = LIMIT(*(ptr->freq),0.0f,20.0f);
	float depth = LIMIT(*(ptr->depth),0.0f,100.0f);
	float gain = (db2lin(LIMIT(*(ptr->gain),-70.0f,20.0f))+ptr->Ogain)*0.5;
	ptr->Ogain = gain;
	unsigned long sample_index;
	float phase_L = 0;
	float phase_R = 0;


	for (sample_index = 0; sample_index < SampleCount; sample_index++) {
		phase_L = 1024.0f * freq * sample_index / ptr->SampleRate + ptr->Phase;
		while (phase_L >= 1024.0f)
		        phase_L -= 1024.0f;
 		phase_R = phase_L + 512.0f;
		while (phase_R >= 1024.0f)
		        phase_R -= 1024.0f;

		*(output_L++) = *(input_L++) * gain *
			(1 - 0.5*depth/100 + 0.5 * depth/100 * cos_table[(unsigned long) phase_L]);
		*(output_R++) = *(input_R++) * gain *
			(1 - 0.5*depth/100 + 0.5 * depth/100 * cos_table[(unsigned long) phase_R]);
	}
	ptr->Phase = phase_L;
	while (ptr->Phase >= 1024.0f)
		ptr->Phase -= 1024.0f;
}


/* Throw away an AutoPan effect instance. */
void
cleanup_AutoPan(LV2_Handle Instance) {
	free(Instance);
}

const void*
extension_data_AutoPan(const char* uri)
{
    return NULL;
}

static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/autopan",
    instantiate_AutoPan,
    connect_port_AutoPan,
    activate_AutoPan,
    run_AutoPan,
    deactivate_AutoPan,
    cleanup_AutoPan,
    extension_data_AutoPan
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
