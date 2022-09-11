#include "chargemodel.h"
#include "autocom.h"
#include <stdio.h>
#include <time.h>
#include <signal.h>

#define CONFIG "/etc/batmon.conf"
#define TS_MIN  0.01
#define TS_MAX  10.0

char go_f;      // A global flag indicating whether a TERM signal was received
acdev_t dev;    // Device struct is gloal so the handler can access it

// A TERM signal handler function
void term_handler(int signum){
    acmessage_send(&dev, "Caught SIGTERM");
    go_f = 0;
}

// A helper function for writing to the data file
int datawrite(acdev_t *dev, char type){
    time_t now;
    FILE *fd;
    static char datafile_f = 1;
    now = time(NULL);
    if(!(fd = fopen(dev->datafile,"a"))){
        if(datafile_f){
            acmessage_send(dev, "Failed to write datum to data file:");
            acmessage_send(dev, dev->datafile);
            acmessage_send(dev, "This message will not be repeated.");
            datafile_f = 0;
        }
        return -1;
    }
    // [time]E S cs Imean Istd Imax Imin Vt Voc Q T
    fprintf(fd, "[%11d]%c %c%8.4f%8.2f%8.2f%8.2f%8.2f%8.4f%8.4f%14.6e%6.1f\n",\
        (unsigned int) now,
        type,
        (char) dev->battery.chargestate,
        dev->battery.soc, 
        dev->battery.istat.mean,
        dev->battery.istat.stdev,
        dev->battery.istat.max,
        dev->battery.istat.min,
        dev->battery.Vt,
        dev->battery.Voc,
        dev->battery.Q,
        dev->battery.tstat.mean);
    fclose(fd);
    return 0;
}

int statwrite(acdev_t *dev){
    time_t now;
    FILE *fd;
    char stemp[128];
    static char statfile_f = 1;
    now = time(NULL);
    
    if(!(fd = fopen(dev->statfile,"w"))){
        if(statfile_f){
            acmessage_send(dev, "Failed to write to the stat file:");
            acmessage_send(dev, dev->statfile);
            acmessage_send(dev, "This message will not be repeated.");
            statfile_f = 0;
        }
        return -1;
    }
    strftime(stemp, 128, "%Y-%m-%d %H:%M:%S UTC%z", localtime(&now));
    fprintf(fd, "Updated: %s\n", stemp);
    fprintf(fd, "Terminal Voltage (V)\n  last:%.4f\n  mean:%.4f\n  stdev:%.4f\n",
        dev->battery.Vt,
        dev->battery.vstat.mean,
        dev->battery.vstat.stdev);
    fprintf(fd, "Current (A)\n  last:%.2f\n  mean:%.2f\n  stdev:%.2f\n  max:%.2f\n  min:%.2f\n",
        dev->battery.I,
        dev->battery.istat.mean,
        dev->battery.istat.stdev,
        dev->battery.istat.max,
        dev->battery.istat.min);
    fprintf(fd, "Temperature (K)\n  mean:%.1f\n",
        dev->battery.tstat.mean);
    fprintf(fd, "Open Circuit Voltage (V)\n  last:%.4f\n",
        dev->battery.Voc);
    fprintf(fd, "State of charge\n  last:%.4f\n",
        dev->battery.soc);
    fprintf(fd, "Charge since last state change:\n  Coulombs: %f\n  Amp-hr: %f\n",
        dev->battery.Q,
        dev->battery.Q / 3600);
    fprintf(fd, "Charge state: ");
    switch(dev->battery.chargestate){
    case CM_CHARGE_EMPTY:
        fprintf(fd, "EMPTY\n");
    break;
    case CM_CHARGE_FULL:
        fprintf(fd, "FULL\n");
    break;
    case CM_CHARGE_NONFULL:
        fprintf(fd, "NONFULL\n");
    break;
    case CM_CHARGE_UNKNOWN:
    default:
        fprintf(fd, "UNKNOWN\n");
    break;
    }
    fclose(fd);
    return 0;
}

int main(int argc, char *argv[]){
    char stemp[AC_STRLEN];
    unsigned int packets_per_datum,     // Number of stream packets between data entries
                packet_count;           // Keep track of the number of packets so far
    int ii;
    double buffer[AC_SAMPLES_PER_PACKET];
    double I, Vt, T;
    
    // Parse the configuration file
    if(acconfig(&dev, CONFIG)){
        fprintf(stderr, "FATAL: Failed to parse configuration file.\n");
        return -1;
    }
    
    // Check the sample intervals for sanity
    if(dev.battery.ts < TS_MIN || dev.battery.ts > TS_MAX){
        acmessage_set(stderr);
        sprintf(stemp, "FATAL: Sample interval (ts) was out of bounds (%f, %f): %lf", TS_MIN, TS_MAX, dev.battery.ts);
        acmessage_send(&dev, stemp);
        return -1;
    }
    // Calculate the packets per datum
    packets_per_datum = (unsigned int) (dev.tdata / (dev.battery.ts * AC_SAMPLES_PER_PACKET));
    // Update tdata to reflect the rounding
    dev.tdata = (double) (packets_per_datum * dev.battery.ts * AC_SAMPLES_PER_PACKET);
    // Log the startup time
    acmessage_send(&dev, "BATMON started.");
    
    // Open the connection
    if(acopen(&dev)){
        acmessage_send(&dev, "FATAL: Failed to connect to device.");
        return -1;
    }
    
    // Start the data stream
    if(acstream_start(&dev)){
        acmessage_send(&dev, "FATAL: Failed to start data stream.");
        acclose(&dev);
        return -1;
    }
    
    // Set up the handler for the TERM signal
    signal(SIGTERM, term_handler);
    
    // Initialize the relevant counter(s)
    packet_count = 0;
    
    // Start the service loop
    // It will exit with a TERM signal
    go_f = 1;
    while(go_f){
        // Read in a packet (blocking)
        acstream_read(&dev, buffer);
        packet_count++;
        // process the new data
        for(ii = 0; ii<AC_SAMPLES_PER_PACKET; ii++){
            I = buffer[3*ii];
            Vt = buffer[3*ii + 1];
            T = buffer[3*ii + 2];
            
            // If the charge state has changed, record the event
            if(cmstep(&dev.battery, I, Vt, T)){
                datawrite(&dev, 'E');
                statwrite(&dev);
                cmreset(&dev.battery);
            }
        }
        // If this is a datum that should be recorded
        if(packet_count >= packets_per_datum){
            // Restart the count
            packet_count = 0;
            datawrite(&dev, ' ');
            statwrite(&dev);
            cmreset(&dev.battery);
        }
    }
    acmessage_send(&dev,"BATMON stopping.");
    // Stop the stream and close
    acstream_stop(&dev);
    acclose(&dev);
    
    return 0;
}

