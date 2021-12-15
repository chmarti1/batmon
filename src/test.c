#include "autocom.h"
#include <unistd.h>


int main(int argc, char* argv[]){
    acdev_t dev;
    acerror_t err;
    acmessage_set(stderr);
    double data[AC_PACKET];
    unsigned int ii;
    
    printf("Config\n");
    acconfig(&dev, "./test.bmc");
    printf("Open\n");
    acopen(&dev);
    printf("Init\n");
    acinit(&dev);
    printf("Start stream\n");
    acstream_start(&dev);
    printf("Read stream\n");
    acstream_read(&dev, data);
    printf("Stop stream\n");
    acstream_stop(&dev);
    printf("Close\n");
    acclose(&dev);
    printf("Show\n");
    acshow(&dev);

    for(ii = 0; ii<AC_PACKET; ii++){
        printf("  %f\n", data[ii]);
    }
    return 0;
}
