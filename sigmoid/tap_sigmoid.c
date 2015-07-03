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

    $Id: tap_sigmoid.c,v 1.3 2005/08/30 11:19:14 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <lv2.h>
#include "tap_utils.h"


/* The Unique ID of the plugin: */

#define ID_MONO         2157

/* The port numbers for the plugin: */

#define PREGAIN         0
#define POSTGAIN        1
#define INPUT           2
#define OUTPUT          3

/* Total number of ports */
#define PORTCOUNT_MONO   4


/* The closer this is to 1.0, the slower the input parameter
   interpolation will be. */
#define INTERP 0.99f


/* The structure used to hold port connection information and state */

typedef struct {
    float * pregain;
    float * postgain;
    float * input;
    float * output;

    int interpolating;
    float pregain_i;
    float postgain_i;

    unsigned long sample_rate;
    float run_adding_gain;
} Sigmoid;


/* Construct a new plugin instance. */
LV2_Handle
instantiate_Sigmoid(const LV2_Descriptor * Descriptor, double SampleRate, const char* bundle_path, const LV2_Feature* const* features) {

        LV2_Handle * Instance;

    if ((Instance = malloc(sizeof(Sigmoid))) != NULL) {
        Sigmoid * ptr = (Sigmoid *)Instance;
        ptr->interpolating = 0;
        ptr->sample_rate   = SampleRate;

        return ptr;
    }
        return NULL;
}


/* Connect a port to a data location. */
void
connect_port_Sigmoid(LV2_Handle Instance,
               uint32_t Port,
               void * DataLocation) {

    Sigmoid * ptr = (Sigmoid *)Instance;

    switch (Port) {
    case PREGAIN:
        ptr->pregain = (float *) DataLocation;
        break;
    case POSTGAIN:
        ptr->postgain = (float *) DataLocation;
        break;
    case INPUT:
        ptr->input = (float *) DataLocation;
        break;
    case OUTPUT:
        ptr->output = (float *) DataLocation;
        break;
    }
}


void activate_Sigmoid(LV2_Handle Instance) {

    Sigmoid * ptr = (Sigmoid *)Instance;
    ptr->interpolating = 0;
}


void
run_Sigmoid(LV2_Handle Instance,
        uint32_t SampleCount) {

    Sigmoid * ptr = (Sigmoid *)Instance;
    float * input = ptr->input;
    float * output = ptr->output;
    float pregain = db2lin(LIMIT(*(ptr->pregain),-90.0f,20.0f));
    float postgain = db2lin(LIMIT(*(ptr->postgain),-90.0f,20.0f));

    if (ptr->interpolating == 0) {
        ptr->interpolating = 1;
        ptr->pregain_i = pregain;
        ptr->postgain_i = postgain;
    }

    float pregain_i = ptr->pregain_i;
    float postgain_i = ptr->postgain_i;

    unsigned long sample_index;
    unsigned long sample_count = SampleCount;

    float in = 0.0f;
    float out = 0.0f;

    if ((pregain_i != pregain) || (postgain_i != postgain)) {

        for (sample_index = 0; sample_index < sample_count; sample_index++) {

            pregain_i = pregain_i * INTERP + pregain * (1.0f - INTERP);
            postgain_i = postgain_i * INTERP + postgain * (1.0f - INTERP);

            in = *(input++) * pregain_i;

            out = 2.0f / (1.0f + exp(-5.0*in)) - 1.0f;

            *(output++) = out * postgain_i;
        }

        ptr->pregain_i = pregain_i;
        ptr->postgain_i = postgain_i;

    } else {
        for (sample_index = 0; sample_index < sample_count; sample_index++) {

            in = *(input++) * pregain_i;

            out = 2.0f / (1.0f + exp(-5.0*in)) - 1.0f;

            *(output++) = out * postgain_i;
        }

        ptr->pregain_i = pregain_i;
        ptr->postgain_i = postgain_i;
    }
}


//void
//set_run_adding_gain_Sigmoid(LV2_Handle Instance, float gain) {

    //Sigmoid * ptr = (Sigmoid *)Instance;

    //ptr->run_adding_gain = gain;
//}


//void
//run_adding_Sigmoid(LV2_Handle Instance,
           //unsigned long SampleCount) {

    //Sigmoid * ptr = (Sigmoid *)Instance;
    //float * input = ptr->input;
    //float * output = ptr->output;
    //float pregain = db2lin(LIMIT(*(ptr->pregain),-90.0f,20.0f));
    //float postgain = db2lin(LIMIT(*(ptr->postgain),-90.0f,20.0f));
    //float pregain_i = ptr->pregain_i;
    //float postgain_i = ptr->postgain_i;

    //unsigned long sample_index;
    //unsigned long sample_count = SampleCount;

    //float in = 0.0f;
    //float out = 0.0f;


    //if ((pregain_i != pregain) || (postgain_i != postgain))   {

        //for (sample_index = 0; sample_index < sample_count; sample_index++) {

            //pregain_i = pregain_i * INTERP + pregain * (1.0f - INTERP);
            //postgain_i = postgain_i * INTERP + postgain * (1.0f - INTERP);

            //in = *(input++) * pregain_i;

            //out = 2.0f / (1.0f + exp(-5.0*in)) - 1.0f;

            //*(output++) = out * postgain_i * ptr->run_adding_gain;
        //}

        //ptr->pregain_i = pregain_i;
        //ptr->postgain_i = postgain_i;

    //} else {
        //for (sample_index = 0; sample_index < sample_count; sample_index++) {

            //in = *(input++) * pregain_i;

            //out = 2.0f / (1.0f + exp(-5.0*in)) - 1.0f;

            //*(output++) = out * postgain_i * ptr->run_adding_gain;
        //}
    //}
