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

    $Id: tap_chorusflanger.c,v 1.3 2004/08/17 09:15:21 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "lv2.h"
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_STEREO       2159

/* The port numbers for the plugin: */

#define FREQ            0
#define PHASE           1
#define DEPTH           2
#define DELAY           3
#define CONTOUR         4
#define DRYLEVEL        5
#define WETLEVEL        6
#define INPUT_L         7
#define INPUT_R         8
#define OUTPUT_L        9
#define OUTPUT_R       10

/* Total number of ports */
#define PORTCOUNT_STEREO 11


/*
 * Largest buffer lengths needed (at 192 kHz).
 * These are summed up to determine the size of *one* buffer per channel.
 */
#define DEPTH_BUFLEN 450
#define DELAY_BUFLEN 19200

/* Max. frequency setting */
#define MAX_FREQ 5.0f

/* bandwidth of highpass filters (in octaves) */
#define HP_BW 1

/* cosine table for fast computations */
#define COS_TABLE_SIZE 1024
float cos_table[COS_TABLE_SIZE];
int flag = 0;


/* The structure used to hold port connection information and state */
typedef struct {
	float * freq;
	float * phase;
	float smoothphase;
	float * depth;
	float smoothdepth;
	float * delay;
	float smoothdelay;
	float * contour;
	float * drylevel;
	float smoothdry;
	float * wetlevel;
	float smoothwet;
	float * input_L;
	float * input_R;
	float * output_L;
	float * output_R;

	float * ring_L;
	unsigned long buflen_L;
	unsigned long pos_L;
	float * ring_R;
	unsigned long buflen_R;
	unsigned long pos_R;

	biquad highpass_L;
	biquad highpass_R;

	float cm_phase;
	float dm_phase;

	double sample_rate;
} ChorusFlanger;


/* Construct a new plugin instance. */
LV2_Handle
instantiate_ChorusFlanger(const LV2_Descriptor * Descriptor, double sample_rate, const char* bundle_path, const LV2_Feature* const* features)
 {

        LV2_Handle * ptr;
        int i;

	if ((ptr = malloc(sizeof(ChorusFlanger))) != NULL) {
		((ChorusFlanger *)ptr)->sample_rate = sample_rate;
		((ChorusFlanger *)ptr)->smoothdry = -3.0;
		((ChorusFlanger *)ptr)->smoothwet = -3.0;
		((ChorusFlanger *)ptr)->smoothphase = 90.0;
		((ChorusFlanger *)ptr)->smoothdepth = 75.0;
		((ChorusFlanger *)ptr)->smoothdelay = 25.0;

		const unsigned long fullbuflen = (DEPTH_BUFLEN + DELAY_BUFLEN) * sample_rate / 192000;

		if ((((ChorusFlanger *)ptr)->ring_L = calloc(fullbuflen, sizeof(float))) == NULL)
			return NULL;
		((ChorusFlanger *)ptr)->buflen_L = fullbuflen;
		((ChorusFlanger *)ptr)->pos_L = 0;

		if ((((ChorusFlanger *)ptr)->ring_R = calloc(fullbuflen, sizeof(float))) == NULL)
			return NULL;
		((ChorusFlanger *)ptr)->buflen_R = fullbuflen;
		((ChorusFlanger *)ptr)->pos_R = 0;


		((ChorusFlanger *)ptr)->cm_phase = 0.0f;
		((ChorusFlanger *)ptr)->dm_phase = 0.0f;

		if(flag == 0)
		{
		   for (i = 0; i < COS_TABLE_SIZE; i++)
                cos_table[i] = cosf(i * 2.0f * M_PI / COS_TABLE_SIZE);
            flag++;
		}


		return ptr;
	}
       	return NULL;
}


void
activate_ChorusFlanger(LV2_Handle Instance) {

	ChorusFlanger * ptr = (ChorusFlanger *)Instance;

	memset(ptr->ring_L, 0, sizeof(float)*ptr->buflen_L);
	memset(ptr->ring_R, 0, sizeof(float)*ptr->buflen_R);

	biquad_init(&ptr->highpass_L);
	biquad_init(&ptr->highpass_R);
}

void
deactivate_ChorusFlanger(LV2_Handle Instance) {


}

