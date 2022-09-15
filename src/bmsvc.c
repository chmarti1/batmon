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


#define ACCONFIG    "com.conf"
#define BATCONFIG   "bat.conf"
#define SVCCONFIG   "service.conf"
#define STRLEN      128
#define SAMPLE_HZ   600.
#define SAMPLE_N    5


/* SVC_T is a struct for configuring the batmon service
 */
typedef struct _svc_t {
    unsigned int datainterval;   // interval between data entries in seconds
    char acconfig[STRLEN];  // autocom configuration file
    char cmconfig[STRLEN];  // charge model configuration file
    char statfile[STRLEN];  // Status file
    char datadir[STRLEN];   // Directory in which to create data files
} svc_t;


int svcconfig(svc_t *svc, char *filename);

int main(int argc, char* argv[]){
    acdev_t dev;
    int err;
    cmbat_t bat;
    double data[25];
    double I=0, V=0, T=0, I2, V2, T2;
    struct timespec start, stop;
    int ii;
    
    
    /* OPEN CONFIG FILES */
    err = (int) acconfig(&dev, ACCONFIG);
    if(err){
        printf("BMSVC: Received error (%02x) while parsing com configuration file: %s\n", err, ACCONFIG);
        return -1;
    }
    err = cmconfig(&bat, BATCONFIG);
    if(err){
        printf("BMSVC: Failed while parsing battery configuration file: %s\n", BATCONFIG);
        return -1;
    }
    
    err = acopen(&dev);
    err = acstream_stop(&dev);
    printf("%d\n", err);
    printf("  %f\n", dev.hardware_version);
    err = acinit(&dev);
    printf("%d\n", err);
    err = acstream_start(&dev, SAMPLE_HZ, SAMPLE_N);
    printf("%d\n", err);

    for(ii=0; ii<20; ii++){
        err = acstream_read(&dev, data);
        err = acmean(&dev, data, &I, &V, &T);
        err = acstream_read(&dev, data);
        err = acmean(&dev, data, &I2, &V2, &T2);
        I = 0.5*(I+I2);
        V = 0.5*(V+V2);
        T = 0.5*(T+T2);
        printf("I(A): %f\nV(V): %f\nT(K): %f\n", I, V, T);
    }

    printf("Tsample (sec, usec): %ld, %ld\n", stop.tv_sec, stop.tv_nsec/1000);
    err = acstream_stop(&dev);
    err = acclose(&dev);
    return 0;
}
