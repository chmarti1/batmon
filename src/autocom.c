#include "autocom.h"
#include <string.h>
#include <unistd.h>
#include <time.h>

#define NBUF 256            // The maximum number of bytes for buffers
#define TIMEOUT_MS  2000    // The timeout interval in milliseconds

char stemp[AC_STRLEN];
uint8_t txbuffer[NBUF];
uint8_t rxbuffer[NBUF];


// HELPER ROUTINES

#define streq(a,b) (strcmp(a,b) == 0)

// Return a double-precision representation of a 64-bit fixed-point 
// number in the buffer.
double f64_to_double(uint8_t * source){
    double result;
    result = *(int64_t*) source;
    return result / 0x100000000;
}

/* CHECKSUM8
 *  Calculate the appropriate checksum8 for the bytes already loaded in
 *  the buffer.  Automatically detects the type of packet to determine 
 *  the appropriate bytes to include in the calculation. 
 *  NOTE - the cs16 results must already be loaded in buffer 4 and 5
 *  for the result to be valid.
 */
uint8_t checksum8(uint8_t *b){
    int index;
    unsigned int sum;
    unsigned int N;
    
    // Is this an extended command?
    // Extended commands have all 1's in bits 3-6
    if((b[1] & 0x78) == 0x78)
        N = 6;
    // Is this a normal command?
    // Normal command lengths are determined by b[1]'s 3 LSbs
    else
        N = (b[1] & 0x07) * 2 + 2;
        
        // accumulate the sum
    sum = 0;
    for(index=1; index<N; index++)
        sum += b[index];
        
    // modify the sum to be 8 bits
    sum = (sum>>8) + (sum & 0xFF);
    sum = (sum>>8) + (sum & 0xFF);
    return (uint8_t) sum;
}

/* CHECKSUM16
 *  Calculate the Word checksum for Bytes already loaded in the
 *  buffer.  Automatically ignores the values in bytes 0-5, which 
 *  includes the checksum8, the command header, and the checksum16 bytes.
 
 *  Returns -1 if the buffer is confirugred for a normal command format.
 *  Returns -2 if the length is illegal.
 *  If length is not NULL, then its value will be set to the detected
 *  buffer length.
 */
int checksum16(uint8_t *b, unsigned int *length){
    int index;
    unsigned int sum = 0;
    unsigned int _length;
    
    // If the application doesn't need to know the buffer length, use a
    // local instead.
    if(!length)
        length = &_length;
    
    // Is this an extended command?
    // Extended commands have all 1's in bits 3-6
    if((b[1] & 0x78) == 0x78)
        (*length) = b[2] * 2 + 6;
    // Is this a normal command?
    // Normal commands do not have cs16 values.
    else{
        (*length) = (b[1] & 0x07) * 2 + 2;
        return -1;
    }
    
    // Test the length for sanity
    if(*length > NBUF){
        return -2;
    }
    // accumulate the sum
    sum = 0;
    for(index=6; index<(*length); index++)
        sum += b[index];
        
    return sum;
}

void buffer_dump(uint8_t *b){
    unsigned int ii;
    unsigned int length;
    checksum16(b, &length);
    if(length>NBUF){
        printf("CORRUPT BUFFER: length = %d!?\n", length);
        return;
    }
        
    for(ii=0;ii<length;ii++){
        printf("%3d(0x%02x) : %02x\n", ii, ii, b[ii]);
    }
}

/* xmit
 *  This wrapper for LJUSB_WriteTO() and LJUSB_ReadTO() automatically 
 *  detects the transmission length, type, and inserts the appropriate 
 *  checksums.  It waits for the response and checks the transmission
 *  for errors.
 * 
 *  If there is a problem with the read or write operations, each is 
 *  repeated "attempts" times before xmit returns with an error. 
 * 
 *  Even though xmit returns an acerror type, it does not write to the
 *  error message string.  That behavior is reserved for the top layer
 *  functions.
 * 
 *  xmit returns three possible error codes: 
 *  ACERR_CORRUPT_BUFFER  indicates that the checksum16 buffer length 
 *          check failed.
 *  ACERR_TX_FAILURE and ACERR_RX_FAILURE indicate that the read/write
 *          operations did not succeed.
 */
