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

//#define BMSVC_CONFIG    "/etc/bmsvc.conf"
#define BMSVC_CONFIG    "bmsvc.conf"
#define BMSVC_SAMPLES   5

int ndifftime(struct timespec *result, struct timespec *stop, struct timespec *start){
    if(start->tv_nsec > stop->tv_nsec){
        result->tv_nsec = (1000000000 - start->tv_nsec) + stop->tv_nsec;
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
    }else{
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
        result->tv_sec = stop->tv_sec - start->tv_sec;
    }
    return 0;
}

int main(int argc, char* argv[]){
    acdev_t dev;
    acerror_t err;
    double data[25];
    double I=0, V=0, T=0, It, Vt, Tt;
    struct timespec start, stop;
    int ii;
    
    memset(data, 0, sizeof(data));
    
    err = acconfig(&dev, BMSVC_CONFIG);
    printf("%d\n", err);
    err = acopen(&dev);
    err = acstream_stop(&dev);
    printf("%d\n", err);
    printf("  %f\n", dev.hardware_version);
    err = acinit(&dev);
    printf("%d\n", err);
    err = acstream_start(&dev, 600., 5);
    printf("%d\n", err);
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(ii=0; ii<20; ii++){
        err = acstream_read(&dev, data);
        err = acmean(&dev, data, &I, &V, &T);
        err = acstream_read(&dev, data);
        err = acmean(&dev, data, &It, &Vt, &Tt);
        I = 0.5*(I+It);
        V = 0.5*(V+Vt);
        T = 0.5*(T+Tt);
        printf("I(A): %f\nV(V): %f\nT(K): %f\n", I, V, T);
    }
    clock_gettime(CLOCK_MONOTONIC, &stop);
    ndifftime(&stop, &stop, &start);

    printf("Tsample (sec, usec): %ld, %ld\n", stop.tv_sec, stop.tv_nsec/1000);
    err = acstream_stop(&dev);
    err = acclose(&dev);
    return 0;
}
