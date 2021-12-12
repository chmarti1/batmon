#include "autocom.h"


int main(int argc, char* argv[]){
    acdev_t dev;
    acerror_t err;
    acmessage_set(stderr);
    
    acconfig(&dev, "./test.bmc");
    
    printf("current = (v - %lf) * %lf\n", dev.current_zero_v, dev.current_slope_av);
    printf("voltage = (v - %lf) * %lf\n", dev.voltage_zero_v, dev.voltage_slope_nd);
    
    if(err = acinit(&dev))
        printf("ACINIT failure: 0x%x\n", err);
    acclose(&dev);
    acshow(&dev);

    return 0;
}
