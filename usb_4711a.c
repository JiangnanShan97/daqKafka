/*******************************************************************************
Copyright (c) 1983-2016 Advantech Co., Ltd.
********************************************************************************
THIS IS AN UNPUBLISHED WORK CONTAINING CONFIDENTIAL AND PROPRIETARY INFORMATION
WHICH IS THE PROPERTY OF ADVANTECH CORP., ANY DISCLOSURE, USE, OR REPRODUCTION,
WITHOUT WRITTEN AUTHORIZATION FROM ADVANTECH CORP., IS STRICTLY PROHIBITED. 

================================================================================
REVISION HISTORY
--------------------------------------------------------------------------------
$Log:  $ 
--------------------------------------------------------------------------------
$NoKeywords:  $
*/
/******************************************************************************
*
* Windows Example:
*    SynchronousOneBufferedAI.c
*
* Example Category:
*    AI
*
* Description:
*    This example demonstrates how to use Synchronous One Buffered AI function.
*
* Instructions for Running:
*    1. Set the 'deviceDescription' which can get from system device manager for opening the device. 
*	 2. Set the 'profilePath' to save the profile path of being initialized device. 
* The following arguments are defined in the "daq.c" module
*    3. Set the 'hal_startchannel' as the first channel for scan analog samples  
*    4. Set the 'num_a_chans' to decide how many sequential channels to scan analog samples.
*    5. Set the 'hal_sectionlength' as the length of data section for Buffered AI.
*	 6. Set the 'hal_flag_start' as the ON flag for the daq module.
*    7. Set the 'hal_flag_stop' as the OFF flag for the daq module.
*    8. Set the 'hal_clockrate' as the frequency of the scanning.
*
* I/O Connections Overview:
*    Please refer to your hardware reference manual.
*
******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "./header/bdaqctrl.h"
#include "./header/compatibility.h"


#include "./header/usb_4711a.h"
#include "librdkafka/rdkafka.h"     // kafka driver

//-----------------------------------------------------------------------------------
// Configure the following parameters before running the demo
//-----------------------------------------------------------------------------------

#define deviceDescription L"USB-4711A,BID#1"
wchar_t const *profilePath = L"DemoDevice.xml";


// user buffer size should be equal or greater than raw data buffer length, because data ready count
// is equal or more than smallest section of raw data buffer and up to raw data buffer length.
// users can set 'USER_BUFFER_SIZE' according to demand.
static int USER_BUFFER_SIZE;
int sectionLength=8;
#define sectionCount 0

/****  Kafka Producer Initialization  ****/
static rd_kafka_t *rk;                          // kafka producer
static rd_kafka_headers_t* hdrs = NULL;         // kafka payload headers {"time": timestamp}
static int partition = RD_KAFKA_PARTITION_UA;   // partitioning strategy
static rd_kafka_topic_t* rkt;                   // topic handle 
char* brokers = "localhost:9092";       // broker ip address
char mode = 'P';                        // producer mode
char* topic = "testTopic";              // topic name
rd_kafka_conf_t* conf;                  // conf struct
rd_kafka_topic_conf_t* topic_conf;      // topic Configuration
char errstr[512];                       // error string
char tmp[16];
rd_kafka_resp_err_t err;
char buf[2048];							// for message buf

ErrorCode ret = Success;
WaveformAiCtrl * wfAiCtrl= NULL;

double gains[8] = {100, 100, 100, 51.33, 50.92, 52.11, 0.2174, 0.2174};

void BDAQCALL OnDataReadyEvent         (void * sender, BfdAiEventArgs * args, void *userParam);
void BDAQCALL OnOverRunEvent           (void * sender, BfdAiEventArgs * args, void *userParam);
void BDAQCALL OnCacheOverflowEvent     (void * sender, BfdAiEventArgs * args, void *userParam);
void BDAQCALL OnStoppedEvent           (void * sender, BfdAiEventArgs * args, void *userParam);

FILE * pFile_x = NULL;
FILE * pFile_y = NULL;
FILE * pFile_z = NULL;

FILE * pFile_force_x = NULL;
FILE * pFile_force_y = NULL;
FILE * pFile_force_z = NULL;

FILE * pFile_def_x = NULL;
FILE * pFile_def_y = NULL;

double * Data;

void waitAnyKey()
{
   do {SLEEP(1);} while (!kbhit());
}