acerror_t xmit(acdev_t *dev, unsigned int attempts, unsigned int reply){
    unsigned int count;
    unsigned int txlength, rxlength;
    unsigned long result;
    char c_flag;
    int txcs16, rxcs16;
    uint8_t rxcs8;
    
    // What is the transmission type?
    // If this is an extended command, then cs16 and cs8 are both 
    // needed.
    txcs16 = checksum16(txbuffer, &txlength);
    // extended transmission requires cs16
    if(txcs16 >= 0){
        *(uint16_t*) &txbuffer[4] = (uint16_t) txcs16;
    }else if(txcs16 == -2){
        acmessage(dev, "XMIT: The buffer was not correctly initialized. Aborting in the CS16 step.",ACLOG_MEDIUM);
        return ACERR_CORRUPT_BUFFER;
    }
    
    // cs8 is always required
    txbuffer[0] = checksum8(txbuffer);
    
    // Attempt to transmit
    count = 0;
    c_flag = 1;     // continue flag
    while(c_flag){
        count ++;
        // Try to transmit.  c_flag will be True on failure
        result = LJUSB_WriteTO(dev->handle, txbuffer, txlength, TIMEOUT_MS);
        c_flag = txlength != result;
        // If transmission was unsuccessful
        if(c_flag){
            // Check for too many tries
            if(count == attempts){
                sprintf(stemp, "XMIT: Transmission failed. Attempted (%d) bytes, sent (%ld).", txlength, result);
                acmessage(dev, stemp, ACLOG_MEDIUM);
                return ACERR_TX_FAILURE;
            }
        // If transmission was successful
        }else{
            // Try to receive.  c_flag will be True on failure
            rxlength = LJUSB_ReadTO(dev->handle, rxbuffer, reply, TIMEOUT_MS);
            c_flag = (rxlength != reply);
            // Check for a bad checksum reply
            if(rxbuffer[1] == 0xB8){
                c_flag = 1;
                acmessage(dev, "XMIT: Device detected a bad checksum.", ACLOG_MEDIUM);
                if(count == attempts)
                    return ACERR_BAD_CHECKSUM;
            // Check for no reply
            }else if(rxlength == 0){
                acmessage(dev, "XMIT: No reply.", ACLOG_MEDIUM);
                if(count == attempts)
                    return ACERR_RX_FAILURE;
            // Check for an unexpected reply
            }else if(c_flag){
                sprintf(stemp, "XMIT: Expected %d bytes, received %d.", reply, rxlength);
                acmessage(dev,stemp, ACLOG_MEDIUM);
                if(count == attempts)
                    return ACERR_RX_LENGTH;
            // If the reply appears valid, compare checksums and length
            }else{
                rxcs16 = checksum16(rxbuffer, &rxlength);
                rxcs8 = checksum8(rxbuffer);
                // Verify the packet length matches the received length
                c_flag = (rxlength != reply);
                if(c_flag){
                    sprintf(stemp, "XMIT: Aborting. Packet length (%d) does not match the expected length (%d).", rxlength, reply);
                    acmessage(dev, stemp, ACLOG_MEDIUM);
                    // Automatic failure; do not re-attempt
                    return ACERR_RX_LENGTH;
                // Check the checksum 8
                }else if(rxcs8 != rxbuffer[0]){
                    c_flag = 1;
                    sprintf(stemp, "XMIT: Bad checksum 8. Received 0x%02x. Calculated 0x%02x.", rxbuffer[0], rxcs8);
                    acmessage(dev,stemp, ACLOG_MEDIUM);
                    if(count == attempts)
                        return ACERR_BAD_CHECKSUM;
                // Check the checksum 16 buffer
                }else if(rxcs16 >= 0 && rxcs16 != *(uint16_t*) &rxbuffer[4]){
                    c_flag = 1;
                    sprintf(stemp, "XMIT: Bad checksum 16.  Received 0x%04x.  Calculated 0x%04x.", *(uint16_t*) &rxbuffer[4], rxcs16);
                    acmessage(dev, stemp, ACLOG_MEDIUM);
                    if(count == attempts)
                        return ACERR_RX_FAILURE;
                }
            }
        }
    }
    return ACERR_NONE;
}




/* 
 * PUBLIC FUNCTIONS
 * 
 *   These are the definitions for the outward-facing functions, 
 *   accessible by the application.
 */


acerror_t acmessage(acdev_t *dev, char *text, acloglevel_t level){
    time_t now;
    FILE *lfd;
    char close_f;   // Close the file?
    static char stemp[AC_STRLEN];
    
    if(level > dev->loglevel && level >= 0)
        return ACERR_NONE;
    
    // What is the current time?
    now = time(NULL);
    
    close_f = 0;
    // If a log file is specified, try to open it
    if(dev->logfile[0]){
        // If the open operation succeeds
        if(lfd = fopen(dev->logfile,"a")){
            // We'll need to remember to close it later
            close_f = 1;
        // If the open operation fails, complain to the user, and revert
        // to standard output
        }else{
            fprintf(stderr, "ACMESSAGE: Failed to write to log file\n    %s\n", dev->logfile);            
            lfd = stdout;
        }
    // If no log file is given, just use standard output
    }else{
        lfd = stdout;
    }
    
    // Write the message
    strftime(stemp, sizeof(stemp), "[%F %H:%M:%S]", localtime(&now));
    fprintf(lfd, "%s %s\n", stemp, text);
    
    // Close the file if necessary
    if(close_f)
        fclose(lfd);
    
    return ACERR_NONE;
}



