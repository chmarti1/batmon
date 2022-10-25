/* CHARGEMODEL
 * 
 */

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "chargemodel.h"


#define streq(a,b) (strcmp(a,b) == 0)

// Helper functions


/* TF_INIT - initializes a transfer function struct with sane values
 * 
 * Zeros the history arrays and initializes the TF to be 1.
 */
void tf_init(cmtf_t *tf){
    int ii;
    for(ii=0; ii<CMHIST; ii++){
        tf->a[ii] = 0.;
        tf->b[ii] = 0.;
        tf->u[ii] = 0.;
        tf->x[ii] = 0.;
    }
    tf->a[CMHIST-1] = 1.;
    tf->b[CMHIST-1] = 1.;
}

/* TF_EVAL - evaluate a discrete-time transfer function
 * 
 * u    The next input sample
 * returns the corresponding output sample
 * 
 * Treats the x, and u indices x[k] like X z^k so that the last sample 
 * is the most recent.  The coefficient indices correspond.
 */
double tf_eval(cmtf_t *tf, double u){
    unsigned int ii;
    // Shift in the new sample
    for(ii = 0; ii<CMHIST-1; ii++){
        tf->x[ii] = tf->x[ii+1];
        tf->u[ii] = tf->u[ii+1];
    }
    tf->u[CMHIST-1] = u;
    
    // Initialize the result
    tf->x[CMHIST-1] = tf->b[CMHIST-1] * tf->u[CMHIST-1];
    for(ii = 0; ii<CMHIST-1; ii ++)
        tf->x[CMHIST-1] += tf->b[ii]*tf->u[ii] - tf->a[ii]*tf->x[ii];
    tf->x[CMHIST-1] /= tf->a[CMHIST-1];
    // Return the new result
    return tf->x[CMHIST-1];
}


/* STAT_RESET - Starts a new statistics collection interval
 * 
 * Resets the ineternal sample counter and the intermediate sums without
 * affecting the "public" members.
 */
void stat_reset(cmstat_t *stat){
    stat->_N = 0;
    stat->_sumx = 0.0;
    stat->_sumx2 = 0.0;
    stat->_max = DBL_MIN;
    stat->_min = DBL_MAX;
}

/* STAT_DATUM - read a new datum into the statistics collection
 * 
 * First updates _N, _sumx, _sumx2, _max, and _min using the new value
 * Then, pushes updated values for mean, rms, stdev, max, min to the 
 * "public" members.  These values are not initialized until the first 
 * call to STAT_DATUM.
 */
void stat_datum(cmstat_t *stat, double x){
    stat->_N++;
    stat->_sumx += x;
    stat->_sumx2 += x*x;
    if(x > stat->_max)
        stat->_max = x;
    if(x < stat->_min)
        stat->_min = x;
        
    // Update the public params
    stat->max = stat->_max;
    stat->min = stat->_min;
    stat->mean = stat->_sumx / stat->_N;
    if(stat->_N > 0){
        stat->rms = sqrt(stat->_sumx2 / stat->_N);
        stat->stdev = sqrt((stat->_sumx2 - stat->_sumx*stat->_sumx/stat->_N)/(stat->_N-1));
    }else{
        stat->rms = stat->mean;
        stat->stdev = 0.;
    }
}


/* STRLOWER - convert to lowercase
 * 
 */
void strlower(char *target){
    unsigned int ii;
    for(ii=0; target[ii]!= '\0'; ii++){
        if(target[ii]>= 'A' && target[ii]<='Z')
            target[ii] += 'a' - 'A';
    }
}


