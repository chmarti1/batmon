/* CHARGEMODEL
 *
 *  Implements a battery model to back-calculate open circuit voltage,
 * state of charge, and battery health.
 * 
 * Uses a dynamic terminal resistance model
 * 
 *          
 *   o------|  R1  |-------+--------|  R2  |-------+-------o 
 *   +                     |                       |       +
 * Voc                     +--------|  C   |-------+        Vt
 *   -                                                     -
 *   o-----------------------------------------------------o 
 * 
 * Parameters, R1, R2, and the time constant, R2 * C are experimentally
 * determined at some temperature and state-of-charge.  The CMBAT model
 * includes a linear temperature coefficient for the terminal 
 * resistances, so 
 * 
 *      R1(T) = R1(Tref) + R1_temp * (T - Tref)
 *      R2(T) = R2(Tref) + R2_temp * (T - Tref)
 * 
 * The time constant is presumed not to change with temperature.
 * 
 * The open-circuit voltage, Voc, is an excellent indicator for the 
 * state of charge of a lead acid battery, but unfortunately, it can 
 * only be measured while the battery is at rest.  However, the terminal
 * voltage, Vt, and the current, I, can be measured at any time during
 * use.  Then, using the terminal reistance model, the open circuit 
 * voltage can be estimated.
 * 
 * State of charge is estimated by a linear interpolation between the 
 * full charge voltage and the discharged voltage, Vfull and Vdisch.  
 * 
 *      Voc = Vfull * SOC  + Vdisch * (1-SOC)
 * 
 * Both the full and discharged open circuit voltages are modeled using
 * a quadratic on 1/T.
 * 
 *                       /      c1     c2 * c2  \
 *      V(T) = Vinf *    | 1 + ---- + --------- |
 *                       \       T      T * T   /
 * 
 * The coefficient, Vinf, is the open-circuit charge or discharge 
 * voltage extrapolated to infinite temperature
 * 
 */
 
#ifndef __chargemodel_h__
#define __chargemodel_h__

#define CMHIST 2

// The CMBAT_T struct tracks the battery properties
// Parameters that are calculated from raw configuration parameters
// are preceeded with an underscore.
typedef struct _cmbat_t {
    // Resistance model
    double R1;          // Initial terminal resistance at reference temperature
    double R1_temp;     // Initial terminal resistance sensitivity to temperature
    double R2;          // Rise in terminal resistance at reference temperature
    double R2_temp;     // Resistance rise sensitivity to temperature
    // Time parameters
    double tc_sec;      // Time constant for resistance relaxation
    double ts_sec;      // Sample interval
    // Temperature parameters
    double Tref_K;      // reference temperature
    // Voltage parameters
    //  V = v0 * (1 + c1/T + (c2/T)*(c2/T))
    double Vfull;       // OCV at full charge at Tref
    double Vfull_c1;    //   temperature coefficient 1 (Kelvin)
    double Vfull_c2;    //   temperature coefficient 2 (Kelvin)
    double Vdisch;      // OCV at fully discharged at Tref
    double Vdisch_c1;   //   temperature coefficient 1 (Kelvin)
    double Vdisch_c2;   //   temperature coefficient 2 (Kelvin)
    
    // Dynamic model histories
    double _I[CMHIST];  // Current history array
    double _VL[CMHIST]; // Voltage loss history array
    double _VT[CMHIST]; // Terminal voltage history
    
    
    
    unsigned int uptime;
    
} cmbat_t;



int cmconfig(cmbat_t *bat, char *filename);

int cmstep(cmbat_t *bat, double I, double V, double T);

#endif
