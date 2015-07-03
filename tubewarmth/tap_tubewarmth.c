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

    $Id: tap_tubewarmth.c,v 1.1 2004/08/02 18:14:50 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_MONO         2158

/* The port numbers for the plugin: */

#define DRIVE            0
#define BLEND            1
#define INPUT            2
#define OUTPUT           3

/* Total number of ports */


#define PORTCOUNT_MONO   4


/* The structure used to hold port connection information and state */

typedef struct {
    float * drive;
    float * blend;
    float * input;
    float * output;

    float prev_med;
    float prev_out;

    float rdrive;
    float rbdr;
    float kpa;
    float kpb;
    float kna;
    float knb;
    float ap;
    float an;
    float imr;
    float kc;
    float srct;
    float sq;
    float pwrq;

    float prev_drive;
    float prev_blend;

    unsigned long sample_rate;
    float run_adding_gain;
} TubeWarmth;



/* Construct a new plugin instance. */
LV2_Handle
instantiate_TubeWarmth(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

        LV2_Handle * ptr;

    if ((ptr = malloc(sizeof(TubeWarmth))) != NULL) {
        ((TubeWarmth *)ptr)->sample_rate = SampleRate;
        ((TubeWarmth *)ptr)->run_adding_gain = 1.0f;

        ((TubeWarmth *)ptr)->prev_med = 0.0f;
        ((TubeWarmth *)ptr)->prev_out = 0.0f;

        ((TubeWarmth *)ptr)->rdrive = 0.0f;
        ((TubeWarmth *)ptr)->rbdr = 0.0f;
        ((TubeWarmth *)ptr)->kpa = 0.0f;
        ((TubeWarmth *)ptr)->kpb = 0.0f;
        ((TubeWarmth *)ptr)->kna = 0.0f;
        ((TubeWarmth *)ptr)->knb = 0.0f;
        ((TubeWarmth *)ptr)->ap = 0.0f;
        ((TubeWarmth *)ptr)->an = 0.0f;
        ((TubeWarmth *)ptr)->imr = 0.0f;
        ((TubeWarmth *)ptr)->kc = 0.0f;
        ((TubeWarmth *)ptr)->srct = 0.0f;
        ((TubeWarmth *)ptr)->sq = 0.0f;
        ((TubeWarmth *)ptr)->pwrq = 0.0f;

                /* These are out of band to force param recalc upon first run() */
        ((TubeWarmth *)ptr)->prev_drive = -1.0f;
        ((TubeWarmth *)ptr)->prev_blend = -11.0f;

        return ptr;
    }
        return NULL;
}


void
activate_TubeWarmth(LV2_Handle Instance) {
}

void
deactivate_TubeWarmth(LV2_Handle Instance) {
}


/* Connect a port to a data location. */
void
connect_port_TubeWarmth(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    TubeWarmth * ptr = (TubeWarmth *)Instance;

    switch (Port) {
    case DRIVE:
        ptr->drive = (float *) DataLocation;
        break;
    case BLEND:
        ptr->blend = (float *) DataLocation;
        break;
    case INPUT:
        ptr->input = (float *) DataLocation;
        break;
    case OUTPUT:
        ptr->output = ( float *) DataLocation;
        break;
    }
}


#define EPS 0.000000001f

static inline float
M(float x) {

    if ((x > EPS) || (x < -EPS))
        return x;
    else
        return 0.0f;
}

static inline float
D(float x) {

    if (x > EPS)
        return sqrt(x);
    else if (x < -EPS)
        return sqrt(-x);
    else
        return 0.0f;
}

