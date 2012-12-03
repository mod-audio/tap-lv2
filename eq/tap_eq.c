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

    $Id: tap_eq.c,v 1.8 2009/08/17 11:16:19 tszilagyi Exp $
*/


/* Please note that this plugin was inspired by and its code based
upon Steve Harris's "DJ EQ" plugin (no. 1901).  While I give him
credit for his excellent work, I reserve myself to be blamed for any
bugs or malfunction. */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"

/* The Unique ID of the plugin */
#define ID_MONO        2141


/* Bandwidth of EQ filters in octaves */
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

#define EQ_INPUT                    16
#define EQ_OUTPUT                   17


/* Total number of ports */
#define PORTCOUNT_MONO  18


//static LV2_Descriptor *eqDescriptor = NULL;

typedef struct {
	float *ch0f;
	float *ch0g;
	float *ch1f;
	float *ch1g;
	float *ch2f;
	float *ch2g;
	float *ch3f;
	float *ch3g;
	float *ch4f;
	float *ch4g;
	float *ch5f;
	float *ch5g;
	float *ch6f;
	float *ch6g;
	float *ch7f;
	float *ch7g;
	float *input;
	float *output;
	biquad *     filters;
	float        fs;
	float old_ch0f;
	float old_ch0g;
	float old_ch1f;
	float old_ch1g;
	float old_ch2f;
	float old_ch2g;
	float old_ch3f;
	float old_ch3g;
	float old_ch4f;
	float old_ch4g;
	float old_ch5f;
	float old_ch5g;
	float old_ch6f;
	float old_ch6g;
	float old_ch7f;
	float old_ch7g;
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
deactivate_eq(LV2_Handle instance) {


}




static
void
cleanup_eq(LV2_Handle instance) {

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
	case EQ_CH1F:
		plugin->ch1f = (float*) data;
		break;
	case EQ_CH1G:
		plugin->ch1g = (float*) data;
		break;
	case EQ_CH2F:
		plugin->ch2f = (float*) data;
		break;
	case EQ_CH2G:
		plugin->ch2g = (float*) data;
		break;
	case EQ_CH3F:
		plugin->ch3f = (float*) data;
		break;
	case EQ_CH3G:
		plugin->ch3g = (float*) data;
		break;
	case EQ_CH4F:
		plugin->ch4f = (float*) data;
		break;
	case EQ_CH4G:
		plugin->ch4g = (float*) data;
		break;
	case EQ_CH5F:
		plugin->ch5f = (float*) data;
		break;
	case EQ_CH5G:
		plugin->ch5g = (float*) data;
		break;
	case EQ_CH6F:
		plugin->ch6f = (float*) data;
		break;
	case EQ_CH6G:
		plugin->ch6g = (float*) data;
		break;
	case EQ_CH7F:
		plugin->ch7f = (float*) data;
		break;
	case EQ_CH7G:
		plugin->ch7g = (float*) data;
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
	ptr->old_ch0g = 0;

	ptr->old_ch1f = 200.0f;
	ptr->old_ch1g = 0;

	ptr->old_ch2f = 400.0f;
	ptr->old_ch2g = 0;

	ptr->old_ch3f = 1000.0f;
	ptr->old_ch3g = 0;

	ptr->old_ch4f = 3000.0f;
	ptr->old_ch4g = 0;

	ptr->old_ch5f = 6000.0f;
	ptr->old_ch5g = 0;

	ptr->old_ch6f = 12000.0f;
	ptr->old_ch6g = 0;

	ptr->old_ch7f = 15000.0f;
	ptr->old_ch7g = 0;

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
	const float ch1f = LIMIT(*(ptr->ch1f),100.0f,500.0f);
	const float ch1g = LIMIT(*(ptr->ch1g),-50.0f,20.0f);
	const float ch2f = LIMIT(*(ptr->ch2f),200.0f,1000.0f);
	const float ch2g = LIMIT(*(ptr->ch2g),-50.0f,20.0f);
	const float ch3f = LIMIT(*(ptr->ch3f),400.0f,2800.0f);
	const float ch3g = LIMIT(*(ptr->ch3g),-50.0f,20.0f);
	const float ch4f = LIMIT(*(ptr->ch4f),1000.0f,5000.0f);
	const float ch4g = LIMIT(*(ptr->ch4g),-50.0f,20.0f);
	const float ch5f = LIMIT(*(ptr->ch5f),3000.0f,9000.0f);
	const float ch5g = LIMIT(*(ptr->ch5g),-50.0f,20.0f);
	const float ch6f = LIMIT(*(ptr->ch6f),6000.0f,18000.0f);
	const float ch6g = LIMIT(*(ptr->ch6g),-50.0f,20.0f);
	const float ch7f = LIMIT(*(ptr->ch7f),10000.0f,20000.0f);
	const float ch7g = LIMIT(*(ptr->ch7g),-50.0f,20.0f);

	const float * input = ptr->input;
	float * output = ptr->output;

	biquad * filters = ptr->filters;
	float fs = ptr->fs;

	unsigned long pos;
	float samp;


	if ((ch0f != ptr->old_ch0f) ||
	    (ch0g != ptr->old_ch0g)) {
		ptr->old_ch0f = ch0f;
		ptr->old_ch0g = ch0g;
		eq_set_params(&filters[0], ch0f, ch0g, BWIDTH, fs);
	}
	if ((ch1f != ptr->old_ch1f) ||
	    (ch1g != ptr->old_ch1g)) {
		ptr->old_ch1f = ch1f;
		ptr->old_ch1g = ch1g;
		eq_set_params(&filters[1], ch1f, ch1g, BWIDTH, fs);
	}
	if ((ch2f != ptr->old_ch2f) ||
	    (ch2g != ptr->old_ch2g)) {
		ptr->old_ch2f = ch2f;
		ptr->old_ch2g = ch2g;
		eq_set_params(&filters[2], ch2f, ch2g, BWIDTH, fs);
	}
	if ((ch3f != ptr->old_ch3f) ||
	    (ch3g != ptr->old_ch3g)) {
		ptr->old_ch3f = ch3f;
		ptr->old_ch3g = ch3g;
		eq_set_params(&filters[3], ch3f, ch3g, BWIDTH, fs);
	}
	if ((ch4f != ptr->old_ch4f) ||
	    (ch4g != ptr->old_ch4g)) {
		ptr->old_ch4f = ch4f;
		ptr->old_ch4g = ch4g;
		eq_set_params(&filters[4], ch4f, ch4g, BWIDTH, fs);
	}
	if ((ch5f != ptr->old_ch5f) ||
	    (ch5g != ptr->old_ch5g)) {
		ptr->old_ch5f = ch5f;
		ptr->old_ch5g = ch5g;
		eq_set_params(&filters[5], ch5f, ch5g, BWIDTH, fs);
	}
	if ((ch6f != ptr->old_ch6f) ||
	    (ch6g != ptr->old_ch6g)) {
		ptr->old_ch6f = ch6f;
		ptr->old_ch6g = ch6g;
		eq_set_params(&filters[6], ch6f, ch6g, BWIDTH, fs);
	}
	if ((ch7f != ptr->old_ch7f) ||
	    (ch7g != ptr->old_ch7g)) {
		ptr->old_ch7f = ch7f;
		ptr->old_ch7g = ch7g;
		eq_set_params(&filters[7], ch7f, ch7g, BWIDTH, fs);
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
    "http://portalmod.com/plugins/tap/eq",
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