acerror_t acdata(acdev_t *dev, cmbat_t *bat){
    struct timeval now;
    FILE *dfd;
    double ipeak;
    
    gettimeofday(&now, NULL);
    
    dfd = fopen(dev->datafile, "a");
    // If the open failed
    if(!dfd){
        sprintf(stemp, "ACDATA: Failed to open/create file: %s", dev->datafile);
        acmessage(stemp);
        return ACERR_LSD_FILE;
    }
    // Is the data file empty?
    // If so, write a header.
    if(ftell(dfd) == 0){
        fprintf(dfd, "# Battery monitor data file: %s\n", dev->datafile);
        fprintf(dfd, "# Created: %d\n", now->tv_sec);
        fprintf(dfd, "# Time is in seconds since the epoch.\n");
        fprintf(dfd, "# Charge states are (U)nknown, (E)mpty, (D)ischarging, (C)harging, and (F)ull.\n");
        fprintf(dfd, "# Time(s)  Terminal_Voltage(V)  Open_Circuit_Voltage(V)  Mean_Current(A)  Peak_Current(A)  Charge(Ahr)  SOC(-)  Charge_State(-)\n");
    }
    
    // Detect whether the maximum or minimum current should be used
    if(bat->Istat.mean >= 0)
        ipeak = bat->Istat.max;
    else
        ipeak = bat->Istate.min;
    fprintf(dfd, "%d\t%7.3f\t%7.3f\t%+7.3f\t%+7.3f\t%+12.3f\t%7.3f\t%c\n",\
        now->tv_sec,\
        bat->Vt,\
        bat->Voc,\
        bat->Istat.mean,\
        ipeak,\
        bat->Q/3600,\
        (char) bat->chargestate);
    fclose(dfd);
}

void acerror(acerror_t err, char *target){
    switch(err){
    case ACERR_NONE:
        strcpy("ACERR_NONE: There was no error.",target);
    break;
    // Generic Errors
    case ACERR_DEV_NOT_OPEN:
        strcpy("ACERR_DEV_NOT_OPEN: Operation failed because the device connection is not open.",target);
    break;
    case ACERR_BAD_CHECKSUM:
        strcpy("ACERR_BAD_CHECKSUM: Communication failed with a bad checksum.",target);
    break;
    case ACERR_CORRUPT_BUFFER:
        strcpy("ACERR_CORRUPT_BUFFER: Unexpected corruption of the communication buffer!",target);
    break;
    case ACERR_TX_FAILURE:
        strcpy("ACERR_TX_FAILURE: Failed while transmitting to the device.",target);
    break;
    case ACERR_RX_FAILURE:
        strcpy("ACERR_RX_FAILURE: Failed to receive a reply from the device.",target);
    break;
    case ACERR_RX_LENGTH:
        strcpy("ACERR_RX_LENGTH: The device replied with an illegal packet size!",target);
    break;
    case ACERR_PARAM_ERROR:
        strcpy("ACERR_PARAM_ERROR: An AC function was called with an illegal parameter.",target);        
    break;

    // Configuration errors
    case ACERR_CONFIG_FILE:
        strcpy("ACERR_CONFIG_FILE: Could not open the configuration file.",target);
    break;
    case ACERR_CONFIG_SYNTAX:
        strcpy("ACERR_CONFIG_SYNTAX: There was a syntax error in the configuration file.",target);
    break;
    case ACERR_LSD_FILE:
        strcpy("ACERR_LSD_FILE: Failed to write to the log, stat, or data file.",target);    
    break;

    // Init/Open Errors
    case ACERR_DEV_ALREADY_OPEN:
        strcpy("ACERR_DEV_ALREADY_OPEN: Open failed - device connection already open.",target);
    break;
    case ACERR_OPEN_FAILED:
        strcpy("ACERR_OPEN_FAILED: Failed to open the device connection.",target);
    break;
    case ACERR_CONFIG_FAILED:
        strcpy("ACERR_CONFIG_FAILED: Initial configuration of the device failed.",target);    
    break;
    
    // Stream Errors
    case ACERR_AICONFIG_FAILED:
        strcpy("ACERR_AICONFIG_FAILED: Failed to configure the device for analog input streaming.",target);
    break;
    case ACERR_AISTART_FAILED:
        strcpy("ACERR_AISTART_FAILED: Failed to start an analog input stream.",target);
    break;
    case ACERR_AIREAD_FAILED:
        strcpy("ACERR_AISTART_FAILED: Failed to read from an analog input stream.",target);
    break;
    case ACERR_AISTOP_FAILED:
        strcpy("ACERR_AISTART_FAILED: Failed to stop an analog input stream.",target);
    break;
    default:
        sprintf(target, "Unrecognized error code: %02x", err);
    }
}


