/* CHARGEMODEL
 * 
 */

#include "chargemodel.h"

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


int cmconfig(cmbat_t *bat, char *filename){
    return 0;
}

int cmstep(cmbat_t *bat, double I, double V, double T){
    double Rinit, Rinf, t;
    double a[CMHIST], b[CMHIST];  // Model coefficients
    bat->uptime ++;
    
    hist_shift(bat->_I_hist, I);
    hist_shift(bat->_V_hist, V);
    hist_shift(bat->_V_loss_hist, 0.);
    
    // Update the resistance values
    Rinit = bat->R1_ohm + bat->R1_temp*(T - bat->T_ref_K);
    Rinf = Rinit + bat->R2_ohm + bat->R2_temp*(T - bat->T_ref_K);
    // Time ratio
    t = bat->tc_sec / bat->ts_sec;
    // Calculate the new model coefficients
    a[0] = 1 - 2*t;
    a[1] = 1 + 2*t;
    b[0] = Rinf - 2*Rinit*t;
    b[1] = Rinf + 2*Rinit*t;

    // Update the loss model
    bat->_V_loss_hist[0] = a1*bat->_V_loss_hist[1] + b0*bat->_I_hist[0] + b1*bat->_I_hist[1];
    
    
    return 0;
}
