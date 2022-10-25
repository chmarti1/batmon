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
 *      R1(T) = R1(Tref) + R1_T * (T - Tref)
 *      R2(T) = R2(Tref) + R2_T * (T - Tref)
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
 * voltage extrapolated to infinite temperature.  It is calculated from
 * VFULL and VDISCH, which are the full-charge voltages at Tref.
 * 
 *** USE ***
 * (1) First, initialize the battery model using the CMCONFIG() 
 * function.  This will read in a configuration file containing the 
 * various coefficient values described above.  It has a mandatory 
 * argument, ts, which determines the sample interval in seconds.  This
 * is the interval at which the battery terminal voltage, current, and
 * temperature will be measured and fed to the model.
 * 
 * (2) The application starts a data acquisition process.  The process
 * should be timed to produce measurements at intervals consistent with 
 * the sample period configured above.  This should be hardware timed 
 * and NOT software timed (i.e. do not use usleep()).
 * 
 * (3) At regular intervals ts seconds apart, CMSTEP() is called to feed
 * new terminal voltage, current, and temperature measurements to the
 * battery model.  
 * 
 * (4) At less frequent intervals, 
 */
 
#ifndef __chargemodel_h__
#define __chargemodel_h__

#define CMHIST 2
#define CM_STRLEN 128

// The CMTF_T struct models a dynamic model.  This is used for coulomb 
// counter and the dynamic terminal model.  It is a discrete-time 
// transfer function model.
typedef struct _cmtf_t {
    double a[CMHIST];       // Model output coefficients
    double b[CMHIST];       // Model input coefficients
    double x[CMHIST];       // Model output history
    double u[CMHIST];       // Model input history
} cmtf_t;


typedef enum _cmcharge_t {
    CM_CHARGE_UNKNOWN = 'U',
    CM_CHARGE_EMPTY='E',
    CM_CHARGE_CHARGING='C', 
    CM_CHARGE_DISCHARGING='D',
    CM_CHARGE_FULL='F'
} cmcharge_t;


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

// The CMBAT_T struct tracks the battery properties
// Parameters that are calculated from raw configuration parameters
// are preceeded with an underscore.
typedef struct _cmbat_t {
    // ** Statically configured model parameters **
    // Resistance model
    double R1_ref;      // Initial terminal resistance at reference temperature
    double R1_T;        // R1 temperature coefficient
    double R2_ref;      // Rise in terminal resistance at reference temperature
    double R2_T;        // R2 temnperature coefficient
    // Time parameters
    double tc;      // Time constant for resistance relaxation
    double ts;      // Sample interval
    // Temperature parameters
    double Tref;        // reference temperature in (Kelvin)
    // Voltage parameters
    //  V = v0 * (1 + c1/T + (c2/T)*(c2/T))
    double Vfull_ref;   // OCV at full charge at Tref
    double Vfull_c1;    //   temperature coefficient 1 (Kelvin)
    double Vfull_c2;    //   temperature coefficient 2 (Kelvin)
    double Vdisch_ref;  // OCV at fully discharged at Tref
    double Vdisch_c1;   //   temperature coefficient 1 (Kelvin)
    double Vdisch_c2;   //   temperature coefficient 2 (Kelvin)
    
    // ** parameters updated by CMSTEP() **
    unsigned long int uptime;   // Number of CMSTEP() calls
    double Vt;          // Last terminal voltage measured
    double I;           // Last measured current
    double T;           // Last measured temperature
    double Voc;         // last open-circuit voltage calculation
    double Q;           // last coulomb count
    // Signal statistics
    cmstat_t Istat;     // Current
    cmstat_t Vstat;     // Terminal voltage
    cmstat_t Tstat;     // Temperature
    
    // ** parameters updated by CMUPDATE() **
    double soc;         // State of charge estimate
    double Vfull;       // OCV at full charge at T (calculated)
    double Vdisch;      // OCV at fully discharged at T (calculated)
    cmcharge_t chargestate; // ENUM establishing the current operating mode
    
    // ** Quasi-private state structs **
    // Dynamic models
    cmtf_t _vttf;
    cmtf_t _qtf;
    cmtf_t _etf;
    
} cmbat_t;



