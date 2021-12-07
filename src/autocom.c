#include "autocom.h"
#include <string.h>


#define NBUF 256            // The maximum number of bytes for buffers
#define TIMEOUT_MS  4000    // The timeout interval in milliseconds

uint8_t txbuffer[NBUF];
uint8_t rxbuffer[NBUF];
FILE* messages = NULL;


// HELPER ROUTINES


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
acerror_t xmit(HANDLE h, unsigned int attempts, unsigned int reply){
    unsigned int count;
    unsigned int txlength, rxlength;
    char c_flag;
    int txcs16, rxcs16;
    uint8_t rxcs8;
    
    // What is the transmission type?
    // If this is an extended command, then cs16 and cs8 are both 
    // needed.
    txcs16 = checksum16(txbuffer, &txlength);
    // extended transmission requires cs16
    if(txcs16 >= 0){
        txbuffer[4] = (uint8_t) (txcs16 & 0xFF);
        txbuffer[5] = (uint8_t) txcs16 >> 8;
    }else if(txcs16 == -2){
        if(messages)
            fprintf(messages, "XMIT: The buffer was not correctly initialized. Aborting in the CS16 step.\n");
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
        c_flag = txlength != LJUSB_WriteTO(h, txbuffer, txlength, TIMEOUT_MS);
        // If transmission was unsuccessful
        if(c_flag){
            c_flag = 1;
            // Check for too many tries
            if(count == attempts){
                if(messages)
                    fprintf(messages, "XMIT: Transmission failed.\n");
                return ACERR_TX_FAILURE;
            }
        // If transmission was successful
        }else{
            // Try to receive.  c_flag will be True on failure
            rxlength = LJUSB_ReadTO(h, rxbuffer, reply, TIMEOUT_MS);
            c_flag = (rxlength != reply);
            // Check for a bad checksum reply
            if(rxbuffer[1] == 0xB8){
                c_flag = 1;
                if(messages)
                    fprintf(messages, "XMIT: Device detected a bad checksum.\n");
                if(count == attempts)
                    return ACERR_BAD_CHECKSUM;
            // Check for no reply
            }else if(rxlength == 0){
                if(messages)
                    fprintf(messages, "XMIT: No reply.\n");
                if(count == attempts)
                    return ACERR_RX_FAILURE;
            // Check for an unexpected reply
            }else if(c_flag){
                if(messages)
                    fprintf(messages, "XMIT: Expected %d bytes, received %d.\n", reply, rxlength);
                if(count == attempts)
                    return ACERR_RX_LENGTH;
            // If the reply appears valid, compare checksums and length
            }else{
                rxcs16 = checksum16(rxbuffer, &rxlength);
                rxcs8 = checksum8(rxbuffer);
                // Verify the packet length matches the received length
                c_flag = (rxlength != reply);
                if(c_flag){
                    if(messages)
                        fprintf(messages, "XMIT: Aborting. Packet length (%d) does not match the expected length (%d).\n", rxlength, reply);
                    // Automatic failure; do not re-attempt
                    return ACERR_RX_LENGTH;
                // Check the checksum 8
                }else if(rxcs8 != rxbuffer[0]){
                    c_flag = 1;
                    if(messages)
                        fprintf(messages, "XMIT: Bad checksum 8. Received 0x%02x. Calculated 0x%02x.\n", rxbuffer[0], rxcs8);
                    if(count == attempts)
                        return ACERR_BAD_CHECKSUM;
                // Check the checksum 16 buffer
                }else if(rxcs16 >= 0 && rxcs16 != *(uint16_t*) &rxbuffer[4]){
                    c_flag = 1;
                    if(messages)
                        fprintf(messages, "XMIT: Bad checksum 16.  Received 0x%04x.  Calculated 0x%04x.\n", *(uint16_t*) &rxbuffer[4], rxcs16);
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

/* ACMESSAGE - set how error messages will be communicated
 *
 *  TARGET is a file pointer to which descriptive error messages should
 *  be streamed.  It is set to NULL by default, but can be redirrected
 *  as desired.  If it is set to NULL, then error messaging is disabled.
 */
acerror_t acmessage(FILE* target){
    messages = target;
}

/* ACINIT - open the device connection
 * 
 *  DEV is the device struct.  dev->handle must be set to NULL before
 *  initialization, or an ACERR_ALREADY_OPEN error will be raised.
 * 
 *  Errors:
 *  ACERR_ALREADY_OPEN  The device handle (dev->handle) was not NULL.
 *  ACERR_OPEN_FAILED   The LJUSB_OpenDevice failed.
 */
acerror_t acinit(acdev_t *dev){
    acerror_t err;
    
    // Test for an existing connection
    if(dev->handle){
        if(messages)
            fprintf(messages, "ACINIT: Device connection appears to be already open.\n");
        return ACERR_ALREADY_OPEN;
    }
    
    // Find a U3; it should be the only LJ device.
    dev->handle = LJUSB_OpenDevice(1, 0, U3_PRODUCT_ID);
    if(!dev->handle){
        if(messages)
            fprintf(messages, "ACINIT: Open operation failed. Check device connection.\n");
        return ACERR_OPEN_FAILED;
    }
    
    // Use ConfigU3 command to retrieve version information
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x0A;
    txbuffer[3] = 0x08;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x00;
    err = xmit(dev->handle,1,38);
    if(err){
        if(messages)
            fprintf(messages, "ACINIT: Failed to read device version information.\n");
        return err;
    }
    
    dev->firmware_version = rxbuffer[10] + ((float) rxbuffer[9]) / 100;
    dev->bootloader_version = txbuffer[12] + ((float) rxbuffer[11]) / 100;
    dev->hardware_version = rxbuffer[14] + ((float) rxbuffer[13]) / 100;
    dev->serial_number = *((unsigned int*) &rxbuffer[15]);
    
    // Get calibration information
    // Use the ReadMem command to read the analog input calibration
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x01;
    txbuffer[3] = 0x2D;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x00;   // Block 0
    err = xmit(dev->handle,1,40);
    if(err){
        if(messages)
            fprintf(messages, "ACINIT: Failed while loading device analog input calibration.\n");
        return err;
    }
    
    dev->ain_slope_v = f64_to_double(&rxbuffer[8]);
    dev->ain_offset_v = f64_to_double(&rxbuffer[16]);
    
    // Use the ReadMem command to read the temperature calibration
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x01;
    txbuffer[3] = 0x2D;
    txbuffer[6] = 0x00;
    txbuffer[7] = 0x02;   // Block 2
    err = xmit(dev->handle,1,40);
    if(err){
        if(messages)
            fprintf(messages, "ACINIT: Failed while loading device analog input calibration.\n");
        return err;
    }
    
    dev->temp_slope_K = f64_to_double(&rxbuffer[8]);
    
    // Use ConfigIO to set the EIOAnalog register
    txbuffer[1] = 0xF8;
    txbuffer[2] = 0x03;
    txbuffer[3] = 0x0B;
    txbuffer[6] = 0x0c;
    txbuffer[7] = 0x00;
    txbuffer[10] = 0x0F;
    txbuffer[11] = AC_EIOAIN_MASK;
    err = xmit(dev->handle,4,12);
    if(err){
        if(messages)
            fprintf(messages, "ACINIT: Failed while setting the FIO/EIO IO settings.\n");
        return err;
    }
    buffer_dump(rxbuffer);
    return ACERR_NONE;
}


acerror_t acclose(acdev_t *dev){
    if(dev->handle){
        LJUSB_CloseDevice(dev->handle);
        dev->handle = NULL;
    }
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
    printf("AIN slope (v/bit): %.6e\nAIN offset (v): %.6e\n",
        dev->ain_slope_v,
        dev->ain_offset_v);
    printf("Temp slope (K/bit): %.6e\n",
        dev->temp_slope_K);
        
    return ACERR_NONE;
}