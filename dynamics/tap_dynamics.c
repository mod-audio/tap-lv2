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

    $Id: tap_dynamics_st.c,v 1.2 2004/06/15 14:50:55 tszilagyi Exp $
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include <lv2.h>
#include "tap_utils.h"


/* ***** VERY IMPORTANT! *****
 *
 * If you enable this, the plugin will use float arithmetics in DSP
 * calculations.  This usually yields lower average CPU usage, but
 * occasionaly may result in high CPU peaks which cause trouble to you
 * and your JACK server.  The default is to use fixpoint arithmetics
 * (with the following #define commented out).  But (depending on the
 * processor on which you run the code) you may find floating point
 * mode usable.
 */
/*#define DYN_CALC_FLOAT*/


typedef signed int sample;

/* coefficient for float to sample (signed int) conversion */
/* this allows for about 60 dB headroom above 0dB, if 0 dB is equivalent to 1.0f */
/* As 2^31 equals more than 180 dB, about 120 dB dynamics remains below 0 dB */
#define F2S 2147483


#ifdef DYN_CALC_FLOAT
typedef float dyn_t;
typedef float rms_t;
#else
typedef sample dyn_t;
typedef int64_t rms_t;
#endif



/* The Unique ID of the plugin: */

#define ID_MONO           2152

#define ID_STEREO         2153

/* The port numbers for the plugin: */

#define ATTACK          0
#define RELEASE         1
#define OFFSGAIN        2
#define MUGAIN          3
#define RMSENV_L        4
#define RMSENV_R        5
#define MODGAIN_L       6
#define MODGAIN_R       7
#define STEREO          8
#define MODE            9
#define INPUT_L         10
#define INPUT_R         11
#define OUTPUT_L        12
#define OUTPUT_R        13

/* Definitions from mono dynamics */

#define RMSENV_M          4
#define MODGAIN_M         5
#define MODE_M            6
#define INPUT_M           7
#define OUTPUT_M          8


/* Total number of ports */

#define PORTCOUNT_MONO   9

#define PORTCOUNT_STEREO   14


#define TABSIZE 256
#define RMSSIZE 64


typedef struct {
        rms_t        buffer[RMSSIZE];
        unsigned int pos;
        rms_t        sum_m;
} rms_envMono;

typedef struct {
        rms_t        buffer[RMSSIZE];
        unsigned int pos;
        rms_t        sum;
} rms_env;


/* max. number of breakpoints on in/out dB graph */
#define MAX_POINTS 20

typedef struct {
    float x;
    float y;
} GRAPH_POINT;

typedef struct {
    unsigned long num_points;
    GRAPH_POINT points[MAX_POINTS];
} DYNAMICS_DATA;

#include "tap_dynamics_presets.h"


/* The structure used to hold port connection information and state */

typedef struct {
    float * attack;
    float * release;
    float * offsgain;
    float * mugain;
    float * rmsenv_L;
    float * rmsenv_R;
    float * modgain_L;
    float * modgain_R;
    float * stereo;
    float * mode;
    float * input_L;
    float * output_L;
    float * input_R;
    float * output_R;
    double sample_rate;

    float * as;
    unsigned long count;
    dyn_t amp_L;
    dyn_t amp_R;
    dyn_t env_L;
    dyn_t env_R;
    float gain_L;
    float gain_R;
    float gain_out_L;
    float gain_out_R;
    rms_env * rms_L;
    rms_env * rms_R;
    rms_t sum_L;
    rms_t sum_R;

    /* variables from dynamics mono */

    float * mugain_m;
    float * rmsenv_m;
    float * modgain_m;
    float * mode_m;
    float * input_m;
    float * output_m;

    dyn_t amp_m;
    dyn_t env_m;
    float gain_m;
    float gain_out_m;
    rms_envMono * rms_m;
    rms_t sum_m;

    DYNAMICS_DATA graph;

} Dynamics;



/* RMS envelope stuff, grabbed without a second thought from Steve Harris's swh-plugins, util/rms.c */
/* Adapted, though, to be able to use fixed-point arithmetics as well. */