/* CMCONFIG - init and load params from a configuration file
 * 
 * cmconfig(cmbat_t *bat, char *filename, double ts);
 *      The TS argument specifies the period between current, voltage,
 *      and temperature measurements in seconds.  It is specified by the
 *      application and NOT by the configuration file.
 * 
 * Parameters are expected in whitespace separated parameter-value pairs
 * All parameters are case-insensitive strings, and all values are 
 * interpreted as floating point numbers.  Below, parameters are listed
 * along with their default value and their units.
 * 
 *      TREF        (298.15, K)
 * The reference temperature in Kelvin.  This is the temperature at 
 * which all parameters (like R1, R2, etc...) are specified.
 * 
 *      R1          (0.0, ohms)
 * The initial or small-current terminal resistance at Tref in ohms.
 * See the description above for the equivalent circuit model.
 *
 *      R1_TEMP     (0.0, ohms)
 * The temperature coefficient for the R1 terminal resistance in ohms/K.
 * When non-zero, this is used to linearly adjust the terminal 
 * resistance with changes in temperature.
 * 
 *      R2          (0.0, ohms)
 * The steady-state rise in terminal resistance at Tref in ohms.  See
 * the description above for the equivalent circuit model.
 * 
 *      R2_TEMP     (0.0, ohms)
 * The temperature coefficient for R2 in ohms/K.  When non-zero this is
 * used to linearly adjust R2 for changes in temperature.
 * 
 *      TC          (1, sec)
 * The battery's time constant in seconds.  When discharge (or charge)
 * begins, the C*R2 time constant determines how long it takes for the
 * terminal voltage to settle into its steady-state value.  Rather than
 * asking the user to estimate capacitance, TC is a parameter that is 
 * easier to observe experimentally.
 * 
 *      VFULL       (12.9, V)
 * The open-circuit voltage of the battery when it is fully charged at
 * temperature, Tref.  See the model above for how R1, R2, and TC are
 * used to relate terminal and open-circuit voltage.  See also VDISCH.
 * 
 *      VFULL_C1, VFULL_C2     (0.0, K)
 * First and second temperature coefficients for the fully charged open-
 * circuit voltage.  Vfull(T) = Vinf( 1 + C1/T + C2*C2/T/T )  Vinf is 
 * calculated such that Vfull(Tref) matches the value specified by VFULL.
 * See also VDISCH_C1 and VDISCH_C2.
 * 
 *      VDISCH      (11.55, V)
 * The open-circuit voltage of the battery when it is fully discharged
 * at temperature, Tref.  
 * 
 *      VDISCH_C1, VDISCH_C2     (0.0, K)
 * First and second temperature coefficients for the fully discharged 
 * open-circuit voltage.  Vfull(T) = Vinf( 1 + C1/T + C2*C2/T/T )  Vinf
 * is calculated such that Vfull(Tref) matches the value specified by 
 * VDISCH.
 */
int cmconfig(cmbat_t *bat, char *filename, double ts);

/* CMVFULL, CMVDISCH - calculate fully (dis)charged OCV in volts.
 *  Returns the fully (dis)charged open-circuit voltage in volts.
 *  BAT - the battery model struct
 * 
 * Uses the last measured temperature registered with CMSTEP() to 
 * compensate for temperature.
 */
double cmvfull(cmbat_t *bat);
double cmvdisch(cmbat_t *bat);

/* CMR1, CMR2 - calculate the terminal resistances in ohms
 *  Returns R1 and R2 in ohms.
 *  BAT - the battery model struct
 * 
 * Uses the last measured temperature registered with CMSTEP() to 
 * compensate for temperature.
 */
double cmr1(cmbat_t *bat);
double cmr2(cmbat_t *bat);

/* CMSOC - state of charge estimate
 *  Uses cmvfull() and cmvdisch() to estimate fully charged and 
 * discharged voltages.  The open-circuit voltage estimate is then used
 * to interpolate,
 * 
 *  soc = (Voc - Vdisch) / (Vfull - Vdisch)
 * 
 * Nominally, the soc parameter should be clamped to be between 0 and 1,
 * but it is diagnostically important to record conditions when the 
 * battery is above full or below fully discharged.  
 */
double cmsoc(cmbat_t *bat);

/* CMSTEP - updates all battery model parameters with a new measurement
 * 
 * Accepts the terminal current and the terminal voltage as inputs. 
 * Updates the following public struct parameters:
 * 
 * uptime   --  integer, number of sample intervals since init
 * Vt       --  double, mean terminal voltage over last cycle
 * I        --  double, mean current over the last 60Hz cycle
 * T        --  double, mean temperature in K
 * Voc      --  double, latest OCV calculation
 * Q        --  double, coulumb counter integral since last reset
 * 
 */
int cmstep(cmbat_t *bat, double I, double V, double T);

/* CMUPDATE - Called periodically to update the params not maintained by
 *  CMSTEP.
 * 
 * Accepts the terminal current and the terminal voltage as inputs. 
 * Returns 1 if the charge state has changed since the last update. 
 * Otherwise, returns 0.
 * 
 * The coloumb count in the bat.Q member is reset on the next call after
 * a change of charge state, so its value should be read immediately if
 * it is needed.
 * 
 * Returns a Charge Model EVENT, which passes important events back to
 * the application.
 */
int cmupdate(cmbat_t *bat);

/* CMRESET - resets the istat, vstat, and tstat statistics structs
 * 
 * This should be called to start a new accumulation interval.  It does
 * not clear the public members in the structs until the next call to 
 * cmstep().
 */
void cmreset(cmbat_t *bat);

#endif
