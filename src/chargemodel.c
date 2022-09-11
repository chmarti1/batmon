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
    char param[AC_STRLEN];
    double value, vmax, vmin;
    double *ftarget;
    FILE *fd = NULL;
    acerror_t done = ACERR_NONE;

    // First, initialize the bat struct
        // Resistance model
    bat->R1 = 0;        // Initial terminal resistance at Tref
    bat->R2 = 0;        // Rise in terminal resistance at reference temperature
    // Time parameters
    bat->tc = 1.;       // Time constant for resistance relaxation
    bat->ts = ts;       // Sample rate
    // Temperature parameters
    bat->Tref = 298.15;      // reference temperature in (Kelvin)
    // Voltage parameters
    bat->Vfull = 12.9;  // OCV at full charge at Tref
    bat->Vfull_c1 = 0.; //   temperature coefficient 1 (Kelvin)
    bat->Vfull_c2 = 0.; //   temperature coefficient 2 (Kelvin)
    bat->Vdisch = 11.55;// OCV at fully discharged at Tref
    bat->Vdisch_c1 = 0.;//   temperature coefficient 1 (Kelvin)
    bat->Vdisch_c2 = 0.;//   temperature coefficient 2 (Kelvin)
    // Measurements/states
    bat->Vt = bat->Vfull;// Last terminal voltage measured
    bat->I = 0.;        // Last measured current
    bat->T = bat->Tref; // Last measured temperature
    bat->Voc = bat->Vfull;// last open-circuit voltage calculation
    bat->Q = 0.;        // Coloumb count
    bat->soc = 1.;         // Depth of charge estimate
    
    bat->chargestate = CM_CHARGE_UNKNOWN;
    
    // Dynamic models
    tf_init(&bat->_vttf);
    tf_init(&bat->_qtf);
    // Signal statistics
    stat_reset(&bat->istat);
    stat_reset(&bat->vstat);
    stat_reset(&bat->tstat);

    bat->uptime = 0;

    // Open the configuration file
    fd = fopen(filename, "r");
    if(!fd){
        fprintf(stderr, "CMCONFIG: Failed to open file: %s", filename);
        return -1;
    }
    
    for(count=1; !feof(fd); count++){
        result = fscanf(fd, "%256s %f[^\n]", param, &value);
        
        if(param[0] != '#' && result != 2 && !feof(fd)){
            fprintf(stderr, "CMCONFIG: Parameter-value pair number %d was illegal. (%d)", count, result);
            
            fprintf(stderr, "          param: %70s", param);
            fprintf(stderr, "          value: %70s", value);
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
            ftarget = &bat->R1;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "r1_temp")){
            ftarget = &bat->R1_temp;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "r2")){
            ftarget = &bat->R2;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "r2_temp")){
            ftarget = &bat->R2_temp;
            vmax = 1000.;
            vmin = -1000.;
        }else if(streq(param, "tc")){
            ftarget = &bat->tc;
            vmax = 1000.;
            vmin = 0.;
        }else if(streq(param, "vfull")){
            ftarget = &bat->Vfull;
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
            ftarget = &bat->Vdisch;
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
    }
    return 0;
}


/* CMVFULL, CMVDISCH - calculate fully (dis)charged OCV in volts.
 *  Returns the fully (dis)charged open-circuit voltage in volts.
 *  BAT - the battery model struct
 * 
 * Uses the last measured temperature registered with CMSTEP() to 
 * compensate for temperature.
 */
double cmvfull(cmbat_t *bat){
    double value;
    value = bat->Vfull;
    if(bat->Vfull_c1){
        value += bat->Vfull_c1 * (1./bat->T - 1./bat->Tref);
    }
    if(bat->Vfull_c2){
        value += bat->Vfull_c2 * bat->Vfull_c2 * (1./bat->T/bat->T - 1./bat->Tref/bat->Tref);
    }
    return value;
}

double cmvdisch(cmbat_t *bat){
    double value;
    value = bat->Vdisch;
    if(bat->Vdisch_c1){
        value += bat->Vdisch_c1 * (1./bat->T - 1./bat->Tref);
    }
    if(bat->Vdisch_c2){
        value += bat->Vdisch_c2 * bat->Vdisch_c2 * (1./bat->T/bat->T - 1./bat->Tref/bat->Tref);
    }
    return value;
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
    value = bat->R1;
    if(bat->R1_temp){
        value += bat->R1_temp * (bat->T - bat->Tref);
    }
    return value;
}

double cmr2(cmbat_t *bat){
    double value;
    value = bat->R2;
    if(bat->R2_temp){
        value += bat->R2_temp * (bat->T - bat->Tref);
    }
    return value;
}


int cmstep(cmbat_t *bat, double I, double V, double T){
    double Vfull, Vdisch;
    cmcharge_t last;
    
    bat->uptime ++;

    // Update the signal statistics
    stat_datum(&bat->istat, I);
    stat_datum(&bat->vstat, V);
    stat_datum(&bat->tstat, T);

    // Update the open circuit voltage calculation
    bat->Voc = V + tf_eval(&bat->_vttf, I);
    bat->Vt = V;
    bat->I = I;
    
    // Calculate voltage limits at the current temperature
    Vfull = 1 + bat->Vfull_c1*(1/T - 1/bat->Tref);
    Vfull += bat->Vfull_c2*bat->Vfull_c2*(1/(T*T) - 1/(bat->Tref*bat->Tref));
    Vfull *= bat->Vfull;
    
    Vdisch = 1 + bat->Vdisch_c1*(1/T - 1/bat->Tref);
    Vdisch += bat->Vdisch_c2*bat->Vdisch_c2*(1/(T*T) - 1/(bat->Tref*bat->Tref));
    Vdisch *= bat->Vdisch;
    
    // If the charge state has changed, then null out the coloumb counter
    last = bat->chargestate;
    // If Vfull <= Vdischarge, then there is a serious problem!
    if(Vfull <= Vdisch){
        bat->chargestate = CM_CHARGE_UNKNOWN;
        bat->soc = 0.;
    }else if(bat->Voc >= Vfull){
        bat->chargestate = CM_CHARGE_FULL;
        bat->soc = 1.;
    }else if(bat->Voc > Vdisch){
        bat->chargestate = CM_CHARGE_NONFULL;
        bat->soc = (bat->Voc - Vdisch)/(Vfull - Vdisch);
    }else{
        bat->chargestate = CM_CHARGE_EMPTY;
        bat->soc = 0.;
    }
    
    // If the charge state has changed, null out the integrator
    // Do not reset Q yet.  Let the application read it first.
    if(bat->chargestate != last){
        bat->_qtf.u[0] = 0.;
        bat->_qtf.u[1] = 0.;
        bat->_qtf.x[0] = 0.;
        bat->_qtf.x[1] = 0.;
        
        return 1;
    }else{
        bat->Q = tf_eval(&bat->_qtf, I);
    }

    return 0;
}


void cmreset(cmbat_t *bat){
    stat_reset(&bat->istat);
    stat_reset(&bat->vstat);
    stat_reset(&bat->tstat);
}