acerror_t acconfig(acdev_t *dev, char *filename){
    int result, count, err;
    char param[AC_STRLEN], value[AC_STRLEN];
    double *ftarget;
    char *starget;
    FILE *fd = NULL;
    acerror_t done = ACERR_NONE;

    // First, initialize the device parameters that need to be set
    // before running acinit()
    dev->handle = NULL;
    dev->datafile[0] = '\0';
    dev->logfile[0] = '\0';
    dev->statfile[0] = '\0';
    dev->current_slope = 0;
    dev->current_zero = 0;
    dev->voltage_slope = 0;
    dev->voltage_zero = 0;
    dev->temp_slope = 0;
    dev->loglevel = ACLOG_LOW;
    dev->aistr_active = 0;
       
    // Open the configuration file
    fd = fopen(filename, "r");
    if(!fd){
        sprintf(stemp, "ACCONFIG: Failed to open file: %s", filename);
        acmessage(dev, stemp,ACLOG_ESSENTIAL);
        return ACERR_CONFIG_FILE;
    }
    
    for(count=1; !feof(fd); count++){
        result = fscanf(fd, "%256s %256[^\n]", param, value);
        
        if(param[0] != '#' && result != 2 && !feof(fd)){
            sprintf(stemp, "ACCONFIG: Parameter-value pair number %d was illegal. (%d)", count, result);
            acmessage(dev, stemp, ACLOG_ESSENTIAL);
            sprintf(stemp, "          param: %70s", param);
            acmessage(dev, stemp, ACLOG_ESSENTIAL);
            sprintf(stemp, "          value: %70s", value);
            acmessage(dev, stemp, ACLOG_ESSENTIAL);
            return ACERR_CONFIG_SYNTAX;
        }else if(result == 0){
            break;
        }
        
        // If the value is a floating point, ftarget is a double pointer
        // that will be set to the value's end destination.  If it is
        // not NULL when parsing is complete, then the value will be
        // converted into a float.
        ftarget = NULL;
        starget = NULL;
        if(param[0] == '#'){
            // This is a comment.  Do nothing.
        }else if(streq(param, "current_slope")){
            ftarget = &dev->current_slope;
        }else if(streq(param, "current_zero")){
            ftarget = &dev->current_zero;
        }else if(streq(param, "voltage_slope")){
            ftarget = &dev->voltage_slope;
        }else if(streq(param, "voltage_zero")){
            ftarget = &dev->voltage_zero;
        }else if(streq(param, "tdata")){
            ftarget = &dev->tdata;
        }else if(streq(param, "logfile")){
            starget = &dev->logfile;
        }else if(streq(param, "statfile")){
            starget = &dev->statfile;
        }else if(streq(param, "datafile")){
            starget = &dev->datafile;
        }else if(streq(param, "loglevel")){
            if(streq(value, "none"))
                dev->loglevel = ACLOG_ESSENTIAL;
            else if(streq(value, "low"))
                dev->loglevel = ACLOG_LOW;
            else if(streq(value, "medium"))
                dev->loglevel = ACLOG_MEDIUM;
            else if(streq(value, "high"))
                dev->loglevel = ACLOG_HIGH;
            else{
                sprintf(stemp, "ACCONFIG: loglevel must be [none, low, high, debug].  Found: %32s", value);
                acmessage(dev, stemp, ACLOG_ESSENTIAL);
            }
        }else{
            sprintf(stemp, "ACCONFIG: Unrecognized parameter: %60s", param);
            acmessage(dev, stemp, ACLOG_ESSENTIAL);
            fclose(fd);
            return ACERR_CONFIG_SYNTAX;
        }
        
        // If the value is floating point
        if(ftarget){
            if(1 != sscanf(value, "%lf", ftarget)){
                sprintf(stemp, "ACCONFIG: Non-numerical value for param: %60s", param);
                acmessage(dev, stemp, ACLOG_ESSENTIAL);
                sprintf(stemp, "          value: %70s", value);
                acmessage(dev, stemp, ACLOG_ESSENTIAL);
                fclose(fd);
                return ACERR_CONFIG_SYNTAX;
            }
        // If the value is a string, move it.
        }else if(starget)
            strcpy(starget, value);
    }
    fclose(fd);
    return done;
}