/* Connect a port to a data location. */
void
connect_port_ChorusFlanger(LV2_Handle Instance,
			   uint32_t Port,
			   void* data) {

	ChorusFlanger * ptr = (ChorusFlanger *)Instance;

	switch (Port) {
	case FREQ:
		ptr->freq = (float*) data;
		break;
	case PHASE:
		ptr->phase = (float*) data;
		break;
	case DEPTH:
		ptr->depth = (float*) data;
		break;
	case DELAY:
		ptr->delay = (float*) data;
		break;
	case CONTOUR:
		ptr->contour = (float*) data;
		break;
	case DRYLEVEL:
		ptr->drylevel = (float*) data;
		break;
	case WETLEVEL:
		ptr->wetlevel = (float*) data;
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
run_ChorusFlanger(LV2_Handle Instance,
		  uint32_t SampleCount) {

	ChorusFlanger * ptr = (ChorusFlanger *)Instance;

	float freq = LIMIT(*(ptr->freq), 0.0f, MAX_FREQ);

	float calcphase = (*(ptr->phase)+ptr->smoothphase)*0.5;
	ptr->smoothphase=calcphase;
	float phase = LIMIT(calcphase, 0.0f, 180.0f) / 180.0f;

	float calcdepth = (*(ptr->depth)+ptr->smoothdepth)*0.5;
	ptr->smoothdepth=calcdepth;
	float depth = 100.0f * ptr->sample_rate / 44100.0f
		* LIMIT(calcdepth,0.0f,100.0f) / 100.0f;

	float calcdelay = (*(ptr->delay)+ptr->smoothdelay)*0.5;
	ptr->smoothdelay=calcdelay;
	float delay = LIMIT(calcdelay,0.0f,100.0f);
	float contour = LIMIT(*(ptr->contour), 20.0f, 20000.0f);

	float calcdry = (*(ptr->drylevel)+ptr->smoothdry)*0.5;
	ptr->smoothdry=calcdry;
	float drylevel = db2lin(LIMIT(calcdry,-90.0f,20.0f));

	float calcwet = (*(ptr->wetlevel)+ptr->smoothwet)*0.5;
	ptr->smoothwet=calcwet;
	float wetlevel = db2lin(LIMIT(calcwet,-90.0f,20.0f));
	float * input_L = ptr->input_L;
	float * input_R = ptr->input_R;
	float * output_L = ptr->output_L;
	float * output_R = ptr->output_R;

	unsigned long sample_index;
	unsigned long sample_count = SampleCount;

	float in_L = 0.0f;
	float in_R = 0.0f;
	float d_L = 0.0f;
	float d_R = 0.0f;
	float f_L = 0.0f;
	float f_R = 0.0f;
	float out_L = 0.0f;
	float out_R = 0.0f;

	float phase_L = 0.0f;
	float phase_R = 0.0f;
	float fpos_L = 0.0f;
	float fpos_R = 0.0f;
	float n_L = 0.0f;
	float n_R = 0.0f;
	float rem_L = 0.0f;
	float rem_R = 0.0f;
	float s_a_L, s_a_R, s_b_L, s_b_R;

	float d_pos = 0.0f;

	if (delay < 1.0f)
		delay = 1.0f;
	delay = 100.0f - delay;

	hp_set_params(&ptr->highpass_L, contour, HP_BW, ptr->sample_rate);
	hp_set_params(&ptr->highpass_R, contour, HP_BW, ptr->sample_rate);

	for (sample_index = 0; sample_index < sample_count; sample_index++) {

		in_L = *(input_L++);
		in_R = *(input_R++);

		push_buffer(in_L, ptr->ring_L, ptr->buflen_L, &(ptr->pos_L));
		push_buffer(in_R, ptr->ring_R, ptr->buflen_R, &(ptr->pos_R));

		ptr->cm_phase += freq / ptr->sample_rate * COS_TABLE_SIZE;

		while (ptr->cm_phase >= COS_TABLE_SIZE)
			ptr->cm_phase -= COS_TABLE_SIZE;

		ptr->dm_phase = phase * COS_TABLE_SIZE / 2.0f;

		phase_L = ptr->cm_phase;
		phase_R = ptr->cm_phase + ptr->dm_phase;
		while (phase_R >= COS_TABLE_SIZE)
			phase_R -= COS_TABLE_SIZE;

		d_pos = delay * ptr->sample_rate / 1000.0f;
		fpos_L = d_pos + depth * (0.5f + 0.5f * cos_table[(unsigned long)phase_L]);
		fpos_R = d_pos + depth * (0.5f + 0.5f * cos_table[(unsigned long)phase_R]);

		n_L = floorf(fpos_L);
		n_R = floorf(fpos_R);
		rem_L = fpos_L - n_L;
		rem_R = fpos_R - n_R;

		s_a_L = read_buffer(ptr->ring_L, ptr->buflen_L,
				    ptr->pos_L, (unsigned long) n_L);
		s_b_L = read_buffer(ptr->ring_L, ptr->buflen_L,
				    ptr->pos_L, (unsigned long) n_L + 1);

		s_a_R = read_buffer(ptr->ring_R, ptr->buflen_R,
				    ptr->pos_R, (unsigned long) n_R);
		s_b_R = read_buffer(ptr->ring_R, ptr->buflen_R,
				    ptr->pos_R, (unsigned long) n_R + 1);

		d_L = ((1 - rem_L) * s_a_L + rem_L * s_b_L);
		d_R = ((1 - rem_R) * s_a_R + rem_R * s_b_R);

		f_L = biquad_run(&ptr->highpass_L, d_L);
		f_R = biquad_run(&ptr->highpass_R, d_R);

		out_L = drylevel * in_L + wetlevel * f_L;
		out_R = drylevel * in_R + wetlevel * f_R;

		*(output_L++) = out_L;
		*(output_R++) = out_R;
	}
}


/* Throw away a ChorusFlanger effect instance. */
void
cleanup_ChorusFlanger(LV2_Handle Instance) {

  	ChorusFlanger * ptr = (ChorusFlanger *)Instance;
	free(ptr->ring_L);
	free(ptr->ring_R);
	free(Instance);
}

const void*
extension_data_ChorusFlanger(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/chorusflanger",
    instantiate_ChorusFlanger,
    connect_port_ChorusFlanger,
    activate_ChorusFlanger,
    run_ChorusFlanger,
    deactivate_ChorusFlanger,
    cleanup_ChorusFlanger,
    extension_data_ChorusFlanger
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
