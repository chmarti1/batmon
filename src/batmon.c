#include "chargemodel.h"
#include "autocom.h"
#include <stdio.h>
#include <time.h>
#include <signal.h>

#define CONFIG "/etc/batmon.conf"
#define TS_MIN  0.01
#define TS_MAX  10.0

char go_f;      // A global flag indicating whether a TERM signal was received

// A TERM signal handler function
void term_handler(int signum){
    acmessage_send("Caught SIGTERM");
    go_f = 0;
}


int main(int argc, char *argv[]){
    acdev_t dev;
    unsigned int packets_per_datum,     // Number of stream packets between data entries
                packet_count;           // Keep track of the number of packets so far
    int ii;
    double buffer[AC_SAMPLES_PER_PACKET];
    double I, Vt, Vl, Voc, T;
    time_t now;
    FILE* fd;
    char datafile_f;
    
    // Parse the configuration file
    if(acconfig(&dev, CONFIG)){
        fprintf(stderr, "FATAL: Failed to parse configuration file.\n");
        return -1;
    }
    
    // Check the sample intervals for sanity
    if(dev->ts < TS_MIN || dev->ts > TS_MAX){
        acmessage_set(stderr);
        acmessage_send("FATAL: Sample interval (ts) was out of bounds (%f, %f): %lf", TS_MIN, TS_MAX, dev->ts);
        return -1;
    }
    // Calculate the packets per datum
    packets_per_datum = (unsigned int) (dev->tdata / (dev->ts * AC_SAMPLES_PER_PACKET));
    // Update tdata to reflect the rounding
    dev->tdata = (double) (packets_per_data * dev->ts * AC_SAMPLES_PER_PACKET);
    // Log the startup time
    acmessage_send("BATMON started.");
    
    // Open the connection
    if(acopen(&dev)){
        acmessage_send("FATAL: Failed to connect to device.");
        return -1;
    }
    
    // Start the data stream
    if(acstream_start(&dev)){
        acmessage_send("FATAL: Failed to start data stream.");
        acclose(&dev);
        return -1;
    }
    
    // Set up the handler for the TERM signal
    signal(SIGTERM, term_handler);
    
    // Initialize the relevant counter(s)
    packet_count = 0;
    datafile_f = 1;
    
    // Start the service loop
    // It will exit with a TERM signal
    go_f = 1;
    while(go_f){
        // Read in a packet (blocking)
        acstream_read(&dev, &buffer);
        packet_count++;
        // process the new data
        for(ii = 0; ii<AC_SAMPLES_PER_PACKET; ii++){
            I = buffer[3*ii];
            Vt = buffer[3*ii + 1];
            T = buffer[3*ii + 2];
            // Get the terminal losses and update the dynamic resistance model
            Vl = cmstep(&dev->battery, I, Vt);
            // Calculate the OCV
            Voc = Vt - Vl;
            // If this is a datum that should be recorded
            if(packet_count >= packets_per_datum){
                // Restart the count
                packet_count = 0;
                // What time is it?
                now = time(NULL);
                // Append to the data file
                if(fd = fopen(dev->datafile, "a")){
                    fprintf(fd, "[%d] %16.6lf%16.6lf %16.6lf\n", (unsigned int) now, I, Vt, Voc);
                // If this is the first datafile failure, report it
                }else if(datafile_f){
                    datafile_f = 0;
                    acmessage_send(&dev, "Failed to write to data file.");
                    acmessage_send(&dev, dev->datafile);
                    acmessage_send(&dev, "This message will not be repeated.");
                }
            }
        }
    }
    acmessage_send("BATMON stopping.");
    // Stop the stream and close
    acstream_stop(&dev);
    acclose(&dev);
    
    return 0;
}

