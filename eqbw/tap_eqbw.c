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

    $Id: tap_eqbw.c,v 1.6 2009/08/17 11:16:19 tszilagyi Exp $
*/


/* This plugin is identical to TAP Equalizer (2141), but it has
 * separate user controls for setting the bandwidth of every filter.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin */
#define ID_MONO        2151


/* Default bandwidth of EQ filters in octaves */
#define BWIDTH        1.0f


/* Port numbers */

#define EQ_CH0G                     0
#define EQ_CH1G                     1
#define EQ_CH2G                     2
#define EQ_CH3G                     3
#define EQ_CH4G                     4
#define EQ_CH5G                     5
#define EQ_CH6G                     6
#define EQ_CH7G                     7

#define EQ_CH0F                     8
#define EQ_CH1F                     9
#define EQ_CH2F                     10
#define EQ_CH3F                     11
#define EQ_CH4F                     12
#define EQ_CH5F                     13
#define EQ_CH6F                     14
#define EQ_CH7F                     15

#define EQ_CH0B                     16
#define EQ_CH1B                     17
#define EQ_CH2B                     18
#define EQ_CH3B                     19
#define EQ_CH4B                     20
#define EQ_CH5B                     21
#define EQ_CH6B                     22
#define EQ_CH7B                     23

#define EQ_INPUT                    24
#define EQ_OUTPUT                   25


/* Total number of ports */
#define PORTCOUNT_MONO  26

typedef struct {
	float *ch0f;
	float *ch0g;
	float *ch0b;
	float *ch1f;
	float *ch1g;
	float *ch1b;
	float *ch2f;
	float *ch2g;
	float *ch2b;
	float *ch3f;
	float *ch3g;
	float *ch3b;
	float *ch4f;
	float *ch4g;
	float *ch4b;
	float *ch5f;
	float *ch5g;
	float *ch5b;
	float *ch6f;
	float *ch6g;
	float *ch6b;
	float *ch7f;
	float *ch7g;
	float *ch7b;
	float *input;
	float *output;
	biquad *     filters;
	float        fs;
	float old_ch0f;
	float old_ch0g;
	float old_ch0b;
	float old_ch1f;
	float old_ch1g;
	float old_ch1b;
	float old_ch2f;
	float old_ch2g;
	float old_ch2b;
	float old_ch3f;
	float old_ch3g;
	float old_ch3b;
	float old_ch4f;
	float old_ch4g;
	float old_ch4b;
	float old_ch5f;
	float old_ch5g;
	float old_ch5b;
	float old_ch6f;
	float old_ch6g;
	float old_ch6b;
	float old_ch7f;
	float old_ch7g;
	float old_ch7b;
} eq;


static
void
activate_eq(LV2_Handle instance) {

	eq *ptr = (eq *)instance;
	biquad *filters = ptr->filters;

	biquad_init(&filters[0]);
	biquad_init(&filters[1]);
	biquad_init(&filters[2]);
	biquad_init(&filters[3]);
	biquad_init(&filters[4]);
	biquad_init(&filters[5]);
	biquad_init(&filters[6]);
	biquad_init(&filters[7]);
}

void
deactivate_eq(LV2_Handle Instance) {


}

static
void
cleanup_eq(LV2_Handle instance) {

	eq *ptr = (eq *)instance;
	free(ptr->filters);
	free(instance);
}


static
void
connectPort_eq(LV2_Handle instance, uint32_t port, void *data) {

	eq *plugin;

	plugin = (eq *)instance;
	switch (port) {
	case EQ_CH0F:
		plugin->ch0f = (float*) data;
		break;
	case EQ_CH0G:
		plugin->ch0g = (float*) data;
		break;
	case EQ_CH0B:
		plugin->ch0b = (float*) data;
		break;
	case EQ_CH1F:
		plugin->ch1f = (float*) data;
		break;
	case EQ_CH1G:
		plugin->ch1g = (float*) data;
		break;
	case EQ_CH1B:
		plugin->ch1b = (float*) data;
		break;
	case EQ_CH2F:
		plugin->ch2f = (float*) data;
		break;
	case EQ_CH2G:
		plugin->ch2g = (float*) data;
		break;
	case EQ_CH2B:
		plugin->ch2b = (float*) data;
		break;
	case EQ_CH3F:
		plugin->ch3f = (float*) data;
		break;
	case EQ_CH3G:
		plugin->ch3g = (float*) data;
		break;
	case EQ_CH3B:
		plugin->ch3b = (float*) data;
		break;
	case EQ_CH4F:
		plugin->ch4f = (float*) data;
		break;
	case EQ_CH4G:
		plugin->ch4g = (float*) data;
		break;
	case EQ_CH4B:
		plugin->ch4b = (float*) data;
		break;
	case EQ_CH5F:
		plugin->ch5f = (float*) data;
		break;
	case EQ_CH5G:
		plugin->ch5g = (float*) data;
		break;
	case EQ_CH5B:
		plugin->ch5b = (float*) data;
		break;
	case EQ_CH6F:
		plugin->ch6f = (float*) data;
		break;
	case EQ_CH6G:
		plugin->ch6g = (float*) data;
		break;
	case EQ_CH6B:
		plugin->ch6b = (float*) data;
		break;
	case EQ_CH7F:
		plugin->ch7f = (float*) data;
		break;
	case EQ_CH7G:
		plugin->ch7g = (float*) data;
		break;
	case EQ_CH7B:
		plugin->ch7b = (float*) data;
		break;
	case EQ_INPUT:
		plugin->input = (float*) data;
		break;
	case EQ_OUTPUT:
		plugin->output = (float*) data;
		break;
	}
}

