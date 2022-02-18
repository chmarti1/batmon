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

/* HIST_SHIFT - shift state history vector
 *  x must be an array with at least CMHIST elements
 *  xnew is the new value to be shifted into the array
 * The x array values are ordered so x[0] is the oldest and x[CMHIST-1]
 * is the newest.  HIST_SHIFT discards x[0] and shifts all values 
 * backwards, so x[CMHIST-1] is available to write xnew.
 */
void hist_shift(double *x, double xnew){
    unsigned int ii;
    for(ii = 0; ii<CMHIST-1; ii++)
        x[ii] = x[ii+1];
    x[CMHIST-1] = xnew;
}

/* TF_EVAL - evaluate a discrete-time transfer function
 *  a output coefficients
 *  b input coefficients
 *  x output history vector
 *  u input history vector
 * 
 * The history vectors must have already been shifted using the 
 * HIST_SHIFT() function.  This will leave the newest value already
 * loaded into u and the newest value of x will be overwritten.
 * 
 */
void tf_eval(double *a, double *b, double *x, double *u){
    unsigned int ii;
    // Initialize the result
    x[CMHIST-1] = b[CMHIST-1] * u[CMHIST-1];
    for(ii = 0; ii<CMHIST-1; ii ++)
        x[CMHIST-1] += b[ii]*u[ii] - a[ii]*x[ii];
    x[CMHIST-1] /= a[CMHIST-1];
}

int cminit(cmbat_t *bat){
    int ii;
    
    bat->R1 = 0.;
    bat->R2 = 0.;
    
    bat->tc = 1;
    bat->ts = 1;
    
    bat->Tref_K = 298.15;
    
    bat->Vfull = 13.8;
    bat->Vfull_c1 = 0.;
    bat->Vfull_c2 = 0.;
    
    bat->Vdisch = 12.0;
    bat->Vdisch_c1 = 0.;
    bat->Vdisch_c2 = 0.;
    
    for(ii=0; ii<CMHIST; ii++){
        bat->_I[ii] = 0.;
        bat->_VL[ii] = 13.8;
        bat->_VT[ii] = 13.8;
    }
    
    return 0;
}

int cmwrite(cmbat_t *bat, char *param, char *value){
    double *fvalue = NULL;
    if(streq(param, "Tref_K")){
        fvalue = &bat->Tref_K;
    }else if(streq(param, "Vfull")){
        fvalue = &bat->Vfull;
    }else if(streq(param, "Vfc1")){
        fvalue = &bat->Vfull_c1;
    }else if(streq(param, "Vfc2")){
        fvalue = &bat->Vfull_c2;
    }else if(streq(param, "Vdisch")){
        fvalue = &bat->Vdisch;
    }else if(streq(param, "Vdc1")){
        fvalue = &bat->Vdisch_c1;
    }else if(streq(param, "Vdc2")){
        fvalue = &bat->Vdisch_c2;
    }else if(streq(param, "R1")){
        fvalue = &bat->R1;
    }else if(streq(param, "R2")){
        fvalue = &bat->R2;
    }else if(streq(param, "ts")){
        fvalue = &bat->ts;
    }else if(streq(param, "tc")){
        fvalue = &bat->tc;
    }else{
        return 1;
    }
    
    if(!fvalue){
        // This should never happen
        fprintf(stderr, "CMWRITE: UNEXPECTED EXCEPTION\n");
        return 1;
    }
    
    if(1 != sscanf(value, "%lf", fvalue)){
        return 2;
    }
    return 0;
}

double cmstep(cmbat_t *bat, double I, double V){
    double Rinit, Rinf, t;
    double a[CMHIST], b[CMHIST];  // Model coefficients
    bat->uptime ++;
    
    hist_shift(bat->_I, I);
    hist_shift(bat->_VT, V);
    hist_shift(bat->_VL, 0.);
    
    // Update the resistance values
    Rinit = bat->R1;
    Rinf = Rinit + bat->R2;
    // Time ratio
    t = bat->tc / bat->ts;
    // Calculate the new model coefficients
    a[0] = 1 - 2*t;
    a[1] = 1 + 2*t;
    b[0] = Rinf - 2*Rinit*t;
    b[1] = Rinf + 2*Rinit*t;

    tf_eval(a,b,bat->_VL, bat->_I);
    
    return bat->_VL[CMHIST-1];
}


void cmstat_reset(cmstat_t *stat){
    stat->_N = 0;
    stat->_sumx = 0.0;
    stat->_sumx2 = 0.0;
    stat->_max = DBL_MIN;
    stat->_min = DBL_MAX;
}

void cmstat_datum(cmstat_t *stat, double x){
    stat->_N++;
    stat->_sumx += x;
    stat->_sumx += x*x;
    if(x > stat->_max)
        stat->_max = x;
    if(x < stat->_min)
        stat->_min = x;
        
    // Update the public params
    stat->max = stat->_max;
    stat->min = stat->_min;
    stat->mean = stat->_sumx / stat->_N;
    if(stat->_N > 0){
        stat->rms = stat->_sumx2 / (stat->_N-1);
        stat->stdev = sqrt((stat->_sumx2 - stat->_sumx*stat->_sumx)/(stat->_N-1));
    }else{
        stat->rms = stat->mean;
        stat->stdev = 0.;
    }
}