acerror_t acopen(acdev_t *dev){
    acerror_t err;
    
    if(dev->handle){
        acmessage(dev, "ACOPEN: The connection already appears to be open.", ACLOG_ESSENTIAL);
        return ACERR_DEV_ALREADY_OPEN;
    }
    
    // Initialize the stream state parameters
    dev->aistr_active = 0;
    dev->aistr_backlog = 0;
    
    // Find a U3; it should be the only LJ device.
    dev->handle = LJUSB_OpenDevice(1, 0, U3_PRODUCT_ID);
    if(!dev->handle){
        acmessage(dev, "ACOPEN: Open operation failed. Check device connection.", ACLOG_ESSENTIAL);
        return ACERR_OPEN_FAILED;
    }
    
    // Use ConfigU3 command to retrieve version information
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x0A;
    txbuffer[3] = 0x08;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x00;
    err = xmit(dev,1,38);
    if(err){
        acmessage(dev, "ACINIT: Failed to read device version information.", ACLOG_MEDIUM);
        return err;
    }
    
    dev->firmware_version = rxbuffer[10] + ((float) rxbuffer[9]) / 100;
    dev->bootloader_version = rxbuffer[12] + ((float) rxbuffer[11]) / 100;
    dev->hardware_version = rxbuffer[14] + ((float) rxbuffer[13]) / 100;
    dev->serial_number = *((unsigned int*) &rxbuffer[15]);
    
    // Get calibration information
    // Use the ReadMem command to read the analog input calibration
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x01;
    txbuffer[3] = 0x2D;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x00;   // Block 0
    err = xmit(dev,1,40);
    if(err){
        acmessage(dev, "ACINIT: Failed while loading device analog input calibration.", ACLOG_MEDIUM);
        return err;
    }
    dev->ain_slope = f64_to_double(&rxbuffer[8]);
    dev->ain_offset = f64_to_double(&rxbuffer[16]);
    
    // Use the ReadMem command to read the temperature calibration
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x01;
    txbuffer[3] = 0x2D;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x02;   // Block 2
    err = xmit(dev,1,40);
    if(err){
        acmessage(dev, "ACINIT: Failed while loading device analog input calibration.", ACLOG_MEDIUM);
        return err;
    }
    dev->temp_slope = f64_to_double(&rxbuffer[8]);
    
    return ACERR_NONE;
}

/* ACINIT - initialize the device connection
 * 
 *  DEV is the ACDEV_T device struct pointer.  Before calling ACINIT,
 * the application is responsible for initializing values of the struct
 * dev->handle must be set to NULL before
 *  initialization, or an ACERR_DEV_ALREADY_OPEN error will be raised.
 * 
 *  Errors:
 *  ACERR_DEV_ALREADY_OPEN  The device handle (dev->handle) was not NULL.
 *  ACERR_OPEN_FAILED   The LJUSB_OpenDevice failed.
 */
acerror_t acinit(acdev_t *dev){
    acerror_t err;
    unsigned int ii;
    char stemp[AC_STRLEN];
    
    // Test for an existing connection
    if(!dev->handle){
        acmessage(dev, "ACINIT: The device connection does not appear to be open.", ACLOG_ESSENTIAL);
        return ACERR_DEV_NOT_OPEN;
    }
    
    
    
    // Use the FEEDBACK command to set the EIO pin directions and states
    // We'll turn everything on to test the LEDs and we'll turn the U3
    // LED off to signify the test.
    
    txbuffer[1] = 0xF8;
    txbuffer[2] = 9; // number of data words
    txbuffer[3] = 0x00; // Feedback CMD
    ii = 6;
    txbuffer[ii++] = 0x00;  // Echo byte
    txbuffer[ii++] = 29;    // Port direction write
    txbuffer[ii++] = 0x00;  // FIO write mask
    txbuffer[ii++] = 0xFF;  // EIO write mask
    txbuffer[ii++] = 0x00;  // CIO write mask
    txbuffer[ii++] = 0x00;  // FIO direction mask
    txbuffer[ii++] = AC_EIODOUT_MASK;  // EIO direction bits
    txbuffer[ii++] = 0x00;  // CIO direction mask
    txbuffer[ii++] = 27;    // Port state write
    txbuffer[ii++] = 0x00;  // FIO write mask
    txbuffer[ii++] = AC_EIODOUT_MASK;  // EIO write mask
    txbuffer[ii++] = 0x00;  // CIO write mask
    txbuffer[ii++] = 0x00;  // FIO values
    txbuffer[ii++] = AC_EIODOUT_MASK;  // EIO values
    txbuffer[ii++] = 0x00;  // CIO values
    txbuffer[ii++] = 9;     // LED Write
    txbuffer[ii++] = 0x00;  // Turn the LED off
    txbuffer[ii++] = 0x00;  // Extra empty byte to finish the word
    
    /*
    txbuffer[1] = 0xF8;
    txbuffer[2] = 2;
    txbuffer[3] = 0x00;
    txbuffer[6] = 0x01;
    txbuffer[7] = 0x09;
    txbuffer[8] = 0x01;
    txbuffer[9] = 0x00;
    */
    err = xmit(dev,1,10);
    if(err){
        acmessage(dev, "ACINIT: Failed to initialize pin states.", ACLOG_ESSENTIAL);
        return err;
    }else if(rxbuffer[6]){
        sprintf(stemp, "ACINIT: Pin initialization failed in frame %d with error code: 0x%02x\n", rxbuffer[7], rxbuffer[6]);
        acmessage(dev, stemp, ACLOG_ESSENTIAL);
        return ACERR_CONFIG_FAILED;
    }
    
    
    // Use ConfigIO to set the EIOAnalog register
    txbuffer[1] = 0xF8; // Extended command
    txbuffer[2] = 0x03; // # Data words
    txbuffer[3] = 0x0B; // ConfigIO CMD
    txbuffer[6] = 0x0c; // Write mask FIO + EIO
    txbuffer[7] = 0x00; // Reserved
    txbuffer[10] = 0x0F;    // FIO Analog in
    txbuffer[11] = AC_EIOAIN_MASK;  // EIO Analog in
    err = xmit(dev,1,12);
    if(err){
        acmessage(dev, "ACINIT: Failed while setting the FIO/EIO IO settings.", ACLOG_ESSENTIAL);
        return err;
    }

    // Finally, use the FEEDBACK command to return LEDs to their resting
    // states.  Normally, this happens so quickly the user will not 
    // notice, but if configuration fails, the alarm and LEDs will hang
    // in a state to warn the user.
    txbuffer[1] = 0xF8;
    txbuffer[2] = 7;
    txbuffer[3] = 0x00;
    ii = 6;
    txbuffer[ii++] = 0x02;  // Echo byte
    txbuffer[ii++] = 27;    // Port state write
    txbuffer[ii++] = 0x00;  // FIO write mask
    txbuffer[ii++] = AC_EIODOUT_MASK;  // EIO write mask
    txbuffer[ii++] = 0x00;  // CIO write mask
    txbuffer[ii++] = 0x00;  // FIO values
    txbuffer[ii++] = 0x00;  // EIO values
    txbuffer[ii++] = 0x00;  // CIO values
    txbuffer[ii++] = 9;     // LED write
    txbuffer[ii++] = 0x01;  // Turn the LED on
    
    /*
    txbuffer[1] = 0xF8;
    txbuffer[2] = 2;
    txbuffer[3] = 0x00;
    txbuffer[6] = 0x01;
    txbuffer[7] = 0x09;
    txbuffer[8] = 0x01;
    txbuffer[9] = 0x00;
    */
    err = xmit(dev,1,10);
    if(err){
        acmessage(dev,"ACINIT: Failed to set pin states.\n", ACLOG_MEDIUM);
        return err;
    }else if(rxbuffer[6]){
        sprintf(stemp, "ACINIT: Failed to turn off LEDs in frame %d with error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes\n",
                rxbuffer[7], rxbuffer[6]);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_CONFIG_FAILED;
    }

    return ACERR_NONE;
}