void
run_TubeWarmth(LV2_Handle Instance,
           uint32_t SampleCount) {

    TubeWarmth * ptr = (TubeWarmth *)Instance;
    float * input = ptr->input;
    float * output = ptr->output;
    float drive = LIMIT(*(ptr->drive),0.1f,10.0f);
    float blend = LIMIT(*(ptr->blend),-10.0f,10.0f);

    unsigned long sample_index;
    unsigned long sample_count = SampleCount;
    unsigned long sample_rate = ptr->sample_rate;

    float rdrive = ptr->rdrive;
    float rbdr = ptr->rbdr;
    float kpa = ptr->kpa;
    float kpb = ptr->kpb;
    float kna = ptr->kna;
    float knb = ptr->knb;
    float ap = ptr->ap;
    float an = ptr->an;
    float imr = ptr->imr;
    float kc = ptr->kc;
    float srct = ptr->srct;
    float sq = ptr->sq;
    float pwrq = ptr->pwrq;

    float prev_med;
    float prev_out;
    float in;
    float med;
    float out;

    if ((ptr->prev_drive != drive) || (ptr->prev_blend != blend)) {

        rdrive = 12.0f / drive;
        rbdr = rdrive / (10.5f - blend) * 780.0f / 33.0f;
        kpa = D(2.0f * (rdrive*rdrive) - 1.0f) + 1.0f;
        kpb = (2.0f - kpa) / 2.0f;
        ap = ((rdrive*rdrive) - kpa + 1.0f) / 2.0f;
        kc = kpa / D(2.0f * D(2.0f * (rdrive*rdrive) - 1.0f) - 2.0f * rdrive*rdrive);

        srct = (0.1f * sample_rate) / (0.1f * sample_rate + 1.0f);
        sq = kc*kc + 1.0f;
        knb = -1.0f * rbdr / D(sq);
        kna = 2.0f * kc * rbdr / D(sq);
        an = rbdr*rbdr / sq;
        imr = 2.0f * knb + D(2.0f * kna + 4.0f * an - 1.0f);
        pwrq = 2.0f / (imr + 1.0f);

        ptr->prev_drive = drive;
        ptr->prev_blend = blend;
    }

    for (sample_index = 0; sample_index < sample_count; sample_index++) {

        in = *(input++);
        prev_med = ptr->prev_med;
        prev_out = ptr->prev_out;

        if (in >= 0.0f) {
            med = (D(ap + in * (kpa - in)) + kpb) * pwrq;
        } else {
            med = (D(an - in * (kna + in)) + knb) * pwrq * -1.0f;
        }

        out = srct * (med - prev_med + prev_out);

        if (out < -1.0f)
            out = -1.0f;

        *(output++) = out;

        ptr->prev_med = M(med);
        ptr->prev_out = M(out);
    }

    ptr->rdrive = rdrive;
    ptr->rbdr = rbdr;
    ptr->kpa = kpa;
    ptr->kpb = kpb;
    ptr->kna = kna;
    ptr->knb = knb;
    ptr->ap = ap;
    ptr->an = an;
    ptr->imr = imr;
    ptr->kc = kc;
    ptr->srct = srct;
    ptr->sq = sq;
    ptr->pwrq = pwrq;
}



//void
//set_run_adding_gain_TubeWarmth(LV2_Handle Instance, float gain) {

    //TubeWarmth * ptr = (TubeWarmth *)Instance;

    //ptr->run_adding_gain = gain;
//}