rms_envMono *
rms_env_Mononew(void) {

        rms_envMono * new = (rms_envMono *)calloc(1, sizeof(rms_envMono));

        return new;
}

void
rms_env_Monoreset(rms_envMono *r) {

        unsigned int i;

        for (i = 0; i < RMSSIZE; i++) {
                r->buffer[i] = 0.0f;
        }
        r->pos = 0;
        r->sum_m = 0.0f;
}

inline static
dyn_t
rms_env_Monoprocess(rms_envMono *r, const rms_t x) {

        r->sum_m -= r->buffer[r->pos];
        r->sum_m += x;
        r->buffer[r->pos] = x;
        r->pos = (r->pos + 1) & (RMSSIZE - 1);

#ifdef DYN_CALC_FLOAT
        return sqrt(r->sum_m / (float)RMSSIZE);
#else
        return sqrt(r->sum_m / RMSSIZE);
#endif
}

rms_env *
rms_env_new(void) {

        rms_env * new = (rms_env *)calloc(1, sizeof(rms_env));

        return new;
}

void
rms_env_reset(rms_env *r) {

        unsigned int i;

        for (i = 0; i < RMSSIZE; i++) {
                r->buffer[i] = 0.0f;
        }
        r->pos = 0;
        r->sum = 0.0f;
}

inline static
dyn_t
rms_env_process(rms_env *r, const rms_t x) {

        r->sum -= r->buffer[r->pos];
        r->sum += x;
        r->buffer[r->pos] = x;
        r->pos = (r->pos + 1) & (RMSSIZE - 1);

#ifdef DYN_CALC_FLOAT
        return sqrt(r->sum / (float)RMSSIZE);
#else
        return sqrt(r->sum / RMSSIZE);
#endif
}



inline
float
get_table_gain(int mode, float level) {

    float x1 = -80.0f;
    float y1 = -80.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    int i = 0;

    if (level <= -80.0f)
        return get_table_gain(mode, -79.9f);

    while (i < dyn_data[mode].num_points && dyn_data[mode].points[i].x < level) {
        x1 = dyn_data[mode].points[i].x;
        y1 = dyn_data[mode].points[i].y;
        i++;
    }
    if (i < dyn_data[mode].num_points) {
        x2 = dyn_data[mode].points[i].x;
        y2 = dyn_data[mode].points[i].y;
    } else
        return 0.0f;

    return y1 + ((level - x1) * (y2 - y1) / (x2 - x1)) - level;
}


LV2_Handle
instantiate_MonoDynamics(const LV2_Descriptor * Descriptor, double sample_rate, const char* bundle_path, const LV2_Feature* const* features) {

    LV2_Handle * ptr;

    float * as = NULL;
    unsigned int count = 0;
        dyn_t amp_m = 0.0f;
    dyn_t env_m = 0.0f;
    float gain_m = 0.0f;
    float gain_out_m = 0.0f;
    rms_envMono * rms_m = NULL;
    rms_t sum_m = 0;
    int i;

    if ((ptr = malloc(sizeof(Dynamics))) == NULL)
        return NULL;

    ((Dynamics *)ptr)->sample_rate = sample_rate;

        if ((rms_m = rms_env_Mononew()) == NULL)
        return NULL;

        if ((as = malloc(TABSIZE * sizeof(float))) == NULL)
        return NULL;

        as[0] = 1.0f;
        for (i = 1; i < TABSIZE; i++) {
        as[i] = expf(-1.0f / (sample_rate * (float)i / (float)TABSIZE));
        }

        ((Dynamics *)ptr)->as = as;
        ((Dynamics *)ptr)->count = count;
        ((Dynamics *)ptr)->amp_m = amp_m;
        ((Dynamics *)ptr)->env_m = env_m;
        ((Dynamics *)ptr)->gain_m = gain_m;
        ((Dynamics *)ptr)->gain_out_m = gain_out_m;
        ((Dynamics *)ptr)->rms_m = rms_m;
        ((Dynamics *)ptr)->sum_m = sum_m;

    return ptr;
}

