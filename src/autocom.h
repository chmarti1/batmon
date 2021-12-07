/* AUTOCOM.H
 * 
 * Header for wrapper codes to automate communication operations with 
 * the LabJack U3.  These are written to be specific to the BatMon 
 * application.
 */


#ifndef __AUTOCOM_H__
#define __AUTOCOM_H__

#include <labjackusb.h>
#include <stdio.h>
#include <stdint.h>


/* ACPIN_T - The pin number type
 *  Establishes constants for the pin numbers
 */
typedef enum _acpin_t {
    ACPIN_NONE = -1,
    ACPIN_CS = 0,
    ACPIN_VS = 1,
    ACPIN_ALARM = 2,
    ACPIN_IND0 = 3,
    ACPIN_IND1 = 4,
    ACPIN_IND2 = 5,
    ACPIN_IND3 = 6,
} acpin_t;

// Build an EIO Analog mask
#define AC_EIOAIN_MASK  (1<<ACPIN_CS | 1<<ACPIN_VS)
#define AC_EIO_OFFSET  8
#define AC_BITSTATE_MASK  0x80



/* ACERROR_T- the error code type
 *  Establishes constants for function return values
 */
typedef enum _acerror_t {
    ACERR_NONE =            0,          // Everything is OK
    // Generic Errors
    ACERR_DEV_NOT_OPEN =    0x10,       // The device handle is NULL
    ACERR_BAD_CHECKSUM =    0x11,       // There was a bad checksum
    ACERR_CORRUPT_BUFFER =  0x12,       // Some aspect of the data in the buffer is invalid
    ACERR_TX_FAILURE =      0x13,       // Transmit or receive failed 
    ACERR_RX_FAILURE =      0x14,       //   
    ACERR_RX_LENGTH =       0x15,       // Reply length was unexpected

    // Init/Open Errors
    ACERR_ALREADY_OPEN =    0x20,       // Init was run twice
    ACERR_OPEN_FAILED =     0x21,       // Open operation failed
    ACERR_CONFIG_FAILED =   0x22,       // Failed while configuring IO
    
} acerror_t;


typedef struct _acdev_t {
    HANDLE handle;
    float firmware_version;
    float bootloader_version;
    float hardware_version;
    unsigned int serial_number;
    double ain_slope_v;
    double ain_offset_v;
    double temp_slope_K;
} acdev_t;



acerror_t acmessage(FILE* target);

/* ACINIT
 *  Opens a connection to the U3 and initializes the relevant 
 * configuration settings.
 * 
 */
acerror_t acinit(acdev_t *dev);


acerror_t acclose(acdev_t *dev);

acerror_t acshow(acdev_t *dev);

#endif