acerror_t acclose(acdev_t *dev){
    
   
    // There isn't much more to do if the deivce is already closed
    if(!dev->handle)
        return ACERR_NONE;
    
    LJUSB_CloseDevice(dev->handle);
    dev->handle = NULL;
    
    return ACERR_NONE;
}


acerror_t acshow(acdev_t *dev){
    if(dev->handle)
        printf("Connection: OPEN\n");
    else
        printf("Connection: CLOSED\n");
    printf("Firmware: %f\nBootloader: %f\nHardware: %f\nSN: %d\n",
        dev->firmware_version,
        dev->bootloader_version,
        dev->hardware_version,
        dev->serial_number);
    printf("AIN slope (V/bit): %.6e\nAIN offset (V): %.6e\n",
        dev->ain_slope,
        dev->ain_offset);
    printf("Temp slope (K/bit): %.6e\n",
        dev->temp_slope);
    printf("Current slope (A/V): %.6e\nCurrent zero (V): %.6e\n",
        dev->current_slope,
        dev->current_zero);
    printf("Voltage slope (V/V): %.6e\nVoltage zero (V): %.6e\n",
        dev->voltage_slope,
        dev->voltage_zero);
    printf("Data interval (sec): %f\n", 
        dev->tdata);
        
    return ACERR_NONE;
}


acerror_t acset(acdev_t *dev, acpin_t pin, int value){
    acerror_t err;
    uint8_t write;
    
    if(!dev->handle){
        acmessage(dev, "ACSET: Device connection is not open.", ACLOG_MEDIUM);
        return ACERR_DEV_NOT_OPEN;
    }
    
    write = pin + AC_EIO_OFFSET;
    if(value)
        write |= AC_BITSTATE_MASK;
    
    // Finally, use the FEEDBACK command to set LEDs
    
    txbuffer[1] = 0xF8;
    txbuffer[2] = 2;
    txbuffer[3] = 0x00;
    
    txbuffer[6] = 0x03;  // Echo byte
    txbuffer[7] = 0x0B; // Bit state write
    txbuffer[8] = write;
    txbuffer[9] = 0x00;
    
    err = xmit(dev,1,10);
    if(err){
        acmessage(dev, "ACSET: Failed to set pin states.\n", ACLOG_MEDIUM);
        return err;
    }else if(rxbuffer[6]){
        sprintf(stemp, "ACSET: Failed to set output with error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes\n",
                rxbuffer[6]);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_CONFIG_FAILED;
    }

    
    return ACERR_NONE;
}