//void
//run_adding_TubeWarmth(LV2_Handle Instance,
              //unsigned long SampleCount) {

    //TubeWarmth * ptr = (TubeWarmth *)Instance;
    //float * input = ptr->input;
    //float * output = ptr->output;
    //float drive = LIMIT(*(ptr->drive),0.1f,10.0f);
    //float blend = LIMIT(*(ptr->blend),-10.0f,10.0f);

    //unsigned long sample_index;
    //unsigned long sample_count = SampleCount;
    //unsigned long sample_rate = ptr->sample_rate;

    //float rdrive = ptr->rdrive;
    //float rbdr = ptr->rbdr;
    //float kpa = ptr->kpa;
    //float kpb = ptr->kpb;
    //float kna = ptr->kna;
    //float knb = ptr->knb;
    //float ap = ptr->ap;
    //float an = ptr->an;
    //float imr = ptr->imr;
    //float kc = ptr->kc;
    //float srct = ptr->srct;
    //float sq = ptr->sq;
    //float pwrq = ptr->pwrq;

    //float prev_med;
    //float prev_out;
    //float in;
    //float med;
    //float out;

    //if ((ptr->prev_drive != drive) || (ptr->prev_blend != blend)) {

        //rdrive = 12.0f / drive;
        //rbdr = rdrive / (10.5f - blend) * 780.0f / 33.0f;
        //kpa = D(2.0f * (rdrive*rdrive) - 1.0f) + 1.0f;
        //kpb = (2.0f - kpa) / 2.0f;
        //ap = ((rdrive*rdrive) - kpa + 1.0f) / 2.0f;
        //kc = kpa / D(2.0f * D(2.0f * (rdrive*rdrive) - 1.0f) - 2.0f * rdrive*rdrive);

        //srct = (0.1f * sample_rate) / (0.1f * sample_rate + 1.0f);
        //sq = kc*kc + 1.0f;
        //knb = -1.0f * rbdr / D(sq);
        //kna = 2.0f * kc * rbdr / D(sq);
        //an = rbdr*rbdr / sq;
        //imr = 2.0f * knb + D(2.0f * kna + 4.0f * an - 1.0f);
        //pwrq = 2.0f / (imr + 1.0f);

        //ptr->prev_drive = drive;
        //ptr->prev_blend = blend;
    //}

    //for (sample_index = 0; sample_index < sample_count; sample_index++) {

        //in = *(input++);
        //prev_med = ptr->prev_med;
        //prev_out = ptr->prev_out;

        //if (in >= 0.0f) {
            //med = (D(ap + in * (kpa - in)) + kpb) * pwrq;
        //} else {
            //med = (D(an - in * (kna + in)) + knb) * pwrq * -1.0f;
        //}

        //out = srct * (med - prev_med + prev_out);

        //if (out < -1.0f)
            //out = -1.0f;

        //*(output++) += out * ptr->run_adding_gain;

        //ptr->prev_med = M(med);
        //ptr->prev_out = M(out);
    //}

    //ptr->rdrive = rdrive;
    //ptr->rbdr = rbdr;
    //ptr->kpa = kpa;
    //ptr->kpb = kpb;
    //ptr->kna = kna;
    //ptr->knb = knb;
    //ptr->ap = ap;
    //ptr->an = an;
    //ptr->imr = imr;
    //ptr->kc = kc;
    //ptr->srct = srct;
    //ptr->sq = sq;
    //ptr->pwrq = pwrq;
//}




/* Throw away a TubeWarmth effect instance. */
void
cleanup_TubeWarmth(LV2_Handle Instance) {

    free(Instance);
}

const void*
extension_data_TubeWarmth(const char* uri)
{
    return NULL;
}

static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/tubewarmth",
    instantiate_TubeWarmth,
    connect_port_TubeWarmth,
    activate_TubeWarmth,
    run_TubeWarmth,
    deactivate_TubeWarmth,
    cleanup_TubeWarmth,
    extension_data_TubeWarmth
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

/* _init() is called automatically when the plugin library is first
   loaded. */
