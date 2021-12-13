#include "autocom.h"
#include <stdio.h>
#include <string.h>

#define BATTERY_CONFIG "/etc/batmon/battery.conf"
#define MEASURE_CONFIG "/etc/batmon/measure.conf"
#define streq(a,b) (strcmp(a,b)==0)




int main(int argc, char* argv[]){
    int ii;
    int itemp;
    char stemp[128];
    acerror_t err;
    acdev_t dev;

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
        // INIT
        if(streq(argv[ii], "init")){
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
        }
        
        // Test for an autocom error
        if(err){
            sprintf(stemp, "BMCMD: Encountered error (0x%02x) at argument %d, %s.", err, ii, argv[ii]);
            acmessage_send(&dev, stemp);
            return -1;
        }
    }
    
    acclose(&dev);
    return 0;
}