acerror_t acget(acdev_t *dev, acpin_t pin, double *value){
    acerror_t err;
    uint8_t read;
    
    if(!dev->handle){
        acmessage(dev, "ACGET: Device connection is not open.", ACLOG_MEDIUM);
        return ACERR_DEV_NOT_OPEN;
    }else if(   !(
            pin == ACPIN_CS ||
            pin == ACPIN_VS ||
            pin == ACPIN_T  )){
        sprintf(stemp, "ACGET: Illegal analog input pin number: %d", pin);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_PARAM_ERROR;
    }
    
    // Use the FEEDBACK command to query the AI channel
    // 0x40 mask uses long settling time and slow ADC
    read = 0x40 | (pin + AC_EIO_OFFSET);
    
    txbuffer[1] = 0xF8;
    txbuffer[2] = 2;
    txbuffer[3] = 0x00;
    
    txbuffer[6] = 0x04;     // Echo byte
    txbuffer[7] = 0x01;     // AI read
    //txbuffer[8] = 0x08;
    txbuffer[8] = read;
    txbuffer[9] = 0x1F;     // Single-ended
    
    err = xmit(dev,1,12);
    
    if(err){
        acmessage(dev, "ACGET: Communication failed.", ACLOG_ESSENTIAL);
        return err;
    }else if(rxbuffer[6]){
        sprintf(stemp, "ACGET: Failed with LJ error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes",
                rxbuffer[6]);
        acmessage(dev, stemp, ACLOG_ESSENTIAL);
        return ACERR_CONFIG_FAILED;
    }

    // Fetch the raw ADC value
    *value = *((uint16_t *) &rxbuffer[9]);
    
    
    // Perform the conversion
    if(pin == ACPIN_CS){
        // Calculate calibrated voltage
        *value = (*value) * dev->ain_slope + dev->ain_offset;
        // Apply the measurement board calibration
        *value = ((*value) - dev->current_zero) * dev->current_slope;
    }else if(pin == ACPIN_VS){
        // Calculate calibrated voltage
        *value = (*value) * dev->ain_slope + dev->ain_offset;
        // Apply the measurement board calibration
        *value = ((*value) - dev->voltage_zero) * dev->voltage_slope;
    }else if(pin == ACPIN_T){
        *value = (*value) * dev->temp_slope;
    }
    return ACERR_NONE;

}


acerror_t acstream_start(acdev_t *dev, double sample_hz, uint8_t samples_per_packet){
    acerror_t err;
    unsigned int sampleroll;     // the roll value for the sample clock
    
    if(!dev->handle){
        acmessage(dev, "ACSTREAM_START: Device connection is not open.", ACLOG_ESSENTIAL);
        return ACERR_DEV_NOT_OPEN;
    }else if(samples_per_packet * AC_CHANNELS > 25){
        acmessage(dev, "ACSTREAM_START: Failed to start. Too many samples per packet: %d\n", samples_per_packet);
        return ACERR_AISTART_FAILED;
    }
       
    // Calculate the roll value for the sample clock
    sampleroll = (unsigned int) (AC_AICLOCK_HZ / sample_hz) / 256;
    dev->tsample = ((double) sampleroll * 256) / AC_AICLOCK_HZ;
    dev->Nsample = samples_per_packet;
   
    printf("SAMPLEROLL: %d\n", sampleroll);
    // First, configure the stream
    // Use the STREAM CONFIG command
    txbuffer[1] = 0xF8;             // Long format
    txbuffer[2] = AC_CHANNELS + 3;
    txbuffer[3] = 0x11;             // StreamConfig CMD
    
    txbuffer[6] = AC_CHANNELS;      // Number of channels
    txbuffer[7] = AC_CHANNELS * dev->Nsample;
    txbuffer[8] = 0x00;
    txbuffer[9] = 0x0C;             // 48MHz clock, 12.8bit resolution
    //txbuffer[9] = 0x04;             // 4MHz clock, 12.8bit resolution
    txbuffer[10] = (uint8_t) (sampleroll & 0x00FF);
    txbuffer[11] = (uint8_t) (sampleroll >> 8);
    
    // Set the channels
    txbuffer[12] = ACPIN_CS + AC_EIO_OFFSET;    // Current
    txbuffer[13] = 31;                          // Single-ended
    
    txbuffer[14] = ACPIN_VS + AC_EIO_OFFSET;    // Voltage
    txbuffer[15] = 31;                          // Single-ended
    
    txbuffer[16] = 30;                          // Temperature
    txbuffer[17] = 31;                          //
    
    printf("STREAM_CONFIG cmd:\n");
    buffer_dump(txbuffer);
    
    err = xmit(dev,1,8);
    
    printf("STREAM_CONFIG reply:\n");
    buffer_dump(rxbuffer);
    
    if(err){
        acmessage(dev, "ACSTREAM_START: Communication failed while configuring the measurement.", ACLOG_MEDIUM);
        return err;
    }else if(rxbuffer[6]){
        sprintf(stemp, "ACSTREAM_START: Configuration failed with LJ error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes",
                rxbuffer[6]);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_CONFIG_FAILED;
    }
    
    txbuffer[1] = 0xA8;
    err = xmit(dev,1,4);

    
    if(err){
        acmessage(dev, "ACSTREAM_START: Communication failed while starting the measurement.", ACLOG_MEDIUM);
        return err;
    }else if(rxbuffer[2]){
        sprintf(stemp, "ACSTREAM_START: StreamStart failed with LJ error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes",
                rxbuffer[2]);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_AISTART_FAILED;
    }
    
    dev->aistr_active = 1;
    
    if(dev->loglevel >= ACLOG_HIGH)
        acmessage(dev, "Stream started successfully", ACLOG_HIGH);
    
    return ACERR_NONE;
}