int cmconfig(cmbat_t *bat, char *filename, double ts){
    int result, count, err;
    char param[CM_STRLEN];
    double value, vmax, vmin;
    double *ftarget;
    FILE *fd = NULL;

    // First, initialize the bat struct
        // Resistance model
    bat->R1_ref = 0;        // Initial terminal resistance at Tref
    bat->R1_T = 0;
    bat->R2_ref = 0;        // Rise in terminal resistance at reference temperature
    bat->R2_T = 0;
    // Time parameters
    bat->tc = 1.;       // Time constant for resistance relaxation
    bat->ts = ts;       // Sample rate
    // Temperature parameters
    bat->Tref = 298.15;      // reference temperature in (Kelvin)
    // Voltage parameters
    bat->Vfull_ref = 12.9;  // OCV at full charge at Tref
    bat->Vfull_c1 = 0.; //   temperature coefficient 1 (Kelvin)
    bat->Vfull_c2 = 0.; //   temperature coefficient 2 (Kelvin)
    bat->Vdisch_ref = 11.55;// OCV at fully discharged at Tref
    bat->Vdisch_c1 = 0.;//   temperature coefficient 1 (Kelvin)
    bat->Vdisch_c2 = 0.;//   temperature coefficient 2 (Kelvin)
    // Measurements/states
    bat->Vt = bat->Vfull;// Last terminal voltage measured
    bat->I = 0.;        // Last measured current
    bat->T = bat->Tref; // Last measured temperature
    bat->Voc = bat->Vfull;// last open-circuit voltage calculation
    bat->Q = 0.;        // Coloumb count
    bat->soc = 1.;         // State of charge estimate
    
    bat->uptime = 0;

    // Open the configuration file
    fd = fopen(filename, "r");
    if(!fd){
        fprintf(stderr, "CMCONFIG: Failed to open file: %s", filename);
        return -1;
    }
    
    for(count=1; !feof(fd); count++){
        result = fscanf(fd, "%256s %lf[^\n]", param, &value);
        
        if(param[0] != '#' && result != 2 && !feof(fd)){
            fprintf(stderr, "CMCONFIG: Parameter-value pair number %d was illegal. (%d)", count, result);
            
            fprintf(stderr, "          param: %70s", param);
            fclose(fd);
            return -1;
        }else if(result == 0){
            break;
        }

        // Convert to a lower-case string
        strlower(param);

        if(param[0] == '#'){
            // This is a comment.  Do nothing.
        }else if(streq(param, "tref")){
            ftarget = &bat->Tref;
            vmax = 350.;
            vmin = 250.;
        }else if(streq(param, "r1")){
            ftarget = &bat->R1_ref;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "r1_t")){
            ftarget = &bat->R1_T;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "r2")){
            ftarget = &bat->R2_ref;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "r2_t")){
            ftarget = &bat->R2_T;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "tc")){
            ftarget = &bat->tc;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "vfull")){
            ftarget = &bat->Vfull_ref;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "vfull_c1")){
            ftarget = &bat->Vfull_c1;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "vfull_c2")){
            ftarget = &bat->Vfull_c2;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "vdisch")){
            ftarget = &bat->Vdisch_ref;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "vdisch_c1")){
            ftarget = &bat->Vdisch_c1;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "vdisch_c2")){
            ftarget = &bat->Vdisch_c2;
            vmax = 1000.;
            vmin = -1000.;
        }else{
            fprintf(stderr, "CMCONFIG: Unrecognized parameter: %60s", param);
            fclose(fd);
            return -1;
        }
        
        // Test for in-bounds
        if(value > vmax || value < vmin){
            fprintf(stderr, "CMCONFIG: Value for \"%s\" was out of bounds [%f, %f]: %f\n", param, vmin, vmax, value);
            fclose(fd);
            return -1;
        }
        // Everything is fine - assign the value
        *ftarget = value;
    }
    // We're done loading the file
    fclose(fd);
    
    // Enforce a few rules:
    if(bat->Vfull < bat->Vdisch){
        fprintf(stderr, "CMCONFIG: The full voltage is greater than the discharge voltage!\n");
        return -1;
    }else if(bat->tc < bat->ts){
        fprintf(stderr, "CMCONFIG: WARNING * The sample interval is longer than the battery time constant.\n");
        return -1;
    }else if(bat->ts <= 0.){
        fprintf(stderr, "CMCONFIG: The application is using a non-positive sample interval!?\n");
        return -1;
    }
    
    // Initialize the histories
    tf_init(&bat->_vttf);
    tf_init(&bat->_qtf);
    // The terminal voltage TF will be set when cmstep() is called.
    // Go ahead and set the charge integrator - use Tustin/trapezoidal
    bat->_qtf.b[1] = 0.5 * bat->ts;
    bat->_qtf.b[0] = 0.5 * bat->ts;
    bat->_qtf.a[1] = 1;
    bat->_qtf.a[0] = -1;
    
    // Initialize the signal statistics
    stat_reset(&bat->Istat);
    stat_reset(&bat->Vstat);
    stat_reset(&bat->Tstat);
    return 0;
}