/* Construct a new plugin instance. */
LV2_Handle
instantiate_StereoDynamics(const LV2_Descriptor * Descriptor, double sample_rate, const char* bundle_path, const LV2_Feature* const* features) {

    LV2_Handle * ptr;

    float * as = NULL;
    unsigned int count = 0;
    dyn_t amp_L = 0.0f;
    dyn_t amp_R = 0.0f;
    dyn_t env_L = 0.0f;
    dyn_t env_R = 0.0f;
    float gain_L = 0.0f;
    float gain_R = 0.0f;
    float gain_out_L = 0.0f;
    float gain_out_R = 0.0f;
    rms_env * rms_L = NULL;
    rms_env * rms_R = NULL;
    rms_t sum_L = 0.0f;
    rms_t sum_R = 0.0f;
    int i;

    if ((ptr = malloc(sizeof(Dynamics))) == NULL)
        return NULL;

    ((Dynamics *)ptr)->sample_rate = sample_rate;

        if ((rms_L = rms_env_new()) == NULL)
        return NULL;
        if ((rms_R = rms_env_new()) == NULL)
        return NULL;

        if ((as = malloc(TABSIZE * sizeof(float))) == NULL)
        return NULL;

        as[0] = 1.0f;
        for (i = 1; i < TABSIZE; i++) {
        as[i] = expf(-1.0f / (sample_rate * (float)i / (float)TABSIZE));
        }

        ((Dynamics *)ptr)->as = as;
        ((Dynamics *)ptr)->count = count;
        ((Dynamics *)ptr)->amp_L = amp_L;
        ((Dynamics *)ptr)->amp_R = amp_R;
        ((Dynamics *)ptr)->env_L = env_L;
        ((Dynamics *)ptr)->env_R = env_R;
        ((Dynamics *)ptr)->gain_L = gain_L;
        ((Dynamics *)ptr)->gain_R = gain_R;
        ((Dynamics *)ptr)->gain_out_L = gain_out_L;
        ((Dynamics *)ptr)->gain_out_R = gain_out_R;
        ((Dynamics *)ptr)->rms_L = rms_L;
        ((Dynamics *)ptr)->rms_R = rms_R;
        ((Dynamics *)ptr)->sum_L = sum_L;
        ((Dynamics *)ptr)->sum_R = sum_R;

    return ptr;
}

void
activate_Dynamics(LV2_Handle Instance) {


}

void
deactivate_Dynamics(LV2_Handle Instance) {


}

void
connect_port_MonoDynamics(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    Dynamics * ptr = (Dynamics *)Instance;

    switch (Port) {
    case ATTACK:
        ptr->attack = (float*) DataLocation;
        break;
    case RELEASE:
        ptr->release = (float*) DataLocation;
        break;
    case OFFSGAIN:
        ptr->offsgain = (float*) DataLocation;
        break;
    case MUGAIN:
        ptr->mugain_m = (float*) DataLocation;
        break;
    case RMSENV_M:
        ptr->rmsenv_m = (float*) DataLocation;
        // *(ptr->rmsenv_m) = -60.0f;
        break;
    case MODGAIN_M:
        ptr->modgain_m = (float*) DataLocation;
        // *(ptr->modgain_m) = 0.0f;
        break;
    case MODE_M:
        ptr->mode_m = (float*) DataLocation;
        break;
    case INPUT_M:
        ptr->input_m = (float*) DataLocation;
        break;
    case OUTPUT_M:
        ptr->output_m = (float*) DataLocation;
        break;
    }
}