static
LV2_Handle
instantiate_eq(const LV2_Descriptor *descriptor, double s_rate, const char* bundle_path, const LV2_Feature* const* features) {

	eq *ptr = (eq *)malloc(sizeof(eq));
	biquad *filters = NULL;
	float fs;

	fs = s_rate;

	memset(ptr, 0, sizeof(eq));

	filters = calloc(8, sizeof(biquad));

	ptr->filters = filters;
	ptr->fs = fs;

	ptr->old_ch0f = 100.0f;
	ptr->old_ch0g = 0.0f;
	ptr->old_ch0b = BWIDTH;

	ptr->old_ch1f = 200.0f;
	ptr->old_ch1g = 0.0f;
	ptr->old_ch1b = BWIDTH;

	ptr->old_ch2f = 400.0f;
	ptr->old_ch2g = 0.0f;
	ptr->old_ch2b = BWIDTH;

	ptr->old_ch3f = 1000.0f;
	ptr->old_ch3g = 0.0f;
	ptr->old_ch3b = BWIDTH;

	ptr->old_ch4f = 3000.0f;
	ptr->old_ch4g = 0.0f;
	ptr->old_ch4b = BWIDTH;

	ptr->old_ch5f = 6000.0f;
	ptr->old_ch5g = 0.0f;
	ptr->old_ch5b = BWIDTH;

	ptr->old_ch6f = 12000.0f;
	ptr->old_ch6g = 0.0f;
	ptr->old_ch6b = BWIDTH;

	ptr->old_ch7f = 15000.0f;
	ptr->old_ch7g = 0.0f;
	ptr->old_ch7b = BWIDTH;

	eq_set_params(&filters[0], 100.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[1], 200.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[2], 400.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[3], 1000.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[4], 3000.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[5], 6000.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[6], 12000.0f, 0.0f, BWIDTH, fs);
	eq_set_params(&filters[7], 15000.0f, 0.0f, BWIDTH, fs);

	return (LV2_Handle)ptr;
}


