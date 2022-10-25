/* BMSVC
 * 
 * Battery Monitor Service
 * 
 */

#include <stdio.h>
#include <string.h>
#include "autocom.h"
#include "chargemodel.h"
#include <time.h>
#include <signal.h>


#define ACCONFIG    "com.conf"
#define BATCONFIG   "bat.conf"
#define STRLEN      128
#define SAMPLE_HZ   600.
#define SAMPLE_N    5

// A state flag that signals it's OK to continue program execution
char go_f;



void halt(int signum){
    go_f = 0;
}


int main(int argc, char* argv[]){
    acdev_t dev;
    int err;
    cmbat_t bat;
    double data[25];
    double  I=0,
            V=0, 
            T=0, 
            I1,
            V1,
            T1,
            I2, 
            V2, 
            T2;
    struct timespec start, stop;
    int ii;
    char bad_f;
    char message[AC_STRLEN];
    
    // Set the go flag high BEFORE registering the exit signals.
    go_f = 1;
    // Register interrupt signals
    signal(SIGINT, &halt);
    signal(SIGTERM, &halt);
    
    /*********************/
    /* OPEN CONFIG FILES */
    /*********************/
    err = (int) acconfig(&dev, ACCONFIG);
    if(err){
        printf("BMSVC: Received error (%02x) while parsing com configuration file: %s\n", err, ACCONFIG);
        return -1;
    }
    err = cmconfig(&bat, BATCONFIG, 1./60.);
    if(err){
        acmessage(&dev, "BMSVC: Failed to parse  - aborting.", ACLOG_ESSENTIAL);
        return -1;
    }
    
    /**************************/
    /* OPEN DEVICE CONNECTION */
    /**************************/
    err = acopen(&dev);
    if(err){
        acerror(err, message);
        acmessage(&dev, message, ACLOG_ESSENTIAL);
        acmessage(&dev, "Fatal error. Aborting.", ACLOG_ESSENTIAL);
        return -1;
    }

    /*************************/
    /* INITIALIZE THE DEVICE */
    /*************************/
    // Make sure there is not an active stream
    // Errors are acceptable here.
    err = acstream_stop(&dev);
    // Initialize the device
    err = acinit(&dev);
    if(err){
        acerror(err, message);
        acmessage(&dev, message, ACLOG_ESSENTIAL);
        acmessage(&dev, "Fatal error. Aborting.", ACLOG_ESSENTIAL);
        return -1;
    }
    // Start the data stream
    err = acstream_start(&dev, SAMPLE_HZ, SAMPLE_N);
    if(err){
        acerror(err, message);
        acmessage(&dev, message, ACLOG_ESSENTIAL);
        acmessage(&dev, "Fatal error. Aborting.", ACLOG_ESSENTIAL);
        return -1;
    }

    /******************************/
    /* START THE MEASUREMENT LOOP */
    /******************************/
    while(go_f){
        // The "bad" flag indicates whether the data should be trusted
        bad_f = 0;
        // Average 10 samples gathered at 600Hz.  This averages over a
        // 60Hz cycle to reject line noise.
        err = acstream_read(&dev, data);
        acmean(&dev, data, &I1, &V1, &T1);
        if(err){
            bad_f = 1;
            acmessage(&dev, "ACSTREAM failed.", ACLOG_HIGH);
        }
        err = acstream_read(&dev, data);
        acmean(&dev, data, &I2, &V2, &T2);
        if(err){
            bad_f = 1;
            acmessage(&dev, "ACSTREAM failed.", ACLOG_HIGH);
        }
        
        // If the stream failed
        // use the data from the last valid measurement
        if(bad_f){
            I = bat->I;
            V = bat->Vt;
            T = bat->T;
        // Otherwise, average the latest measurements
        }else{
            I = 0.5*(I1+I2);
            V = 0.5*(V1+V2);
            T = 0.5*(T1+T2);
        }
        
        // Update the battery model
        cmstep(&bat, I, V, T);
    }

    // Halt the data stream
    err = acstream_stop(&dev);
    // Close the device connection
    err = acclose(&dev);
    return 0;
}
