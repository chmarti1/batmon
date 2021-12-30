#include "autocom.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BATTERY_CONFIG "/etc/batmon/battery.conf"
#define MEASURE_CONFIG "/etc/batmon/measure.conf"
#define streq(a,b) (strcmp(a,b)==0)




int main(int argc, char* argv[]){
    int ii;
    int itemp;
    double ftemp;
    char stemp[128];
    acerror_t err;
    acdev_t dev;
    char help_text[] = \
"BMCMD - Battery monitor command\n"\
"\n"\
"bmcmd [COMMAND [ARGUMENT [ARGUMENT...]]] [COMMAND ...]\n"\
"\n"\
"  Used to interact with the battery monitor system from the command \n"\
"  line. Parses whitespace-separated commands from left-to-right. Some\n"\
"  commands accept arguments that define their behavior.\n"\
"\n"\
"**COMMANDS**\n"\
"help\n"\
"  Prints this help text, and does not exit. Requires no arguments.\n"\
"\n"\
"init\n"\
"  Initializes the DAQ.  Requires no arguments.\n"\
"  This includes setting all the pin I/O settings and setting the digital\n"\
"  outputs to be off.\n"\
"\n"\
"alarm [on|off]\n"\
"  Sets the alarm status. Requires a single argument \"on\" or \"off\".\n"\
"\n"\
"ind[n] [on|off]\n"\
"  Sets the state of one of the indicator LEDs. Requires a single argument\n"\
"  \"on\" or \"off\". n should be replaced with the indicator index (0-3)\n"\
"    e.g. bmcmd ind0 on\n"\
"\n"\
"voltage\n"\
"  Return the terminal voltage measured\n"\
"\n"\
"current\n"\
"  Return the current measured. Positive current is leaving the battery.\n"
"\n"
"temperature\n"\
"show\n"\
"\n"

    acmessage_set(stderr);
    // Open the configuration file
    if(err = acconfig(&dev, MEASURE_CONFIG)){
        sprintf(stemp, "BMCMD: Configuration failed with error 0x%x", err);
        acmessage_send(&dev, stemp);
        return -1;
    }
    // Open the connection
    if(err = acopen(&dev)){
        sprintf(stemp, "BMCMD: Failed to open device connection with error 0x%x", err);
        acmessage_send(&dev, stemp);
        return -1;
    }
    // Loop over the parameters
    for(ii=1; ii<argc; ii++){
        err = ACERR_NONE;
        // HELP
        if(streq(argv[ii], "help")){
            sprintf(help_text);
        // INIT
        }else if(streq(argv[ii], "init")){
            err = acinit(&dev);
        // ALARM
        }else if(streq(argv[ii], "alarm")){
            // Check for the parameter
            if(ii >= argc){
                acmessage_send(&dev, "BMCMD: ALARM requires an ON or OFF argument");
                acclose(&dev);
                return -1;
            }
            ii ++;
            if(streq(argv[ii], "on"))
                err = acset(&dev, ACPIN_ALARM, 1);
            else if(streq(argv[ii], "off"))
                err = acset(&dev, ACPIN_ALARM, 0);
        // IND
        }else if(1==sscanf(argv[ii], "ind%d", &itemp)){
            // Make sure the value is legal
            if(itemp < 0 || itemp>3){
                sprintf(stemp, "BMCMD: Only IND0 - IND3 are valid. Received: %s", argv[ii]);
                acmessage_send(&dev, stemp);
                acclose(&dev);
                return -1;
            // Check for the parameter
            }else if(ii >= argc){
                acmessage_send(&dev, "BMCMD: INDx requires an ON or OFF argument");
                acclose(&dev);
                return -1;
            }
            ii ++;
            if(streq(argv[ii], "on"))
                err = acset(&dev, itemp + 3, 1);
            else if(streq(argv[ii], "off"))
                err = acset(&dev, itemp + 3, 0);
        // VOLTS
        }else if(streq(argv[ii], "voltage")){
            if(err = acget(&dev, ACPIN_VS, &ftemp)){
                sprintf(stemp, "BMCMD: Voltage measurement failed with error (0x%02x).", err);
                acmessage_send(&dev, stemp);
                acclose(&dev);
                return -1;
            }
            printf("%.4fV\n", ftemp);
        // AMPS
        }else if(streq(argv[ii], "current")){
            if(err = acget(&dev, ACPIN_CS, &ftemp)){
                sprintf(stemp, "BMCMD: Current measurement failed with error (0x%02x).", err);
                acmessage_send(&dev, stemp);
                acclose(&dev);
                return -1;
            }
            printf("%.3fA\n", ftemp);
        // TEMP
        }else if(streq(argv[ii], "temperature")){
            if(err = acget(&dev, ACPIN_T, &ftemp)){
                sprintf(stemp, "BMCMD: Temperature measurement failed with error (0x%02x).", err);
                acmessage_send(&dev, stemp);
                acclose(&dev);
                return -1;
            }
            printf("%.2fK\n", ftemp);
        // SHOW
        }else if(streq(argv[ii], "show")){
            acshow(&dev);
        // WAIT
        }else if(streq(argv[ii], "wait")){
            if(ii >= argc){
                acmessage_send(&dev, "BMCMD: The WAIT command requires an argument; number of seconds.");
                acclose(&dev);
                return -1;
            }else if(1!=sscanf(argv[++ii], "%lf", &ftemp)){
                sprintf(stemp, "BMCMD: WAIT expects number of seconds. Found: %s", argv[ii]);
                acmessage_send(&dev, stemp);
                acclose(&dev);
                return -1;
            }
            usleep((int) (ftemp * 1e6));
        // STOP
        }else if(streq(argv[ii], "stop")){
            err = acstream_stop(&dev);
        }else{
            sprintf(stemp,  "BMCMD: Unrecognized command: %s", argv[ii]);
            acmessage_send(&dev, stemp);
            acclose(&dev);
            return -1;
        }
        
        // Test for an autocom error
        if(err){
            sprintf(stemp, "BMCMD: Encountered error (0x%02x) at argument %d, %s.", err, ii, argv[ii]);
            acmessage_send(&dev, stemp);
            acclose(&dev);
            return -1;
        }
    }
    
    acclose(&dev);
    return 0;
}