void openFile_x()
{
    if (((pFile_x = fopen("acc_x_10000hz.txt", "wb+")) == NULL) || 
        ((pFile_force_x = fopen("force_x_10000hz.txt", "wb+")) == NULL) ||
        ((pFile_def_x = fopen("def_x_10000hz.txt", "wb+")) == NULL)) 
	{
		printf("cannot open x file!\n");
		exit(1);
	}
}

void openFile_y()
{
   if (((pFile_y = fopen("acc_y_10000hz.txt", "wb+")) == NULL) || 
        ((pFile_force_y = fopen("force_y_10000hz.txt", "wb+")) == NULL) ||
        ((pFile_def_y = fopen("def_y_10000hz.txt", "wb+")) == NULL)) 
    {
		printf("cannot open y file!\n");
		exit(1);
    }
}

void openFile_z()
{
   if (((pFile_z = fopen("acc_z_10000hz.txt", "wb+")) == NULL) || 
        ((pFile_force_z = fopen("force_z_10000hz.txt", "wb+")) == NULL) ) 
    {
		printf("cannot open z file!\n");
		exit(1);
    }
}


int sendMessageToTopic()
{
	size_t len = strlen(buf);
	if (buf[len - 1] == '\n')
			buf[--len] = '\0';
	
	// send the data via kafka producer	
    err = RD_KAFKA_RESP_ERR_NO_ERROR;
    
    /* Create header*/
    time_t timer = time(NULL);
    char* curTime = ctime(&timer);
    err = rd_kafka_header_add(hdrs, "time", sizeof("time"), &curTime, sizeof(curTime)); 
    err = rd_kafka_producev(
                rk,
                RD_KAFKA_V_RKT(rkt),
                RD_KAFKA_V_PARTITION(partition),  // automatic partitioning
                RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                RD_KAFKA_V_VALUE(buf, len),
                RD_KAFKA_V_HEADERS(hdrs),
                RD_KAFKA_V_END);
                   
    if (err) {
        rd_kafka_headers_destroy(hdrs);
        fprintf(stdin,
            "%% Failed to produce to topic %s "
            "partition %i: %s\n",
            rd_kafka_topic_name(rkt), partition,
            rd_kafka_err2str(err));
              
        /* Poll to handle delivery reports */
        rd_kafka_poll(rk, 0);
        return -1;
    }
    rd_kafka_poll(rk, 0);
    fprintf(stdin, "Message sent at %s", curTime);
    return 0;
} 


