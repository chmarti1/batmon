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

// Analog stream prameters
#define AC_SAMPLES_PER_READ     5
#define AC_CHANNELS             3
#define AC_PACKET               AC_SAMPLES_PER_READ * AC_CHANNELS
typedef enum _acchannel_t {
    ACCHAN_CURRENT = 0,
    ACCHAN_VOLTAGE = 1,
    ACCHAN_TEMPERATURE = 2,
} acchannel_t;

#define AC_EIO_OFFSET  8

/* ACPIN_T - The pin number type
 *  Establishes constants for the pin numbers
 */
typedef enum _acpin_t {
    ACPIN_NONE =    -1,
    ACPIN_CS =      0,
    ACPIN_VS =      1,
    ACPIN_ALARM =   2,
    ACPIN_IND0 =    3,
    ACPIN_IND1 =    4,
    ACPIN_IND2 =    5,
    ACPIN_IND3 =    6,
    ACPIN_T =       30 - AC_EIO_OFFSET,
} acpin_t;

// Build an EIO Analog mask
#define AC_EIOAIN_MASK  (1<<ACPIN_CS | 1<<ACPIN_VS)
#define AC_BITSTATE_MASK  0x80

#define AC_ADCCLK_HZ    15625.0
#define AC_ADCCLK_DIVISOR

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
    ACERR_PARAM_ERROR =     0x16,       // A function argument was illegal

    // Configuration errors
    ACERR_CONFIG_FILE =     0x20,       // Configuration file was not opened
    ACERR_CONFIG_SYNTAX =   0x21,       // Illegally formatted configuration file
    ACERR_LOG_FILE =        0x22,       // Could not open the logfile
    ACERR_STAT_FILE =       0x23,       // Could not open the stat file

    // Init/Open Errors
    ACERR_DEV_ALREADY_OPEN =    0x30,       // Init was run twice
    ACERR_OPEN_FAILED =     0x31,       // Open operation failed
    ACERR_CONFIG_FAILED =   0x32,       // Failed while configuring IO
    
    // Stream Errors
    ACERR_AICONFIG_FAILED = 0x40,       
    ACERR_AISTART_FAILED =  0x41,
    ACERR_AIREAD_FAILED =   0x42,
    ACERR_AISTOP_FAILED =   0x43,
    
} acerror_t;

/* ACDEV_T - The device struct
 * 
 *  The device struct tracks values relevant to the DAQ. Below is a list
 * of the members, a description of their purpose, and lists of the 
 * functions responsible for initializing, setting, and that depend on
 * the member's value.
 * 
 * handle
 *  initialized: application
 *  set: acinit, acclose
 *  used: all communication
 * 
 * The handle is a pointer used by the LJUSB driver as a device handle. 
 * It should be set to NULL to indicate that the connection is closed. 
 * Otherwise, it will be set to a value returned by the LJUSB_OpenDevice 
 * function.  The application is responsible for initializing it to NULL
 * to prevent an error in acinit().
 * 
 * firmware_version
 * bootloader_version
 * hardware_version
 * serial_number
 *  initialized: acinit
 *  set: acinit
 *  used: application
 * 
 * The version numbers are floating point numbers indicating the various
 * versions reported by the U3.  The serial number is an unsigned 
 * integer indicating the device serial number reported by the U3.
 * These are not used by the interface, but are available to the 
 * application if needed.
 * 
 * ain_slope_v
 * ain_offset_v
 * temp_slope_K
 *  initialized: acinit
 *  set: acinit
 *  used: acstream_read
 * 
 * In the initialization process, the device's calibration memory is
 * queried for its raw calibration coefficients.  These are used to 
 * convert ADC counts into analog voltage measured at the terminals and
 * temperature reported by the internal temperature sensor.  The U3 has
 * separate coefficients for differential and single-ended measurements;
 * these are for single-ended measurements.  The units are in 
 * volts/count for slope, volts for offset, and K/count for temperature
 * slope.
 * 
 * current_slope_av
 * current_zero_v
 * voltage_slope_nd
 * voltage_zero_v
 *  initialized: application
 *  set: application
 *  used: acstream_read
 * 
 * The application is responsible for setting values for 
 */
typedef struct _acdev_t {
    // Device handle
    HANDLE handle;
    // Files
    FILE *logfile;
    FILE *statfile;

    // Descriptive
    float firmware_version;
    float bootloader_version;
    float hardware_version;
    unsigned int serial_number;
    // Calibration
    // Device calibration
    double ain_slope_v;
    double ain_offset_v;
    // Device temperature calibration
    double temp_slope_K;
    // Current
    double current_slope_av;
    double current_zero_v;
    // Voltage
    double voltage_slope_nd;
    double voltage_zero_v;
    // Stream status parameters
    unsigned int aistr_backlog;
    unsigned int aistr_active:1;
} acdev_t;


