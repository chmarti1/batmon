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
#define AC_CHANNELS             3
#define AC_AICLOCK_HZ           48e6
#define AC_BITSTATE_MASK        0x80        // Pin 7 is the bit state/direction bit in the bit direction/state commands
#define AC_EIO_OFFSET           8           // The EIO pins are 8-15, so there is an offset of 8 for the pin numbers.
#define AC_STRLEN               128         // The longest string length allowed

typedef enum _acchannel_t {
    ACCHAN_CURRENT = 0,
    ACCHAN_VOLTAGE = 1,
    ACCHAN_TEMPERATURE = 2,
} acchannel_t;

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
    ACERR_LSD_FILE =        0x22,       // Could not open the log, stat, or datafile

    // Init/Open Errors
    ACERR_DEV_ALREADY_OPEN = 0x30,      // Init was run twice
    ACERR_OPEN_FAILED =     0x31,       // Open operation failed
    ACERR_CONFIG_FAILED =   0x32,       // Failed while configuring IO
    
    // Stream Errors
    ACERR_AICONFIG_FAILED = 0x40,
    ACERR_AISTART_FAILED =  0x41,
    ACERR_AIREAD_FAILED =   0x42,
    ACERR_AISTOP_FAILED =   0x43,
    
} acerror_t;


typedef enum _acloglevel_t{
    ACLOG_ESSENTIAL = -1,
    ACLOG_LOW = 0,
    ACLOG_MEDIUM = 1,
    ACLOG_HIGH = 2,
} acloglevel_t;

// These are masks constructed from the relevant EIO pin numbers
// --> EIO Analog input mask 8-bits with 1 for AIN pins
#define AC_EIOAIN_MASK  (1<<ACPIN_CS | 1<<ACPIN_VS) // These are the analog
// --> EIO Digital output mask 8-bits with 1 for digital outputs
#define AC_EIODOUT_MASK (1<<ACPIN_ALARM | 1<<ACPIN_CS | 1<<ACPIN_IND0 | 1<<ACPIN_IND1 | 1<<ACPIN_IND2 | 1<<ACPIN_IND3)


/* ACDEV_T - The device struct
 * 
 */
typedef struct _acdev_t {
    // Device handle
    HANDLE handle;
    // Files
    char logfile[AC_STRLEN];
    char statfile[AC_STRLEN];
    char datafile[AC_STRLEN];

    // Log level
    acloglevel_t loglevel;

    // Descriptive
    float firmware_version;
    float bootloader_version;
    float hardware_version;
    unsigned int serial_number;
    // Calibration
    // Device calibration
    double ain_slope;
    double ain_offset;
    // Device temperature calibration
    double temp_slope;
    // Current
    double current_slope;
    double current_zero;
    // Voltage
    double voltage_slope;
    double voltage_zero;
    
    // data record interval
    double tdata;
    // AI sample interval
    double tsample;
    // AI sample count
    uint8_t Nsample;
    
    // Stream status parameters
    unsigned int aistr_backlog;
    unsigned int aistr_active:1;
} acdev_t;


/* ACMESSAGE_SEND - Log and send a message
 *  acmessage_send pushes text to a log location.  If no log file has 
 * been configured for the device, then stdout is used.  If the 
 * dev->logfile path is defined, it attempts to open the file, append to
 * it, and close the file.  If that process fails, a message is sent to 
 * stderr and the original message is sent to stdout.
 * 
 * Messages will have an integer "[TIMESTAMP]" prepended (seconds since 
 * the epoch), and a newline appended.  
 * 
 * The message's log level is specified by the "level" enum.  Messages
 * that are higher than the devices configured dev->loglevel value will
 * be ignored.  Messages with lower values will be sent.  Negative 
 * values will always be sent.
 */
acerror_t acmessage(acdev_t *dev, char *text, acloglevel_t level);

/* ACCONFIG - Load a device configuration file
 * 
 * This initializes the device configuration struct and all its members
 * (including the battery model) before loading the configuration file.
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


/* ACSHOW - Prints a summary of configuration and device parameters to
 *  stdout.
 */
acerror_t acshow(acdev_t *dev);

/* ACSET - Set a digital output
 * 
 * The pin enumerated type indicates the pin number to set. The integer
 * is interpreted as True for any non-zero value and False for zero.
 */
acerror_t acset(acdev_t *dev, acpin_t pin, int value);

/* ACGET - Get an analog input
 * 
 * The pin enumerated type indicates which input to measure. VALUE is 
 * used to return the result in the appropriate calibrated units.  
 *      Pin     unit    Description
 *  ACPIN_CS    (A)     Current signal
 *  ACPIN_VS    (V)     Terminal voltage signal
 *  ACPIN_T     (K)     Temperature
 */
acerror_t acget(acdev_t *dev, acpin_t pin, double *value);

/* ACSTREAM_START - Starts continuous analog measurements
 */
acerror_t acstream_start(acdev_t *dev, double sample_hz, uint8_t samples_per_packet);

acerror_t acstream_read(acdev_t *dev, double *data);

acerror_t acstream_stop(acdev_t *dev);

/* ACCAL - Get Current, Voltage, and Temperature
 * 
 * Accepts the data array from the AC_STREAM_READ() function.  
 * Calculates mean voltage, current, and temperature over the period of
 * data collection (should be one line cycle).
 */
acerror_t acmean(acdev_t* dev, double* data, double* I, double* V, double* T);
#endif