static
void
run_eq(LV2_Handle instance, uint32_t sample_count) {

	eq * ptr = (eq *)instance;

	const float ch0f = LIMIT(*(ptr->ch0f),40.0f,280.0f);
	const float ch0g = LIMIT(*(ptr->ch0g),-50.0f,20.0f);
	const float ch0b = LIMIT(*(ptr->ch0b),0.1f,5.0f);
	const float ch1f = LIMIT(*(ptr->ch1f),100.0f,500.0f);
	const float ch1g = LIMIT(*(ptr->ch1g),-50.0f,20.0f);
	const float ch1b = LIMIT(*(ptr->ch1b),0.1f,5.0f);
	const float ch2f = LIMIT(*(ptr->ch2f),200.0f,1000.0f);
	const float ch2g = LIMIT(*(ptr->ch2g),-50.0f,20.0f);
	const float ch2b = LIMIT(*(ptr->ch2b),0.1f,5.0f);
	const float ch3f = LIMIT(*(ptr->ch3f),400.0f,2800.0f);
	const float ch3g = LIMIT(*(ptr->ch3g),-50.0f,20.0f);
	const float ch3b = LIMIT(*(ptr->ch3b),0.1f,5.0f);
	const float ch4f = LIMIT(*(ptr->ch4f),1000.0f,5000.0f);
	const float ch4g = LIMIT(*(ptr->ch4g),-50.0f,20.0f);
	const float ch4b = LIMIT(*(ptr->ch4b),0.1f,5.0f);
	const float ch5f = LIMIT(*(ptr->ch5f),3000.0f,9000.0f);
	const float ch5g = LIMIT(*(ptr->ch5g),-50.0f,20.0f);
	const float ch5b = LIMIT(*(ptr->ch5b),0.1f,5.0f);
	const float ch6f = LIMIT(*(ptr->ch6f),6000.0f,18000.0f);
	const float ch6g = LIMIT(*(ptr->ch6g),-50.0f,20.0f);
	const float ch6b = LIMIT(*(ptr->ch6b),0.1f,5.0f);
	const float ch7f = LIMIT(*(ptr->ch7f),10000.0f,20000.0f);
	const float ch7g = LIMIT(*(ptr->ch7g),-50.0f,20.0f);
	const float ch7b = LIMIT(*(ptr->ch7b),0.1f,5.0f);

	const float * input = ptr->input;
	float * output = ptr->output;

	biquad * filters = ptr->filters;
	float fs = ptr->fs;

	unsigned long pos;
	float samp;


	if ((ch0f != ptr->old_ch0f) ||
	    (ch0g != ptr->old_ch0g) ||
	    (ch0b != ptr->old_ch0b)) {
		ptr->old_ch0f = ch0f;
		ptr->old_ch0g = ch0g;
		ptr->old_ch0b = ch0b;
		eq_set_params(&filters[0], ch0f, ch0g, ch0b, fs);
	}
	if ((ch1f != ptr->old_ch1f) ||
	    (ch1g != ptr->old_ch1g) ||
	    (ch1b != ptr->old_ch1b)) {
		ptr->old_ch1f = ch1f;
		ptr->old_ch1g = ch1g;
		ptr->old_ch1b = ch1b;
		eq_set_params(&filters[1], ch1f, ch1g, ch1b, fs);
	}
	if ((ch2f != ptr->old_ch2f) ||
	    (ch2g != ptr->old_ch2g) ||
	    (ch2b != ptr->old_ch2b)) {
		ptr->old_ch2f = ch2f;
		ptr->old_ch2g = ch2g;
		ptr->old_ch2b = ch2b;
		eq_set_params(&filters[2], ch2f, ch2g, ch2b, fs);
	}
	if ((ch3f != ptr->old_ch3f) ||
	    (ch3g != ptr->old_ch3g) ||
	    (ch3b != ptr->old_ch3b)) {
		ptr->old_ch3f = ch3f;
		ptr->old_ch3g = ch3g;
		ptr->old_ch3b = ch3b;
		eq_set_params(&filters[3], ch3f, ch3g, ch3b, fs);
	}
	if ((ch4f != ptr->old_ch4f) ||
	    (ch4g != ptr->old_ch4g) ||
	    (ch4b != ptr->old_ch4b)) {
		ptr->old_ch4f = ch4f;
		ptr->old_ch4g = ch4g;
		ptr->old_ch4b = ch4b;
		eq_set_params(&filters[4], ch4f, ch4g, ch4b, fs);
	}
	if ((ch5f != ptr->old_ch5f) ||
	    (ch5g != ptr->old_ch5g) ||
	    (ch5b != ptr->old_ch5b)) {
		ptr->old_ch5f = ch5f;
		ptr->old_ch5g = ch5g;
		ptr->old_ch5b = ch5b;
		eq_set_params(&filters[5], ch5f, ch5g, ch5b, fs);
	}
	if ((ch6f != ptr->old_ch6f) ||
	    (ch6g != ptr->old_ch6g) ||
	    (ch6b != ptr->old_ch6b)) {
		ptr->old_ch6f = ch6f;
		ptr->old_ch6g = ch6g;
		ptr->old_ch6b = ch6b;
		eq_set_params(&filters[6], ch6f, ch6g, ch6b, fs);
	}
	if ((ch7f != ptr->old_ch7f) ||
	    (ch7g != ptr->old_ch7g) ||
	    (ch7b != ptr->old_ch7b)) {
		ptr->old_ch7f = ch7f;
		ptr->old_ch7g = ch7g;
		ptr->old_ch7b = ch7b;
		eq_set_params(&filters[7], ch7f, ch7g, ch7b, fs);
	}

	for (pos = 0; pos < sample_count; pos++) {
		samp = input[pos];
		if (ch0g != 0.0f)
			samp = biquad_run(&filters[0], samp);
		if (ch1g != 0.0f)
			samp = biquad_run(&filters[1], samp);
		if (ch2g != 0.0f)
			samp = biquad_run(&filters[2], samp);
		if (ch3g != 0.0f)
			samp = biquad_run(&filters[3], samp);
		if (ch4g != 0.0f)
			samp = biquad_run(&filters[4], samp);
		if (ch5g != 0.0f)
			samp = biquad_run(&filters[5], samp);
		if (ch6g != 0.0f)
			samp = biquad_run(&filters[6], samp);
		if (ch7g != 0.0f)
			samp = biquad_run(&filters[7], samp);
		output[pos] = samp;
	}
}

const void*
extension_data_eq(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/eqbw",
    instantiate_eq,
    connectPort_eq,
    activate_eq,
    run_eq,
    deactivate_eq,
    cleanup_eq,
    extension_data_eq
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    if (index == 0) return &Descriptor;
    else return NULL;

}