/* CMR1, CMR2 - calculate the terminal resistances in ohms
 *  Returns R1 and R2 in ohms.
 *  BAT - the battery model struct
 * 
 * Uses the last measured temperature registered with CMSTEP() to 
 * compensate for temperature.
 */
double cmr1(cmbat_t *bat){
    double value;
    value = bat->R1_ref;
    if(bat->R1_T){
        value += bat->R1_T * (bat->T - bat->Tref);
    }
    return value;
}

double cmr2(cmbat_t *bat){
    double value;
    value = bat->R2_ref;
    if(bat->R2_T){
        value += bat->R2_T * (bat->T - bat->Tref);
    }
    return value;
}

int cmupdate(cmbat_t *bat){
    double T;
    // Use the mean temperature
    T = bat->Tstat.mean;
    // Update the Vfull values
    bat->Vfull = bat->Vfull_ref;
    if(bat->Vfull_c1){
        bat->Vfull += bat->Vfull_c1 * (1./T - 1./bat->Tref);
    }
    if(bat->Vfull_c2){
        bat->Vfull += bat->Vfull_c2 * bat->Vfull_c2 * (1./T/T - 1./bat->Tref/bat->Tref);
    }
    // Update the Vdisch values
    bat->Vdisch = bat->Vdisch_ref;
    if(bat->Vdisch_c1){
        bat->Vdisch += bat->Vdisch_c1 * (1./T - 1./bat->Tref);
    }
    if(bat->Vdisch_c2){
        bat->Vdisch += bat->Vdisch_c2 * bat->Vdisch_c2 * (1./T/T - 1./bat->Tref/bat->Tref);
    }
    // Update the soc
    bat->soc = (bat->Voc - bat->Vdisch)/(bat->Vfull - bat->Vdisch);
    // Clamp the soc and case out the charge state
    if(bat->soc >= 1.){
        bat->chargestate = CM_CHARGE_FULL;
        bat->soc = 1;
    }else if(bat->soc<= 0.){
        bat->chargestate = CM_CHARGE_EMPTY;
        bat->soc = 0;
    }else if(bat->I > 0.){
        bat->chargestate = CM_CHARGE_DISCHARGING;
    }else if(bat->I < 0.){
        bat->chargestate = CM_CHARGE_CHARGING;
    }
    
    return 0;
}


int cmstep(cmbat_t *bat, double I, double V, double T){
    double Vfull, Vdisch, Rinf, R0, tratio;
    cmcharge_t last;
    
    bat->uptime ++;

    // Update the direct measurements
    bat->Vt = V;
    bat->I = I;
    bat->T = T;

    // Update the signal statistics
    stat_datum(&bat->Istat, I);
    stat_datum(&bat->Vstat, V);
    stat_datum(&bat->Tstat, T);

    // Update the charge integral
    bat->Q = tf_eval(&bat->_qtf, I);

    // Calculate current terminal resistance values (depends on T)
    R0 = cmr1(bat);
    Rinf = R0 + cmr2(bat);
    // Ratio of the time constant to the sample interval
    tratio = 2*bat->tc/bat->ts;
    // Update the TF coefficients
    bat->_vttf.b[1] = Rinf + R0*tratio;
    bat->_vttf.b[0] = Rinf - R0*tratio;
    bat->_vttf.a[1] = 1+tratio;
    bat->_vttf.a[0] = 1-tratio;

    // Update the open circuit voltage calculation
    bat->Voc = V + tf_eval(&bat->_vttf, I);

    // Check for a state change that might prompt the application to
    // call for an update.
    
    return 0;
}


void cmreset(cmbat_t *bat){
    stat_reset(&bat->Istat);
    stat_reset(&bat->Vstat);
    stat_reset(&bat->Tstat);
}
