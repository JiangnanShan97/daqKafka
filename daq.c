/********************************************************************
* Description:  daq.c							*
*               This file, 'daq.c', is a HAL components for data	*
*				acquisition with external DAQ devices.	*
*									*
* Author: Hu Po								*
* Last modified by: Shan Jiangnan					*
* License: GPL Version 2						*
*									*
* Copyright (c) 2018 All rights reserved.				*
********************************************************************/

#include "hal.h"			/* HAL public API decls */
#include "rtapi.h"			/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include <malloc.h>

#include "./header/daq.h"
#include "./header/usb_4711a.h"

/* module information */
MODULE_AUTHOR("HIT_CNC");
MODULE_DESCRIPTION("HAL module for data aquisition");
MODULE_LICENSE("GPL");

/***********************************************************************
*                STRUCTURES AND GLOBAL VARIABLES                       *
************************************************************************/

static int comp_id;		/* component ID */
hal_a_chan_data* hal_a_chan_ptr;	// used to store all the sensors data
double halchannel[8];

int num_a_chans;
int hal_clockrate;
int hal_startchannel;
int hal_sectionlength;
int hal_flag_start;
int hal_flag_stop;

RTAPI_MP_INT(num_a_chans, "number of analog channels");
RTAPI_MP_INT(hal_clockrate, "clockrate of the daq device");
RTAPI_MP_INT(hal_startchannel,"the physical start channel of daq device");
RTAPI_MP_INT(hal_sectionlength,"the sectionlength of the daq device");
RTAPI_MP_INT(hal_flag_start,"the initial flag of the daq device");
RTAPI_MP_INT(hal_flag_stop,"the stop flag of the daq device");

//start stop parameters used for test, when the test is completed, you can use the "hal_flag_start","hal_flag_stop" to replace them.
int start = 0;
int stop = 1;


/***********************************************************************
*                    LOCAL FUNCTION DECLARATIONS                       *
************************************************************************/

static int export_pins(void);
static int export_functions(void);

static void read_a_chan(void *arg, long period);
static void read_a_all(void *arg, long period);


/***********************************************************************
*                        INIT AND EXIT CODE                            *
************************************************************************/

int rtapi_app_main(void)
{
	int retval;

	/* STEP 1: initialise the driver */
	comp_id = hal_init("daq");
	if (comp_id < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "DAQ: ERROR: hal_init() failed\n");
		return -1;
	}
	start=hal_flag_start;
	stop=hal_flag_stop;

	/* STEP 2: allocate shared memory for etherCAT data */
	hal_a_chan_ptr = hal_malloc(num_a_chans * sizeof(hal_a_chan_data));			// hal_a_chan_ptr存放所有读取的通道数据

	if (hal_a_chan_ptr == 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "DAQ: ERROR: hal_malloc() failed\n");
		hal_exit(comp_id);
		return -1;
	}

	/* STEP 3: export the pin(s) */
	retval = export_pins();
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "DAQ: export_pins() failed\n");
		hal_exit(comp_id);
		return -1;
	}

	/* STEP 4: export sync function */
	retval = export_functions();
	if (retval != 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "DAQ: export_functions() failed\n");
		hal_exit(comp_id);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_INFO, "DAQ: installed driver for %d analog in channels\n", num_a_chans);
	hal_ready(comp_id);

 	return 0;
}

void rtapi_app_exit(void)
{
	hal_exit(comp_id);
}

/***********************************************************************
*                      EXPORT PINS AND FUNCTIONS                       *
************************************************************************/

static int export_pins(void)
{
	int n, retval;
	hal_a_chan_data* a_chan = NULL;

	/* pins for analog input */
    for (n = 0; n < num_a_chans; n++) {
    	a_chan = &(hal_a_chan_ptr[n]);
    	if ((retval = hal_pin_float_newf(HAL_OUT, &(a_chan->analog_in), comp_id, "daq.analog-in.%d", n)) != 0)
    		return retval;
    	if ((retval = hal_pin_float_newf(HAL_OUT, &(a_chan->analog_in_val), comp_id, "daq.analog-in-val.%d", n)) != 0)
    		return retval;
    	if ((retval = hal_param_float_newf(HAL_RW, &(a_chan->scale_in), comp_id, "daq.scale-in.%d", n)) != 0)
    		return retval;
    	if ((retval = hal_param_s32_newf(HAL_RW, &(a_chan->gain_in), comp_id, "daq.gain-in.%d", n)) != 0)
    		return retval;
    	if ((retval = hal_param_s32_newf(HAL_RW, &(a_chan->daq_type), comp_id, "daq.type.%d", n)) != 0)
			return retval;
    	if ((retval = hal_param_s32_newf(HAL_RW, &(a_chan->channel), comp_id, "daq.channel.%d", n)) != 0)
			return retval;
    	if ((retval = hal_param_s32_newf(HAL_RW, &(a_chan->read_mode), comp_id, "daq.read-mode.%d", n)) != 0)
			return retval;
    }

    return 0;
}

static int export_functions(void)
{
	int n, retval;
	char name[HAL_NAME_LEN + 1];

	rtapi_snprintf(name, sizeof(name), "read.analog-in.all");
	if ((retval = hal_export_funct(name, read_a_all, &hal_a_chan_ptr, 1, 0, comp_id)) != 0)
		return retval;

    for (n = 0; n < num_a_chans; n++) {
    	rtapi_snprintf(name, sizeof(name), "read.analog-in.%d", n);
    	if ((retval = hal_export_funct(name, read_a_chan, &(hal_a_chan_ptr[n]), 1, 0, comp_id)) != 0)
    		return retval;
    }

	return 0;
}

/***********************************************************************
*                    REALTIME READ AND WRITE FUNCTIONS                 *
************************************************************************/

static void read_a_chan(void *arg, long period)
{
	hal_a_chan_data* a_chan = arg;
	int type = a_chan->daq_type;
	int chan = a_chan->channel;
	double gain = a_chan->gain_in;
	int mode = a_chan->read_mode;
	double scale = a_chan->scale_in;
	
	if(type == USB_4711A)
	{
		usb_4711a_read_chan();
		*(a_chan->analog_in) = halchannel[chan];
		*(a_chan->analog_in_val) = halchannel[chan] * scale;
	}
	

}

static void read_a_all(void *arg, long period)
{	
	//set parameters for every channel
	int n, type, chan, mode;
	double gain, scale;
	hal_a_chan_data* a_chan = NULL;
	
	usb_4711a_read_chan();
	for (n = 0; n < num_a_chans; n++) {
		a_chan = &(hal_a_chan_ptr[n]);
		type = a_chan->daq_type;
		chan = a_chan->channel;
		gain = a_chan->gain_in;
		mode = a_chan->read_mode;
		scale = a_chan->scale_in;

		if(type == USB_4711A)
		{
			*(a_chan->analog_in) = halchannel[n];
			*(a_chan->analog_in_val) = halchannel[n] * scale;
		}
	}

	
}

