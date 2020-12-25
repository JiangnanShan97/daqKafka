/********************************************************************
* Description:  usb_4711a.h											*
*               This file, 'usb_4711a.h', is the header file of 	*
*				driver for USB data acquisition card USB-4711A.		*
*																	*
* Author: Li Wangyang														*
* License: GPL Version 2											*
*																	*
* Copyright (c) 2018 All rights reserved.							*
********************************************************************/

#ifndef USB_4711A_H
#define USB_4711A_H

#define NORMAL_MODE 0
#define DIFFERENTIAL_MODE 1

#define PLUS_MINUS_10 0
#define PLUS_10 1
#define PLUS_MINUS_5 2
#define PLUS_5 3

extern int num_a_chans;
extern int hal_clockrate;
extern int hal_startchannel;
extern int hal_sectionlength;
extern int hal_flag_start;
extern int hal_flag_stop;

/* Read signal from one analog channel */
extern void usb_4711a_read_chan();
extern double halchannel[8];

#endif
