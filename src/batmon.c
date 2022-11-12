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


#define ACCONFIG    "/etc/batmon/com.conf"
#define BATCONFIG   "/etc/batmon/bat.conf"
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
    double data[25];    // Buffer for raw data
    double  I=0,        // Temporary variables for averages
            V=0, 
            T=0, 
            I1,
            V1,
            T1,
            I2, 
            V2, 
            T2;
    unsigned int step_count,    // Counter
        log_count,
        log_steps,      // How many steps between log entries?
        clear_logs;    // How many log entries between data updates?
    char bad_f;         // status flag - data read failed
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
        acmessage(&dev, "BMSVC: Failed to parse " BATCONFIG " - aborting.", ACLOG_ESSENTIAL);
        return -1;
    }
    
    // Calculate the number of steps between data records
    log_steps = (unsigned int) (dev.tdata * 60);    // The steps are at 60Hz
    // For now, the clear_logs parameter is hard-coded
    // This may be configurable in the future.
    // Archive the data every 10 thousand entries
    // If tdata is 300 seconds, this is a little over 1 month of data.
    clear_logs = 10000;
    
    acmessage(&dev, "Starting service.", ACLOG_LOW);
    
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
    step_count = 0;
    log_count = 0;
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
            I = bat.I;
            V = bat.Vt;
            T = bat.T;
        // Otherwise, average the latest measurements
        }else{
            I = 0.5*(I1+I2);
            V = 0.5*(V1+V2);
            T = 0.5*(T1+T2);
        }
        
        // Update the battery model
        // If there is a change of charge state, or if the data record 
        // counter has run out, update the records
        step_count++;
        if(cmstep(&bat, I, V, T) || step_count >= log_steps){
            acdata(&dev, &bat);
            step_count = 0;
            log_count ++;
            // If the log count is at the clear number, move the file
            if(log_count >= clear_logs){
                log_count = 0;
                // Borrow message to construct the archive file name
                sprintf(message, "%s_old", dev.datafile);
                if(rename(dev.datafile, message))
                    acmessage(&dev,"Failed to back up data!", ACLOG_ESSENTIAL);
                else
                    acmessage(&dev,"Started a fresh data file.", ACLOG_ESSENTIAL);
            }
            
            // Reset the signal statistics accumulators
            cmreset(&bat);
            // If soc < 0.5, raise an alarm
            if(bat.soc < 0.5)
                acset(&dev, ACPIN_ALARM, 1);
            else
                acset(&dev, ACPIN_ALARM, 0);
            // Light the red LED if discharging or empty
            if(bat.chargestate == CM_CHARGE_DISCHARGING || bat.chargestate == CM_CHARGE_EMPTY)
                acset(&dev, ACPIN_IND0, 1);
            else
                acset(&dev, ACPIN_IND0, 0);
            // Light the first green LED if discharging or charging
            if(bat.chargestate == CM_CHARGE_DISCHARGING || bat.chargestate == CM_CHARGE_CHARGING)
                acset(&dev, ACPIN_IND1, 1);
            else
                acset(&dev, ACPIN_IND1, 0);
        }
    }

    acmessage(&dev, "Halting service.", ACLOG_LOW);

    // Halt the data stream
    err = acstream_stop(&dev);
    // Close the device connection
    err = acclose(&dev);
    return 0;
}
