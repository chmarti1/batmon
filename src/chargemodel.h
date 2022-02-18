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
    double R2;          // Rise in terminal resistance at reference temperature
    // Time parameters
    double tc;      // Time constant for resistance relaxation
    double ts;      // Sample interval
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

    unsigned long int uptime;
    
} cmbat_t;

typedef struct _cmstat_t {
    double _sumx2;      // sum of the square of measurements
    double _sumx;       // sum of measurements
    double _max;        // The maximum value encountered so far
    double _min;        // The minimum value encountered so far
    unsigned int _N;    // The number of measurements read in
    // Reported values
    double max;
    double min;
    double mean;
    double rms;
    double stdev;
} cmstat_t;

// Assign safe initial conditions to the battery parameters
int cminit(cmbat_t *bat);

/* CMWRITE
 * Provides a parametric interface for reading in parameter-value pairs
 * from a configuration file.  Return values are: 
 *  0   on success
 *  1   the parameter was not recognized
 *  2   there was a problem parsing the value
 */
int cmwrite(cmbat_t *bat, char *param, char *value);

double cmstep(cmbat_t *bat, double I, double V);

/* CMSTAT_RESET
 * The CMSTAT_T struct holds statistics on a recurring measurement 
 * between calls to the CMSTAT_RESET function.  A "reset" clears members
 * that accumulate sums for the mean and standard deviation calculations
 * as well as members for tracking maximum and minimum values.
 * 
 * A reset does NOT affect the "reported" values in members:
 *  max, min, mean, rms, and stdev
 * 
 * Calling CMSTAT_RESET on a stat effectively concludes the accumulation
 * of data and starts a new interval.  See CMSTAT_DATUM.
 */
void cmstat_reset(cmstat_t *stat);

/* CMSTAT_DATUM
 * Enter a new datum into the statistics accumulation struct. There are
 * "private" members that track measurement statistics,
 *      _sumx, _sumx2, _max, _min, _N
 * which should never be accessed directly.
 * 
 * Each call to CMSTAT_DATUM updates the "public" members:
 *      max, min, mean, rms, stdev
 * 
 * These values will be uninitialized even after a call to CMSTAT_RESET,
 * but they will hold valid values after the first call to CMSTAT_DATUM.
 * After a second call to CMSTAT_RESET, they will retain their values 
 * until the next call to CMSTAT_DATUM.
 */
void cmstat_datum(cmstat_t *stat, double x);

#endif
