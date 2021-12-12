/* CHARGEMODEL
 *
 *  Implements a battery model to back-calculate open circuit voltage,
 * state of charge, and battery health.
 * 
 * Uses a dynamic terminal resistance model
 * 
 *          
 *   o------|  R1  |-------+--------|  R2  |-------+-------o
 *                         |                       |
 *                         |                       |
 *                         +--------|  C   |-------+
 * 
 * Parameters, R1, R2, and the time constant, R2 * C are experimentally
 * determined at some temperature and state-of-charge.  Many models 
 * allow these parameters to change with temperature and state of 
 * charge, so the CMBAT_T struct allows for temperature and soc 
 * sensitivities.
 */
 
#ifndef __chargemodel_h__
#define __chargemodel_h__

#define CMHIST 2

// The CMBAT_T struct tracks the battery properties
// Parameters that are calculated from raw configuration parameters
// are preceeded with an underscore.
typedef struct _cmbat_t {
    // Resistance model
    double R1_ohm;      // Initial terminal resistance at reference temperature
    double R1_temp;     // Initial terminal resistance sensitivity to temperature
    double R2_ohm;      // Rise in terminal resistance at reference temperature
    double R2_temp;     // Resistance rise sensitivity to temperature
    // Time parameters
    double tc_sec;      // Time constant for resistance relaxation
    double ts_sec;      // Sample interval
    // Temperature parameters
    double T_ref_K;     // reference temperature
    // Voltage parameters
    double V_full;      // OCV at full charge
    double V_full_temp; // OCV full charge temperature coefficient
    double V_disch;     // OCV at fully discharged
    double V_disch_temp;// OCV discharged temperature coefficient
    
    // Dynamic model histories
    double _I_hist[CMHIST];  // Current history array
    double _V_loss_hist[CMHIST]; // Voltage loss history array
    
    
    unsigned int uptime;
    
} cmbat_t;



int cmconfig(cmbat_t *bat, char *filename);

int cmstep(cmbat_t *bat, double I, double V, double T);

#endif