//}


/* Throw away a Sigmoid effect instance. */
void
cleanup_Sigmoid(LV2_Handle Instance) {

    free(Instance);
}

const void*
extension_data_Sigmoid(const char* uri)
{
    return NULL;
}


static const
LV2_Descriptor Descriptor = {
    "http://moddevices.com/plugins/tap/sigmoid",
    instantiate_Sigmoid,
    connect_port_Sigmoid,
    activate_Sigmoid,
    run_Sigmoid,
    NULL,
    cleanup_Sigmoid,
    extension_data_Sigmoid
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
    //mono_descriptor->Label = strdup("tap_sigmoid");
    //mono_descriptor->Properties = LV2_PROPERTY_HARD_RT_CAPABLE;
    //mono_descriptor->Name = strdup("TAP Sigmoid Booster");
    //mono_descriptor->Maker = strdup("Tom Szilagyi");
    //mono_descriptor->Copyright = strdup("GPL");
    //mono_descriptor->PortCount = PORTCOUNT_MONO;

    //if ((port_descriptors =
         //(LV2_PortDescriptor *)calloc(PORTCOUNT_MONO, sizeof(LV2_PortDescriptor))) == NULL)
        //exit(1);

    //mono_descriptor->PortDescriptors = (const LV2_PortDescriptor *)port_descriptors;
    //port_descriptors[PREGAIN] = LV2_PORT_INPUT | LV2_PORT_CONTROL;
    //port_descriptors[POSTGAIN] = LV2_PORT_INPUT | LV2_PORT_CONTROL;
    //port_descriptors[INPUT] = LV2_PORT_INPUT | LV2_PORT_AUDIO;
    //port_descriptors[OUTPUT] = LV2_PORT_OUTPUT | LV2_PORT_AUDIO;

    //if ((port_names =
         //(char **)calloc(PORTCOUNT_MONO, sizeof(char *))) == NULL)
        //exit(1);

    //mono_descriptor->PortNames = (const char **)port_names;
    //port_names[PREGAIN] = strdup("Pre Gain [dB]");
    //port_names[POSTGAIN] = strdup("Post Gain [dB]");
    //port_names[INPUT] = strdup("Input");
    //port_names[OUTPUT] = strdup("Output");

    //if ((port_range_hints =
         //((LV2_PortRangeHint *)calloc(PORTCOUNT_MONO, sizeof(LV2_PortRangeHint)))) == NULL)
        //exit(1);

    //mono_descriptor->PortRangeHints   = (const LV2_PortRangeHint *)port_range_hints;
    //port_range_hints[PREGAIN].HintDescriptor =
        //(LV2_HINT_BOUNDED_BELOW |
         //LV2_HINT_BOUNDED_ABOVE |
         //LV2_HINT_DEFAULT_0);
    //port_range_hints[POSTGAIN].HintDescriptor =
        //(LV2_HINT_BOUNDED_BELOW |
         //LV2_HINT_BOUNDED_ABOVE |
         //LV2_HINT_DEFAULT_0);
    //port_range_hints[PREGAIN].LowerBound = -90.0f;
    //port_range_hints[PREGAIN].UpperBound = 20.0f;
    //port_range_hints[POSTGAIN].LowerBound = -90.0f;
    //port_range_hints[POSTGAIN].UpperBound = 20.0f;
    //port_range_hints[INPUT].HintDescriptor = 0;
    //port_range_hints[OUTPUT].HintDescriptor = 0;
    //mono_descriptor->instantiate = instantiate_Sigmoid;
    //mono_descriptor->connect_port = connect_port_Sigmoid;
    //mono_descriptor->activate = NULL;
    //mono_descriptor->run = run_Sigmoid;
    //mono_descriptor->run_adding = run_adding_Sigmoid;
    //mono_descriptor->set_run_adding_gain = set_run_adding_gain_Sigmoid;
    //mono_descriptor->deactivate = NULL;
    //mono_descriptor->cleanup = cleanup_Sigmoid;
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


///* _fini() is called automatically when the library is unloaded. */
//void
//_fini() {
    //delete_descriptor(mono_descriptor);
//}


/* Return a descriptor of the requested plugin type. */
//const LV2_Descriptor *
//lv2_descriptor(unsigned long Index) {

    //switch (Index) {
    //case 0:
        //return mono_descriptor;
    //default:
        //return NULL;
    //}
//}