//The function is used to deal with 'DataReady' Event.
void BDAQCALL OnDataReadyEvent (void *sender, BfdAiEventArgs *args, void *userParam)
{
   
	// Data = (double*)malloc((double) sizeof(double) * num_a_chans * hal_sectionlength);
	int32 getDataCount = 0;
	WaveformAiCtrl * waveformAiCtrl = NULL;
	waveformAiCtrl = (WaveformAiCtrl *) sender;
	printf("sender: %p, ID: %d, count: %d, markcount: %d, offset: %d\n", sender, args->Id, args->Count, args->MarkCount, args->Offset);
	printf("usr buff size: %d, args count: %d \n", USER_BUFFER_SIZE, args->Count);
	getDataCount = MinValue(USER_BUFFER_SIZE, args->Count);	// args->count: amount the of new data
	WaveformAiCtrl_GetDataF64(waveformAiCtrl, getDataCount, Data, 0, NULL, NULL, NULL, NULL);
   
	//get the data of every channel, and multiplies gains
	for (int i = 0; i < num_a_chans; i++)
	{
	   halchannel[i] = Data[i] * gains[i];
    }

	//save the data to txt
	for (int i, index = 0; i < num_a_chans; i++) {
		switch (i) {
			case 0:
				fprintf(pFile_force_x, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Xforce=%f\n", halchannel[i]);
				break;
			case 1:
				fprintf(pFile_force_y, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Yforce=%f\n", halchannel[i]);
				break;
			case 2:
				fprintf(pFile_force_z, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Zforce=%f\n", halchannel[i]);
				break;
			case 3:
				fprintf(pFile_x, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Xacceeration=%f\n", halchannel[i]);
				break;
			case 4:
				fprintf(pFile_y, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Yacceeration=%f\n", halchannel[i]);
				break;
			case 5:
				fprintf(pFile_z, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Zacceeration=%f\n", halchannel[i]);
				break;
			case 6:
				fprintf(pFile_def_x, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Xdef=%f\n", halchannel[i]);
				break;
			case 7:
				fprintf(pFile_def_y, "%f\n", halchannel[i]);
				index += sprintf(&buf[index], "Ydef=%f\n", halchannel[i]);
				break;
			default:
				break;
		}
	}
	
	
	if(sendMessageToTopic()){
		printf("Failed to send the meesage\n");
	}
   //printf("The real-time size of file is %d byte\n\n", WrittenFileSize());
   //printf("Executed %d time.\n\n", i++);
}

//The function is used to deal with 'OverRun' Event.
void BDAQCALL OnOverRunEvent (void * sender, BfdAiEventArgs * args, void *userParam)
{
   printf("Streaming AI Overrun: offset = %d, count = %d\n", args->Offset, args->Count);
}

//The function is used to deal with 'CacheOverflow' Event.
void BDAQCALL OnCacheOverflowEvent (void * sender, BfdAiEventArgs * args, void *userParam)
{
   printf(" Streaming AI Cache Overflow: offset = %d, count = %d\n", args->Offset, args->Count);
}

//The function is used to deal with 'Stopped' Event.
void BDAQCALL OnStoppedEvent (void * sender, BfdAiEventArgs * args, void *userParam)
{
   printf("sender: %p, ID: %d, count: %d, markcount: %d, offset: %d\n", sender, args->Id, args->Count, args->MarkCount, args->Offset);
   printf("Streaming AI stopped: offset = %d, count = %d\n", args->Offset, args->Count);
}

/** 
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */ 
static void msg_delivered(rd_kafka_t* rk,
    const rd_kafka_message_t* rkmessage, void* opaque) {
    if (rkmessage->err)
        fprintf(stdin,
            "%% Message delivery failed (broker %"PRId32"): %s\n",
            rd_kafka_message_broker_id(rkmessage),
            rd_kafka_err2str(rkmessage->err));
    else
        fprintf(stdin,
            "%% Message delivered (%zd bytes, offset %"PRId64", "
            "partition %"PRId32", broker %"PRId32"): %.*s\n",
            rkmessage->len, rkmessage->offset,
            rkmessage->partition,
            rd_kafka_message_broker_id(rkmessage),
            (int)rkmessage->len, (const char*)rkmessage->payload);
}  

void usb_4711a_read_chan(void *arg, long period)
{  
	if(hal_flag_start == 1)
	{	
        printf("Data acquisition module initializing... \n");
		USER_BUFFER_SIZE = num_a_chans * hal_sectionlength;
	    
        hal_flag_start = 0;

		Data = (double*)malloc((int)sizeof(double) * num_a_chans * hal_sectionlength);

    	hdrs = rd_kafka_headers_new(1); // contains one header: {time:timestamp}
        conf = rd_kafka_conf_new();

		printf("Setting bootstrap server.\n");
        if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers,
            errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            fprintf(stdin, "%s\n", errstr);
            return;
        }
        rd_kafka_conf_set_dr_msg_cb(conf, msg_delivered);

        /* Create Kafka handle */
		printf("Creating handle.\n");
        if (!(rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf,
            errstr, sizeof(errstr)))) {
            fprintf(stdin,
                "%% Failed to create new producer: %s\n",
                errstr);
            exit(1);
        }
        /* Add brokers */
		printf("Adding broker.\n");
        if (rd_kafka_brokers_add(rk, brokers) == 0) {
            fprintf(stdin, "%% No valid brokers specified\n");
            exit(1);
        }
        
        /* Create topic */
		printf("Creating topic.\n");
        topic_conf = rd_kafka_topic_conf_new();
        rkt = rd_kafka_topic_new(rk, topic, topic_conf);
        topic_conf = NULL; /* Now owned by topic */

        /***************** Event Hander ******************/ 
		printf("Setting event handler.\n");
	
		Conversion * conversion = NULL;
    	Record * record = NULL; 
    	AiChannel * channels= NULL;
    
		// Step 1: Create a 'WaveformAiCtrl' for Waveform AI function.
		wfAiCtrl = WaveformAiCtrl_Create();
		//WaveformAiCtrl * wfAiCtrl = WaveformAiCtrl_Create();

		// Step 2: Open the file descriptors
		openFile_x();
		openFile_y();
		openFile_z();

		// Step 3: Set the notification event Handler by which we can known the state of operation effectively.
		WaveformAiCtrl_addDataReadyHandler     (wfAiCtrl, OnDataReadyEvent,     NULL);
		WaveformAiCtrl_addOverrunHandler       (wfAiCtrl, OnOverRunEvent,       NULL);
		WaveformAiCtrl_addCacheOverflowHandler (wfAiCtrl, OnCacheOverflowEvent, NULL);
		WaveformAiCtrl_addStoppedHandler       (wfAiCtrl, OnStoppedEvent,       NULL);

		do 
		{
		// Step 4: Select a device by device number or device description and specify the access mode.
		// in this example we use ModeWrite mode so that we can fully control the device, including configuring, sampling, etc.
			DeviceInformation devInfo;
			devInfo.DeviceNumber = -1;
			devInfo.DeviceMode   = ModeWrite;
			devInfo.ModuleIndex  = 0;
			wcscpy(devInfo.Description, deviceDescription);
			ret = WaveformAiCtrl_setSelectedDevice(wfAiCtrl, &devInfo);
			CHK_RESULT(ret);
			ret = WaveformAiCtrl_LoadProfile(wfAiCtrl, profilePath);//Loads a profile to initialize the device.
			CHK_RESULT(ret);
			//needs to add singaltype 
			channels = WaveformAiCtrl_getChannels(wfAiCtrl);
			int32 k2;
	  
			for(k2 = 0; k2 < num_a_chans; k2++)
			{
			AiChannel_setSignalType(Array_getItem(channels,k2),SingleEnded);
			AiChannel_setValueRange(Array_getItem(channels,k2),V_Neg10To10);
			}  
	  
			// Step 5: Set necessary parameters for Waveform AI operation, 
			conversion = WaveformAiCtrl_getConversion(wfAiCtrl);
			record = WaveformAiCtrl_getRecord(wfAiCtrl);
			ret = Conversion_setChannelStart(conversion, hal_startchannel);
			CHK_RESULT(ret);
			ret= Conversion_setChannelCount(conversion, num_a_chans);
			CHK_RESULT(ret);
			ret = Conversion_setClockRate(conversion, hal_clockrate);
			CHK_RESULT(ret);
			ret = Record_setSectionCount(record, 0);//The 0 means setting 'streaming' mode.
			CHK_RESULT(ret);
			ret = Record_setSectionLength(record, 8);
			CHK_RESULT(ret);

			// Step 6: The operation has been started.
			// We can get samples via event handlers.
			ret = WaveformAiCtrl_Prepare(wfAiCtrl);
			CHK_RESULT(ret);
			ret = WaveformAiCtrl_Start(wfAiCtrl);
			CHK_RESULT(ret);
			printf("DAQ initialization complete!\n");
		} while (0);
			if (BioFailed(ret))
				printf("Some error occurred. And the last error code is 0x%X.\n", ret);
	}

	if(hal_flag_stop == 1)
	{
		hal_flag_start = 0;

		free(Data);

		do{
		// step 8: Stop the operation if it is running.
			ret = WaveformAiCtrl_Stop(wfAiCtrl);
			CHK_RESULT(ret);
		} while(0);

		// Step 9: Close device, release any allocated resource.
		WaveformAiCtrl_Dispose(wfAiCtrl);
		fclose(pFile_x);
		fclose(pFile_y);
		fclose(pFile_z);
		fclose(pFile_force_x);
		fclose(pFile_force_y);
		fclose(pFile_force_z);
		fclose(pFile_def_x);
		fclose(pFile_def_y);

        /* Poll to handle delivery reports */
        rd_kafka_poll(rk, 0);
        
        /* Wait for messages to be delivered */
        while (rd_kafka_outq_len(rk) > 0)
            rd_kafka_poll(rk, 100);
        
        /* Destroy topic */
        rd_kafka_topic_destroy(rkt);
        
        /* Destroy the handle */
        rd_kafka_destroy(rk);
        
        fprintf(stdin, "Producer terminated successfully.\n");

		// If something wrong in this execution, print the error code on screen for tracking.
		if (BioFailed(ret))
		{
            printf("Some error occurred. And the last error code is 0x%X.\n", ret);
            waitAnyKey();// wait any key to quit!
		}
	}
}