/* Connect a port to a data location. */
void
connect_port_StereoDynamics(LV2_Handle Instance,
             uint32_t Port,
             void * DataLocation) {

    Dynamics * ptr = (Dynamics *)Instance;

    switch (Port) {
    case ATTACK:
        ptr->attack = (float*) DataLocation;
        break;
    case RELEASE:
        ptr->release = (float*) DataLocation;
        break;
    case OFFSGAIN:
        ptr->offsgain = (float*) DataLocation;
        break;
    case MUGAIN:
        ptr->mugain = (float*) DataLocation;
        break;
    case RMSENV_L:
        ptr->rmsenv_L = (float*) DataLocation;
        // *(ptr->rmsenv_L) = -60.0f;
        break;
    case RMSENV_R:
        ptr->rmsenv_R = (float*) DataLocation;
        // *(ptr->rmsenv_R) = -60.0f;
        break;
    case MODGAIN_L:
        ptr->modgain_L = (float*) DataLocation;
        // *(ptr->modgain_L) = 0.0f;
        break;
    case MODGAIN_R:
        ptr->modgain_R = (float*) DataLocation;
        // *(ptr->modgain_R) = 0.0f;
        break;
    case STEREO:
        ptr->stereo = (float*) DataLocation;
        break;
    case MODE:
        ptr->mode = (float*) DataLocation;
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

void
run_MonoDynamics(LV2_Handle Instance,
         uint32_t sample_count) {

    Dynamics * ptr = (Dynamics *)Instance;
    float * input_m = ptr->input_m;
    float * output_m = ptr->output_m;
        const float attack = LIMIT(*(ptr->attack), 4.0f, 500.0f);
        const float release = LIMIT(*(ptr->release), 4.0f, 1000.0f);
        const float offsgain = LIMIT(*(ptr->offsgain), -20.0f, 20.0f);
        const float mugain_m = db2lin(LIMIT(*(ptr->mugain_m), -20.0f, 20.0f));
    const int mode_m = LIMIT(*(ptr->mode_m), 0, NUM_MODES-1);
    unsigned long sample_index;

        dyn_t amp_m = ptr->amp_m;
        dyn_t env_m = ptr->env_m;
        float * as = ptr->as;
        unsigned int count = ptr->count;
        float gain_m = ptr->gain_m;
        float gain_out_m = ptr->gain_out_m;
        rms_envMono * rms_m = ptr->rms_m;
        rms_t sum_m = ptr->sum_m;

        const float ga = as[(unsigned int)(attack * 0.001f * (float)(TABSIZE-1))];
        const float gr = as[(unsigned int)(release * 0.001f * (float)(TABSIZE-1))];
        const float ef_a = ga * 0.25f;
        const float ef_ai = 1.0f - ef_a;

    float level = 0.0f;
    float adjust = 0.0f;

        for (sample_index = 0; sample_index < sample_count; sample_index++) {

#ifdef DYN_CALC_FLOAT
        sum_m += input_m[sample_index] * input_m[sample_index];
        if (amp_m > env_m) {
            env_m = env_m * ga + amp_m * (1.0f - ga);
        } else {
            env_m = env_m * gr + amp_m * (1.0f - gr);
        }
#else
        sum_m += (rms_t)(input_m[sample_index] * F2S * input_m[sample_index] * F2S);
        if (amp_m) {
            if (amp_m > env_m) {
                env_m = (double)env_m * ga + (double)amp_m * (1.0f - ga);
            } else {
                env_m = (double)env_m * gr + (double)amp_m * (1.0f - gr);
            }
        } else
            env_m = 0;
#endif

        if (count++ % 4 == 3) {
#ifdef DYN_CALC_FLOAT
            amp_m = rms_env_Monoprocess(rms_m, sum_m / 4);
#else
            if (sum_m)
                amp_m = rms_env_Monoprocess(rms_m, sum_m / 4);
            else
                amp_m = 0;
#endif

#ifdef DYN_CALC_FLOAT
            if (isnan(amp_m))
                amp_m = 0.0f;
#endif
            sum_m = 0;

            /* set gain_out_m according to the difference between
               the env_melope volume level (env_m) and the corresponding
               output_m level (from graph) */
#ifdef DYN_CALC_FLOAT
            level = 20 * log10f(2 * env_m);
#else
            level = 20 * log10f(2 * (double)env_m / (double)F2S);
#endif
            adjust = get_table_gain(mode_m, level + offsgain);
            gain_out_m = db2lin(adjust);

        }
        gain_m = gain_m * ef_a + gain_out_m * ef_ai;
        output_m[sample_index] = input_m[sample_index] * gain_m * mugain_m;
        output_m[sample_index] = input_m[sample_index] * gain_m * mugain_m;
        }
        ptr->sum_m = sum_m;
        ptr->amp_m = amp_m;
        ptr->gain_m = gain_m;
        ptr->gain_out_m = gain_out_m;
        ptr->env_m = env_m;
        ptr->count = count;

    *(ptr->rmsenv_m) = LIMIT(level, -60.0f, 20.0f);
    *(ptr->modgain_m) = LIMIT(adjust, -60.0f, 20.0f);
}

void
run_StereoDynamics(LV2_Handle Instance,
         uint32_t sample_count) {

    Dynamics * ptr = (Dynamics *)Instance;
    float * input_L = ptr->input_L;
    float * output_L = ptr->output_L;
    float * input_R = ptr->input_R;
    float * output_R = ptr->output_R;
        const float attack = LIMIT(*(ptr->attack), 4.0f, 500.0f);
        const float release = LIMIT(*(ptr->release), 4.0f, 1000.0f);
        const float offsgain = LIMIT(*(ptr->offsgain), -20.0f, 20.0f);
        const float mugain = db2lin(LIMIT(*(ptr->mugain), -20.0f, 20.0f));
    const int stereo = LIMIT(*(ptr->stereo), 0, 2);
    const int mode = LIMIT(*(ptr->mode), 0, NUM_MODES-1);
    unsigned long sample_index;

        dyn_t amp_L = ptr->amp_L;
        dyn_t amp_R = ptr->amp_R;
        dyn_t env_L = ptr->env_L;
        dyn_t env_R = ptr->env_R;
        float * as = ptr->as;
        unsigned int count = ptr->count;
        float gain_L = ptr->gain_L;
        float gain_R = ptr->gain_R;
        float gain_out_L = ptr->gain_out_L;
        float gain_out_R = ptr->gain_out_R;
        rms_env * rms_L = ptr->rms_L;
        rms_env * rms_R = ptr->rms_R;
        rms_t sum_L = ptr->sum_L;
        rms_t sum_R = ptr->sum_R;

        const float ga = as[(unsigned int)(attack * 0.001f * (float)(TABSIZE-1))];
        const float gr = as[(unsigned int)(release * 0.001f * (float)(TABSIZE-1))];
        const float ef_a = ga * 0.25f;
        const float ef_ai = 1.0f - ef_a;

    float level_L = 0.0f;
    float level_R = 0.0f;
    float adjust_L = 0.0f;
    float adjust_R = 0.0f;

        for (sample_index = 0; sample_index < sample_count; sample_index++) {

#ifdef DYN_CALC_FLOAT
                sum_L += input_L[sample_index] * input_L[sample_index];
                sum_R += input_R[sample_index] * input_R[sample_index];

                if (amp_L > env_L) {
                        env_L = env_L * ga + amp_L * (1.0f - ga);
                } else {
                        env_L = env_L * gr + amp_L * (1.0f - gr);
                }
                if (amp_R > env_R) {
                        env_R = env_R * ga + amp_R * (1.0f - ga);
                } else {
                        env_R = env_R * gr + amp_R * (1.0f - gr);
                }
#else
        sum_L += (rms_t)(input_L[sample_index] * F2S) * (rms_t)(input_L[sample_index] * F2S);
        sum_R += (rms_t)(input_R[sample_index] * F2S) * (rms_t)(input_R[sample_index] * F2S);

        if (amp_L) {
            if (amp_L > env_L) {
                env_L = (double)env_L * ga + (double)amp_L * (1.0f - ga);
            } else {
                env_L = (double)env_L * gr + (double)amp_L * (1.0f - gr);
            }
        } else
            env_L = 0;

        if (amp_R) {
            if (amp_R > env_R) {
                env_R = (double)env_R * ga + (double)amp_R * (1.0f - ga);
            } else {
                env_R = (double)env_R * gr + (double)amp_R * (1.0f - gr);
            }
        } else
            env_R = 0;
#endif

        if (count++ % 4 == 3) {
#ifdef DYN_CALC_FLOAT
            amp_L = rms_env_process(rms_L, sum_L * 0.25f);
            amp_R = rms_env_process(rms_R, sum_R * 0.25f);
#else
            if (sum_L)
                amp_L = rms_env_process(rms_L, sum_L * 0.25f);
            else
                amp_L = 0;

            if (sum_R)
                amp_R = rms_env_process(rms_R, sum_R * 0.25f);
            else
                amp_R = 0;
#endif


#ifdef DYN_CALC_FLOAT
            if (isnan(amp_L))
                amp_L = 0.0f;
            if (isnan(amp_R))
                amp_R = 0.0f;
#endif
            sum_L = sum_R = 0;

            /* set gain_out according to the difference between
               the envelope volume level (env) and the corresponding
               output level (from graph) */
#ifdef DYN_CALC_FLOAT
            level_L = 20 * log10f(2 * env_L);
            level_R = 20 * log10f(2 * env_R);
#else
                        level_L = 20 * log10f(2 * (double)env_L / (double)F2S);
                        level_R = 20 * log10f(2 * (double)env_R / (double)F2S);
#endif
            adjust_L = get_table_gain(mode, level_L + offsgain);
            adjust_R = get_table_gain(mode, level_R + offsgain);

            /* set gains according to stereo mode */
            switch (stereo) {
            case 0:
                gain_out_L = db2lin(adjust_L);
                gain_out_R = db2lin(adjust_R);
                break;
            case 1:
                adjust_L = adjust_R = (adjust_L + adjust_R) / 2.0f;
                gain_out_L = gain_out_R = db2lin(adjust_L);
                break;
            case 2:
                adjust_L = adjust_R = (adjust_L > adjust_R) ? adjust_L : adjust_R;
                gain_out_L = gain_out_R = db2lin(adjust_L);
                break;
            }

        }
        gain_L = gain_L * ef_a + gain_out_L * ef_ai;
        gain_R = gain_R * ef_a + gain_out_R * ef_ai;
        output_L[sample_index] = input_L[sample_index] * gain_L * mugain;
        output_R[sample_index] = input_R[sample_index] * gain_R * mugain;
        }
        ptr->sum_L = sum_L;
        ptr->sum_R = sum_R;
        ptr->amp_L = amp_L;
        ptr->amp_R = amp_R;
        ptr->gain_L = gain_L;
        ptr->gain_R = gain_R;
        ptr->gain_out_L = gain_out_L;
        ptr->gain_out_R = gain_out_R;
        ptr->env_L = env_L;
        ptr->env_R = env_R;
        ptr->count = count;

    *(ptr->rmsenv_L) = LIMIT(level_L, -60.0f, 20.0f);
    *(ptr->rmsenv_R) = LIMIT(level_R, -60.0f, 20.0f);
    *(ptr->modgain_L) = LIMIT(adjust_L, -60.0f, 20.0f);
    *(ptr->modgain_R) = LIMIT(adjust_R, -60.0f, 20.0f);
}

void
cleanup_MonoDynamics(LV2_Handle Instance) {

    Dynamics * ptr = (Dynamics *)Instance;

    free(ptr->rms_m);
    free(ptr->as);
    free(Instance);
}

/* Throw away a Dynamics effect instance. */
void
cleanup_StereoDynamics(LV2_Handle Instance) {

    Dynamics * ptr = (Dynamics *)Instance;

    free(ptr->rms_L);
    free(ptr->rms_R);
    free(ptr->as);
    free(Instance);
}

const void*
extension_data_Dynamics(const char* uri)
{
    return NULL;
}

static const
LV2_Descriptor MonoDescriptor = {
    "http://portalmod.com/plugins/tap/dynamics_m",
    instantiate_MonoDynamics,
    connect_port_MonoDynamics,
    activate_Dynamics,
    run_MonoDynamics,
    deactivate_Dynamics,
    cleanup_MonoDynamics,
    extension_data_Dynamics
};

static const
LV2_Descriptor StereoDescriptor = {
    "http://portalmod.com/plugins/tap/dynamics_st",
    instantiate_StereoDynamics,
    connect_port_StereoDynamics,
    activate_Dynamics,
    run_StereoDynamics,
    deactivate_Dynamics,
    cleanup_StereoDynamics,
    extension_data_Dynamics
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
    switch (index) {
    case 0:
        return &MonoDescriptor;
    case 1:
        return &StereoDescriptor;
    default:
        return NULL;
    }
}