/* ACMESSAGE_SET - Configure a file stream for error messages
 *  Internal error messages are written to two file streams when they
 * occur: to the log file specified in configuration and to a message
 * stream that can be specified by acmessage_set().  By default, it is
 * inactive, but many applications will want to set it to stderr.
 * 
 * Unlike the log file, which is device-specific, the message stream is
 * a global setting for autocom.  All messages will go to the same 
 * stream, while each device receives its own log file.
 * 
 * See acmessage_send() for more information.
 */
acerror_t acmessage_set(FILE* target);

/* ACMESSAGE_SEND - Log and send a message
 *  acmessage_send pushes text to two file streams: the log file 
 * specified in configuration and the messages stream specified by 
 * acmessage_set().  Both are inactive by default, so in those cases,
 * acmessage_send() will do nothing.  The messages stream will usually
 * be set to stderr or stdout, and the log stream will usually be 
 * directed to a log file in /var/log somewhere.
 * 
 * Messages sent to the messages stream and the log file will have a 
 * newline appended, and messages sent to the log file will have a 
 * timestamp prepended in brackets.  The timestamp uses C calendar time
 * (seconds since the epoch).  The intent is that messages will be read
 * as they are generated, but logs will be reviewed after-the-fact, so
 * users will be able to reconstruct when and how failures happened.
 * 
 * acmessage_send() is used internally, but it is also exposed to the
 * application, so high-level messages may be mixed with internal ones
 * for more context for the error.
 */
acerror_t acmessage_send(acdev_t *dev, char *text);

/* ACCONFIG - Load a device configuration file
 */
acerror_t acconfig(acdev_t *dev, char *filename);

/* ACOPEN - Open a connection to the device
 */
acerror_t acopen(acdev_t *dev);

/* ACINIT - Initialize the U3
 *  Configures the U3 for use with the battery monitor system once the
 * connection is already open. The process is:
 *  (1) Configures ALARM and IND0-IND3 pins for output and 
 *      sets all High while setting the U3 LED off
 *  (2) Sets ALARM and IND0-IND3 Low and turns the U3 LED on.
 * If there is an error in the initialization phase and it cannot 
 * complete, the alarm and LEDs will remain lit to warn the user.
 * 
 * Each of these processes requires that a packet be transmitted over
 * USB and a response packet must be received.  
 * 
 * For information on errors and warnings, be sure to set the messages
 * file pointer using the ACMESSAGE() function.
 * 
 * The ACINIT function returns error codes:
 * ACERR_DEV_ALREADY_OPEN
 *      It appears that the device connection is already open. This 
 *      means dev.handle is not NULL, so if the connection is not 
 *      actually open, just add a line of code setting dev.handle to 
 *      NULL.
 * ACERR_OPEN_FAILED    
 *      Failed to open a connection to a U3 device.
 * ACERR_CORRUPT_BUFFER 
 *      Some aspect of the packet being transmitted or received is 
 *      illegally formatted.  Maybe the packet length specified in the
 *      header is illegal, or some other aspect is unexpected.
 * ACERR_TX_FAILED
 * ACERR_RX_FAILED
 *      TX and RX errors occur when either step of the transaction fails
 *      without a clear reason.  Maybe the connection is faulty, or the
 *      device is unresponsive?
 * ACERR_BAD_CHECKSUM
 *      Either the device reported a checksum error or a checksum check
 *      on the device's response has failed.
 * ACERR_CONFIG_FAILED
 *      The transmission succeeded, but one of the configuration steps
 *      returned an error code. Turn on messages with ACMESSAGE() for
 *      more details.
 */
acerror_t acinit(acdev_t *dev);

/* ACCLOSE - Close the U3 connection
 *  First, the function transmits a packet to turn off all LEDs and the
 * alarm.  If that fails, closure is not aborted.
 */
acerror_t acclose(acdev_t *dev);

acerror_t acshow(acdev_t *dev);

acerror_t acset(acdev_t *dev, acpin_t pin, int value);

acerror_t acget(acdev_t *dev, acpin_t pin, double *value);

acerror_t acstream_start(acdev_t *dev);

acerror_t acstream_read(acdev_t *dev, double *data);

acerror_t acstream_stop(acdev_t *dev);

#endif