//void
//_init() {

    //char ** port_names;
    //LV2_PortDescriptor * port_descriptors;
    //LV2_PortRangeHint * port_range_hints;

    //if ((mono_descriptor =
         //(LV2_Descriptor *)malloc(sizeof(LV2_Descriptor))) == NULL)
        //exit(1);


    //mono_descriptor->UniqueID = ID_MONO;
    //mono_descriptor->Label = strdup("tap_tubewarmth");
    //mono_descriptor->Properties = LV2_PROPERTY_HARD_RT_CAPABLE;
    //mono_descriptor->Name = strdup("TAP TubeWarmth");
    //mono_descriptor->Maker = strdup("Tom Szilagyi");
    //mono_descriptor->Copyright = strdup("GPL");
    //mono_descriptor->PortCount = PORTCOUNT_MONO;

    //if ((port_descriptors =
         //(LV2_PortDescriptor *)calloc(PORTCOUNT_MONO, sizeof(LV2_PortDescriptor))) == NULL)
        //exit(1);

    //mono_descriptor->PortDescriptors = (const LV2_PortDescriptor *)port_descriptors;
    //port_descriptors[DRIVE] = LV2_PORT_INPUT | LV2_PORT_CONTROL;
    //port_descriptors[BLEND] = LV2_PORT_INPUT | LV2_PORT_CONTROL;
    //port_descriptors[INPUT] = LV2_PORT_INPUT | LV2_PORT_AUDIO;
    //port_descriptors[OUTPUT] = LV2_PORT_OUTPUT | LV2_PORT_AUDIO;

    //if ((port_names =
         //(char **)calloc(PORTCOUNT_MONO, sizeof(char *))) == NULL)
        //exit(1);

    //mono_descriptor->PortNames = (const char **)port_names;
    //port_names[DRIVE] = strdup("Drive");
    //port_names[BLEND] = strdup("Tape--Tube Blend");
    //port_names[INPUT] = strdup("Input");
    //port_names[OUTPUT] = strdup("Output");

    //if ((port_range_hints =
         //((LV2_PortRangeHint *)calloc(PORTCOUNT_MONO, sizeof(LV2_PortRangeHint)))) == NULL)
        //exit(1);

    //mono_descriptor->PortRangeHints = (const LV2_PortRangeHint *)port_range_hints;
    //port_range_hints[DRIVE].HintDescriptor =
        //(LV2_HINT_BOUNDED_BELOW |
         //LV2_HINT_BOUNDED_ABOVE |
         //LV2_HINT_DEFAULT_LOW);
    //port_range_hints[BLEND].HintDescriptor =
        //(LV2_HINT_BOUNDED_BELOW |
         //LV2_HINT_BOUNDED_ABOVE |
         //LV2_HINT_DEFAULT_MAXIMUM);
    //port_range_hints[DRIVE].LowerBound = 0.1f;
    //port_range_hints[DRIVE].UpperBound = 10.0f;
    //port_range_hints[BLEND].LowerBound = -10.0f;
    //port_range_hints[BLEND].UpperBound = 10.0f;
    //port_range_hints[INPUT].HintDescriptor = 0;
    //port_range_hints[OUTPUT].HintDescriptor = 0;
    //mono_descriptor->instantiate = instantiate_TubeWarmth;
    //mono_descriptor->connect_port = connect_port_TubeWarmth;
    //mono_descriptor->activate = NULL;
    //mono_descriptor->run = run_TubeWarmth;
    //mono_descriptor->run_adding = run_adding_TubeWarmth;
    //mono_descriptor->set_run_adding_gain = set_run_adding_gain_TubeWarmth;
    //mono_descriptor->deactivate = NULL;
    //mono_descriptor->cleanup = cleanup_TubeWarmth;
//}


//void
//delete_descriptor(LV2_Descriptor * descriptor) {
    //unsigned long index;
    //if (descriptor) {
        //free((char *)descriptor->Label);
        //free((char *)descriptor->Name);
        //free((char *)descriptor->Maker);
        //free((char *)descriptor->Copyright);
        //free((LV2_PortDescriptor *)descriptor->PortDescriptors);
        //for (index = 0; index < descriptor->PortCount; index++)
            //free((char *)(descriptor->PortNames[index]));
        //free((char **)descriptor->PortNames);
        //free((LV2_PortRangeHint *)descriptor->PortRangeHints);
        //free(descriptor);
    //}
//}


/* _fini() is called automatically when the library is unloaded. */
//void
//_fini() {
    //delete_descriptor(mono_descriptor);
//}


/* Return a descriptor of the requested plugin type. */