acerror_t acstream_read(acdev_t *dev, double *data){
    unsigned int result, nbytes, sample, ii, jj;
    double x;
    
    nbytes = 14 + 2 * AC_CHANNELS * dev->Nsample;   
    // Streaming does not require a transmit/response cycle; only read
    //result = LJUSB_ReadTO(
    /*result = LJUSB_StreamTO(
            dev->handle, rxbuffer, STREAM_NBUF,
            (unsigned int) AC_SAMPLETIME_SEC * 1200);     // Add 20% for a timeout interval (in ms)
    */
    result = LJUSB_StreamTO(
            dev->handle, rxbuffer, nbytes, 5000);
    
    if(result != nbytes){
        sprintf(stemp, "ACSTREAM_READ: Expected %d bytes, got %d.", 64, result);
        acmessage(dev, stemp, ACLOG_ESSENTIAL);
        return ACERR_RX_FAILURE;
    }else if(rxbuffer[11]){
        sprintf(stemp, "ACSTREAM_READ: StreamData failed with LJ error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes",
                rxbuffer[11]);
        acmessage(dev, stemp, ACLOG_ESSENTIAL);
        return ACERR_AIREAD_FAILED;
    }

    jj = 0;     // jj is the position in the data array
    // Apply voltage calibrations
    for(ii=12; ii<12+nbytes;){
        // // // Current Calibration // // //
        // Convert from uint16 to double
        x = (double) rxbuffer[ii++];
        x += (double) rxbuffer[ii++] * 256;
        // Calibrate to voltage
        x = dev->ain_slope * x + dev->ain_offset;
        // Apply sensor calibration
        data[jj++] = dev->current_slope*(x - dev->current_zero);
        // // // Voltage Calibration // // //
        // Convert from uint16 to double
        x = (double) rxbuffer[ii++];
        x += (double) rxbuffer[ii++] * 256;
        // Calibrate to voltage
        x = dev->ain_slope * x + dev->ain_offset;
        // Apply sensor calibration
        data[jj++] = dev->voltage_slope*(x - dev->voltage_zero);
        
        // // // Temperature Calibration // // //
        // Convert from uint16 to double
        x = (double) rxbuffer[ii++];
        x += (double) rxbuffer[ii++] * 256;
        // Apply sensor calibration
        data[jj++] = dev->temp_slope*x;
    }
    dev->aistr_backlog = rxbuffer[nbytes-2];

    if(dev->aistr_backlog)
        acmessage(dev, "Stream read with backlog", ACLOG_HIGH);
    
    return ACERR_NONE;
}

acerror_t acstream_stop(acdev_t *dev){
    acerror_t err;
    
    txbuffer[1] = 0xB0;
    err = xmit(dev,1,4);
    
    if(err){
        acmessage(dev, "ACSTREAM_STOP: Communication failed while stopping the measurement.", ACLOG_MEDIUM);
        return err;
    }else if(rxbuffer[2]){
        sprintf(stemp, "ACSTREAM_STOP: StreamStop failed with LJ error code: 0x%02x\n"
                "  https://labjack.com/support/datasheets/u3/low-level-function-reference/errorcodes",
                rxbuffer[2]);
        acmessage(dev, stemp, ACLOG_MEDIUM);
        return ACERR_AISTOP_FAILED;
    }else{
        acmessage(dev, "ACSTREAM_STOP: Stream stopped successfully\n", ACLOG_HIGH);
    }
    
    dev->aistr_active = 0;
    
    return ACERR_NONE;
}

/* ACCAL - Get Current, Voltage, and Temperature
 * 
 * Accepts the data array from the AC_STREAM_READ() function.  
 * Calculates mean voltage, current, and temperature over the period of
 * data collection (should be one line cycle).
 * 
 * ACCAL does not raise an error.
 */
acerror_t acmean(acdev_t *dev, double* data, double* I, double* V, double* T){
    unsigned int sample_index, index;
    
    // Accumulate the mean signals
    *I = 0.;
    *V = 0.;
    *T = 0.;
    for(index=0; index<AC_CHANNELS*dev->Nsample;){
        *I += data[index++];
        *V += data[index++];
        *T += data[index++];
    }
    *I /= dev->Nsample;
    *V /= dev->Nsample;
    *T /= dev->Nsample;
    
    // The voltage calibration is performed in the acstream_read() function.   
    return ACERR_NONE;
}
