#include "autocom.h"


int main(int argc, char* argv[]){
    acdev_t dev;
    acerror_t err;
    // Initialize the handle safely
    dev.handle = NULL;
    
    acmessage(stderr);
    if(err = acinit(&dev))
        printf("ACINIT failure: 0x%x\n", err);
    acclose(&dev);
    acshow(&dev);

    return 0;
}
