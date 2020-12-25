/********************************************************************
* Description:  daq.h												*
*               This file, 'daq.h', is the header file of			*
*				an HAL components for data acquisition with			*
*				external DAQ devices.								*
*																	*
* Author: Hu Po														*
* License: GPL Version 2											*
*																	*
* Copyright (c) 2018 All rights reserved.							*
********************************************************************/

#ifndef DAQ_H
#define DAQ_H

#define USB_4711A 0

typedef struct {
	hal_float_t*	analog_in;		// HAL_OUT | Analog in signal of channel
	hal_float_t*	analog_in_val;	// HAL_OUT | Analog in value of channel
	hal_float_t		scale_in;		// PARAM | Scale of channel
	hal_s32_t		gain_in;		// PARAM | Gain of channel
	hal_s32_t		daq_type;		// PARAM | Type of DAQ device
	hal_s32_t		channel;		// PARAM | Channel number
	hal_s32_t		read_mode;		// PARAM | Read mode
} hal_a_chan_data;

#endif


