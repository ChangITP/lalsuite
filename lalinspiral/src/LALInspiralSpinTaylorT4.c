/*
 * Copyright (C) 2011 E. Ochsner, M. Agathos
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 *  MA  02111-1307  USA
 */

#include <math.h>
#include <lal/Units.h>
#include <lal/LALConstants.h>
#include <lal/LALSimInspiral.h>
#include <lal/LALInspiral.h>
#include <lal/LALAdaptiveRungeKutta4.h>
#include <lal/TimeSeries.h>
#include "check_series_macros.h"

#define DEBUG 0

#define UNUSED(expr) do { (void)(expr); } while (0)
/*
#ifdef __GNUC__
#define UNUSED __attribute__ ((unused))
#else
#define UNUSED
#endif
*/

NRCSID (LALINSPIRALSPINTAYLORT4C, "$Id$");

/* use error codes above 1024 to avoid conflicts with GSL */
#define LALSIMINSPIRAL_ST4_TEST_ENERGY 			1025
#define LALSIMINSPIRAL_ST4_TEST_OMEGADOT 		1026
#define LALSIMINSPIRAL_ST4_TEST_COORDINATE 		1027
#define LALSIMINSPIRAL_ST4_TEST_OMEGANAN 		1028
#define LALSIMINSPIRAL_ST4_TEST_FREQBOUND 		1029
#define LALSIMINSPIRAL_ST4_DERIVATIVE_OMEGANONPOS 	1030

/* (2x) Highest available PN order - UPDATE IF NEW ORDERS ADDED!!*/
#define LAL_MAX_PN_ORDER 8
/* Number of variables used for precessing waveforms */
#define LAL_NUM_ST4_VARIABLES 14
/* absolute and relative tolerance for adaptive Runge-Kutta ODE integrator */
/* 1.e-06 is too large for end of 1.4--1.4 M_sun BNS inspiral */
/* (phase difference at end will be ~10% of GW cycle). */
/* 1.e-12 is used so last data point isn't nan for 6PN tidal, */
/* since larger values probably cause larger step sizes. */
#define LAL_ST4_ABSOLUTE_TOLERANCE 1.e-12
#define LAL_ST4_RELATIVE_TOLERANCE 1.e-12

/** Default values for enumerated flag */
#define LAL_SIM_INSPIRAL_INTERACTION_DEFAULT LAL_SIM_INSPIRAL_INTERACTION_ALL

/**
 * (Twice) the highest known PN order of amplitude correction for
 * non-precessing binaries.
 */
#define MAX_NONPRECESSING_AMP_PN_ORDER 5

/**
 * (Twice) the highest known PN order of amplitude correction for
 * precessing binaries.
 */
#define MAX_PRECESSING_AMP_PN_ORDER 3

/**
 * Struct containing all of the non-dynamical coefficients needed
 * to evolve a spinning, precessing binary and produce a waveform.
 * This struct is passed to the 2 static functions below: 
 * Derivatives and StoppingTest
 */
typedef struct tagXLALSimInspiralSpinTaylorT4Coeffs
{
	REAL8 M; ///< total mass in seconds
	REAL8 Mchirp; ///< chirp mass in seconds
	REAL8 eta; ///< symmetric mass ratio
	REAL8 m1M; ///< m1 / M
	REAL8 m2M; ///< m2 / M
	REAL8 wdotnewt; ///< leading order coefficient of wdot = \f$\dot{\omega}\f$
	REAL8 wdotcoeff[LAL_MAX_PN_ORDER]; ///< coeffs. of PN corrections to wdot
	REAL8 wdotlogcoeff; ///< coefficient of log term in wdot
	REAL8 Ecoeff[LAL_MAX_PN_ORDER]; ///< coeffs. of PN corrections to energy
	REAL8 wdotSO15s1, wdotSO15s2; ///< non-dynamical 1.5PN SO corrections
	REAL8 wdotSS2; ///< non-dynamical 2PN SS correction
	REAL8 wdotQM2S1; ///< non-dynamical S1^2 2PN quadrupole-monopole correction
	REAL8 wdotQM2S1L; ///< non-dynamical (S1.L)^2 2PN quadrupole-monopole correction
	REAL8 wdotQM2S2; ///< non-dynamical S2^2 2PN quadrupole-monopole correction
	REAL8 wdotQM2S2L; ///< non-dynamical (S2.L)^2 2PN quadrupole-monopole correction
	REAL8 wdotSSselfS1; ///< non-dynamical S1^2 2PN self-spin correction
	REAL8 wdotSSselfS1L; ///< non-dynamical (S1.L)^2 self-spin correction
	REAL8 wdotSSselfS2; ///< non-dynamical S2^2 2PN self-spin correction
	REAL8 wdotSSselfS2L; ///< non-dynamical (S2.L)^2 self-spin correction
	REAL8 wdotSO25s1, wdotSO25s2; ///< non-dynamical 2.5PN SO corrections
	REAL8 ESO15s1, ESO15s2; ///< non-dynamical 1.5PN SO corrections
	REAL8 ESS2; ///< non-dynamical 2PN SS correction
	REAL8 EQM2S1; ///< non-dynamical S1^2 2PN quadrupole-monopole correction
	REAL8 EQM2S1L;///< non-dynamical (S1.L)^2 2PN quadrupole-monopole correction
	REAL8 EQM2S2; ///< non-dynamical S2^2 2PN quadrupole-monopole correction
	REAL8 EQM2S2L;///< non-dynamical (S2.L)^2 2PN quadrupole-monopole correction
	REAL8 ESO25s1, ESO25s2; ///< non-dynamical 2.5PN SO corrections 
	REAL8 LNhatSO15s1, LNhatSO15s2; ///< non-dynamical 1.5PN SO corrections
	REAL8 LNhatSS2; ///< non-dynamical 2PN SS correction 
	REAL8 wdottidal5pn;	///< leading order tidal correction 
	REAL8 wdottidal6pn;	///< next to leading order tidal correction
	REAL8 Etidal5pn; ///< leading order tidal correction to energy
	REAL8 Etidal6pn; ///< next to leading order tidal correction to energy
	REAL8 fStart; ///< starting GW frequency of integration
	REAL8 fEnd; ///< ending GW frequency of integration
} XLALSimInspiralSpinTaylorT4Coeffs;

/* Declarations of static functions - defined below */
static int XLALSimInspiralSpinTaylorT4StoppingTest(double t, 
	const double values[], double dvalues[], void *mparams);
static int XLALSimInspiralSpinTaylorT4Derivatives(double t, 
	const double values[], double dvalues[], void *mparams);

/**
 * This function serves as an interface for porting the SpinTaylorT4
 * waveform generator into the lalinspiral package for injection. 
 * This waveform is originally coded for use within lalsimulation.
 * The output signal is of the form CoherentGW instead of the 
 * REAL8TimeSeries that is originally output by the waveform generator.
 * This is in line with other time-domain injected waveforms previously 
 * used in lalnest.
 **/
 
void LALInspiralInterfaceSpinTaylorT4(
	LALStatus *status, 
	CoherentGW *waveform, 
	InspiralTemplate *params,
	PPNParamStruc *ppnParams,
	REAL8 *dxis, 
	REAL8 cutoff,
	LALInspiralEOS EoS
	)
{
	INITSTATUS (status, "LALInspiralInterfaceSpinTaylorT4", LALINSPIRALSPINTAYLORT4C);
	ATTATCHSTATUSPTR(status);
	/* check inputs */
	ASSERT (waveform,  status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
	ASSERT (params, status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL);
    ASSERT (params->nStartPad >= 0, status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
    ASSERT (params->nEndPad >= 0, status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
    ASSERT (params->fLower > 0.0, status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
    ASSERT (params->tSampling > 0.0, status, LALINSPIRALH_ESIZE, LALINSPIRALH_MSGESIZE);
    ASSERT( !( waveform->a ), status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
    ASSERT( !( waveform->h ), status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
    ASSERT( !( waveform->f ), status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
    ASSERT( !( waveform->phi ), status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
    ASSERT( !( waveform->shift ), status, LALINSPIRALH_ENULL, LALINSPIRALH_MSGENULL );
	
	REAL8TimeSeries *hplusptr = NULL;
	REAL8TimeSeries *hcrossptr = NULL;
	REAL8 deltaT, m1, m2, r;
	REAL8 fStart, fRef, fEnd, phiRef, v0;
	LIGOTimeGPS *endTime;
	LALPNOrder phaseOrder, amplitudeOrder;
	REAL8 i, s1x, s1y, s1z, s2x, s2y, s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z;
	REAL8 mttot, m1_intr, m2_intr, lambda1, lambda2, qmparam1, qmparam2, OnePlusZ; 
	LALSpinInteraction spinInteraction;
	LALSimInspiralInteraction simInteraction = LAL_SIM_INSPIRAL_INTERACTION_DEFAULT;
	
	i = params->inclination;
	lnhatx = sin(i);
	lnhaty = 0.0;
	lnhatz = cos(i);
	e1x = cos(i);
	e1y = 0.0;
	e1z = -sin(i);
	phaseOrder = params->order;
	amplitudeOrder = params->ampOrder;
    amplitudeOrder = amplitudeOrder <= MAX_PRECESSING_AMP_PN_ORDER ? amplitudeOrder : MAX_PRECESSING_AMP_PN_ORDER;
    m1 = params->mass1 * LAL_MSUN_SI;    /** has to be in kg **/
    m2 = params->mass2 * LAL_MSUN_SI;    /** has to be in kg **/
    OnePlusZ = (1.0 + 6.932E4/LAL_C_SI*(params->distance)/(1.0E6*LAL_PC_SI)); 
    m1_intr = params->mass1*OnePlusZ; /** intrinsic masses in Msun **/
    m2_intr = params->mass2*OnePlusZ;
    mttot = (m1_intr + m2_intr)*LAL_MTSUN_SI; /** intrinsic total mass in seconds **/

    /** Calculate tidal deformability parameters for the two bodies (dimensionless) **/
    lambda1 = XLALLambdaOfM_EOS(EoS, m1_intr)/pow(mttot,5.); /** Tidal effect for mass1 switched on **/
    lambda2 = XLALLambdaOfM_EOS(EoS, m2_intr)/pow(mttot,5.); /** Tidal effect for mass2 switched on **/
    lambda1 = (lambda1 > 0.) ? lambda1 : 0.0;
    lambda2 = (lambda2 > 0.) ? lambda2 : 0.0;
    /** Calculate quadrupole-monopole parameters for the two bodies **/
    qmparam1 = XLALQMparameter_EOS(EoS, m1_intr); 
    qmparam2 = XLALQMparameter_EOS(EoS, m2_intr); 
    
    fStart = params->fLower;
	fRef = 0.0;
	fEnd = cutoff;
	phiRef = params->startPhase;
	deltaT = 1./params->tSampling;
	r = params->distance;  /** has to be in m **/
	v0 = 1.0;
	s1x = params->spin1[0];
	s1y = params->spin1[1];
	s1z = params->spin1[2];
	s2x = params->spin2[0];
	s2y = params->spin2[1];
	s2z = params->spin2[2];
	spinInteraction = params->spinInteraction;
	endTime = (LIGOTimeGPS*) LALCalloc( 1, sizeof(LIGOTimeGPS));
	endTime->gpsSeconds = params->end_time.gpsSeconds;
	endTime->gpsNanoSeconds = params->end_time.gpsNanoSeconds;
	
	/* Translate LALSpinInteraction to LALSimInspiralInteraction */
	int NOinter = 0 ;
	int SOinter = 1 << 5 | 1 << 4 | 1 ;  /** this defines the spin-orbit interaction to include up to 3PN contributions **/
	int SSinter = 1 << 1 ;  /** Spin-Spin interaction 2PN **/
	int SSselfinter = 1 << 2 ;  /** Spin-Spin self-interaction 2PN **/
	int Allinter = (1 << 6) - 1 ;  /** this includes all spin interactions (no tidal) **/
	int QMinter = 1 << 3 ; /** quadrupole-monopole interaction **/
	
	if( spinInteraction == LAL_NOInter ) simInteraction = NOinter ;
	else if( spinInteraction == LAL_AllInter ) simInteraction = Allinter ;	
	else {
		if( (spinInteraction & LAL_SOInter) == LAL_SOInter ) simInteraction |= SOinter ;	
		if( (spinInteraction & LAL_SSInter) == LAL_SSInter ) simInteraction |= SSinter ;	
		if( (spinInteraction & LAL_SSselfInter) == LAL_SSselfInter ) simInteraction |= SSselfinter ;	
		if( (spinInteraction & LAL_QMInter) == LAL_QMInter ) simInteraction |= QMinter ;	
	}
		
	/* DEBUGGING PRINT */
#if DEBUG
	printf(	"XLALSimInspiralSpinTaylorT4( hplus pointer = %ld , \nhcrossptr = %ld , \nphiRef = %e , \nv0 = %e ,\ndeltaT = %e ,\nm1 = %e ,\nm2 = %e ,\nfStart = %e ,\nfRef = %e , \
	\nr = %e ,\ns1x = %e ,\ns1y = %e ,\ns1z = %e ,\ns2x = %e ,\ns2y = %e ,\ns2z = %e ,\nlnhatx = %e ,\nlnhaty = %e ,\nlnhatz = %e ,\ne1x = %e ,\ne1y = %e ,\ne1z = %e ,\nlambda1 = %e , \
	\nlambda2 = %e ,\nqmparam1 = %e ,\nqmparam2 = %e ,\nsimInteraction = %i ,\nphaseOrder = %d ,\namplitudeOrder = %d,\ndxi[0] = %e,\ndxi[1] = %e,\ndxi[2] = %e,\ndxi[3] = %e)\n", (long int) &hplusptr, (long int) &hcrossptr, phiRef, v0, deltaT, m1, m2, fStart, 
	fRef, r, s1x, s1y, s1z, s2x, s2y, s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, lambda1, lambda2, qmparam1, qmparam2, simInteraction, phaseOrder, params->ampOrder, dxis[0], dxis[1], dxis[2], dxis[3]);
#endif

	/* Call the SpinTaylorT4 generator */
	XLALSimInspiralSpinTaylorT4(&hplusptr, &hcrossptr, phiRef, v0, deltaT, m1, m2, fStart, fRef, r, s1x, s1y, s1z, s2x, s2y, s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, lambda1, lambda2, qmparam1, qmparam2, simInteraction, phaseOrder, amplitudeOrder, dxis);
	
    UINT4 bins = hplusptr->data->length;

	/* Fill in the CoherentGW structure */

    /* Allocate the waveform structures. */
    
    if ( ( waveform->h = (REAL4TimeVectorSeries *)
                LALMalloc( sizeof(REAL4TimeVectorSeries) ) ) == NULL ) {
        ABORT( status, LALINSPIRALH_EMEM,
                LALINSPIRALH_MSGEMEM );
    }
    memset( waveform->h, 0, sizeof(REAL4TimeVectorSeries) );
    
    waveform->h->deltaT = hplusptr->deltaT;
    waveform->h->f0 = hplusptr->f0;
    waveform->h->sampleUnits = hplusptr->sampleUnits;
    waveform->h->epoch.gpsSeconds = hplusptr->epoch.gpsSeconds; 
    waveform->h->epoch.gpsNanoSeconds = hplusptr->epoch.gpsNanoSeconds;
    waveform->position = ppnParams->position;
    waveform->psi = ppnParams->psi;
    snprintf( waveform->h->name, LALNameLength, "SpinTaylorT4 polarizations");

    waveform->h->data = XLALCreateREAL4VectorSequence(bins, 2);

    /* Copy the data from REAL8TimeSeries to a REAL4TimeVectorSeries */

    REAL4Vector *hplus4 = (REAL4Vector *)XLALCreateREAL4Vector(bins);
    REAL4Vector *hcross4 = (REAL4Vector *)XLALCreateREAL4Vector(bins);

    UINT4 j;
    for (j=0;j<bins;j++) {
        hplus4->data[j]=(REAL4)hplusptr->data->data[j];  /* Move into a REAL4 vector */
        hcross4->data[j]=(REAL4)hcrossptr->data->data[j];
        waveform->h->data->data[2*j] = (REAL4)hplusptr->data->data[j];  /* the convention is writing hp and hc in alternating sequence */
        waveform->h->data->data[2*j+1] = (REAL4)hcrossptr->data->data[j];
	}

	/* print the REAL4Vector to file */
	
#if DEBUG
    FILE *outInj4=fopen("SpinTaylorT4_preInj4.dat","w");
    for (j=0; j<hplusptr->data->length; j++) {
            fprintf(outInj4, "%lf %e %e\n", j*deltaT, waveform->h->data->data[2*j], waveform->h->data->data[2*j + 1]);
    }
    fclose(outInj4);
#endif
	
    /* fill some output */
    ppnParams->tc     = (double)(bins-1) * deltaT ;
    ppnParams->length = bins;
    ppnParams->dfdt = 0.0;  // NOT USED ???
    ppnParams->fStop  = params->fFinal;
    ppnParams->fStart   = ppnParams->fStartIn;
    // ppnParams->termCode        = GENERATEPPNINSPIRALH_EFSTOP;
    // ppnParams->termDescription = GENERATEPPNINSPIRALH_MSGEFSTOP;

	/* Destroy all */
	
	XLALDestroyREAL4Vector(hplus4);
	XLALDestroyREAL4Vector(hcross4);
	XLALDestroyREAL8TimeSeries(hplusptr);
	XLALDestroyREAL8TimeSeries(hcrossptr);
	LALFree(endTime);
	
    DETATCHSTATUSPTR( status );
    RETURN( status );
}


/**
 * This function evolves the orbital equations for a precessing binary using 
 * the \"TaylorT4\" approximant for solving the orbital dynamics 
 * (see arXiv:0907.0700 for a review of the various PN approximants).
 *
 * It returns time series of the \"orbital velocity\", orbital phase, 
 * and components for both individual spin vectors, the \"Newtonian\"
 * orbital angular momentum (which defines the instantaneous plane)
 * and "E1", a basis vector in the instantaneous orbital plane.
 * Note that LNhat and E1 completely specify the instantaneous orbital plane.
 * It also returns the time and phase of the final time step
 *
 * For input, the function takes the two masses, the initial orbital phase, 
 * Values of S1, S2, LNhat, E1 vectors at starting time,
 * the desired time step size, the starting GW frequency, 
 * and PN order at which to evolve the phase,
 * 
 * NOTE: All vectors are given in the so-called "radiation frame", 
 * where the direction of propagation is the z-axis, the principal "+" 
 * polarization axis is the x-axis, and the y-axis is given by the RH rule.
 * You must give the initial values in this frame, and the time series of the
 * vector components will also be returned in this frame
 */
int XLALSimInspiralPNEvolveOrbitSpinTaylorT4(
	REAL8TimeSeries **V,          /**< post-Newtonian parameter [returned]*/
	REAL8TimeSeries **Phi,        /**< orbital phase            [returned]*/
	REAL8TimeSeries **S1x,	      /**< Spin1 vector x component [returned]*/
	REAL8TimeSeries **S1y,	      /**< "    "    "  y component [returned]*/
	REAL8TimeSeries **S1z,	      /**< "    "    "  z component [returned]*/
	REAL8TimeSeries **S2x,	      /**< Spin2 vector x component [returned]*/
	REAL8TimeSeries **S2y,	      /**< "    "    "  y component [returned]*/
	REAL8TimeSeries **S2z,	      /**< "    "    "  z component [returned]*/
	REAL8TimeSeries **LNhatx,     /**< unit orbital ang. mom. x [returned]*/
	REAL8TimeSeries **LNhaty,     /**< "    "    "  y component [returned]*/
	REAL8TimeSeries **LNhatz,     /**< "    "    "  z component [returned]*/
	REAL8TimeSeries **E1x,	      /**< orb. plane basis vector x[returned]*/
	REAL8TimeSeries **E1y,	      /**< "    "    "  y component [returned]*/
	REAL8TimeSeries **E1z,	      /**< "    "    "  z component [returned]*/
	REAL8 deltaT,          	      /**< sampling interval (s) */
	REAL8 m1,              	      /**< mass of companion 1 (kg) */
	REAL8 m2,              	      /**< mass of companion 2 (kg) */
	REAL8 fStart,                 /**< starting GW frequency */
	REAL8 fEnd,                   /**< ending GW frequency, fEnd=0 means integrate as far forward as possible */
	REAL8 s1x,                    /**< initial value of S1x */
	REAL8 s1y,                    /**< initial value of S1y */
	REAL8 s1z,                    /**< initial value of S1z */
	REAL8 s2x,                    /**< initial value of S2x */
	REAL8 s2y,                    /**< initial value of S2y */
	REAL8 s2z,                    /**< initial value of S2z */
	REAL8 lnhatx,                 /**< initial value of LNhatx */
	REAL8 lnhaty,                 /**< initial value of LNhaty */
	REAL8 lnhatz,                 /**< initial value of LNhatz */
	REAL8 e1x,                    /**< initial value of E1x */
	REAL8 e1y,                    /**< initial value of E1y */
	REAL8 e1z,                    /**< initial value of E1z */
	REAL8 lambda1,                /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
	REAL8 lambda2,                /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
	REAL8 quadparam1,             /**< quadrupole-monopole parameter of mass 1 **/
	REAL8 quadparam2,             /**< quadrupole-monopole parameter of mass 2 **/
	LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
	INT4 phaseO,                  /**< twice post-Newtonian order */
    REAL8 *dxis                   /**< testing GR parameters */
	)
{
	static const char *func = "XLALSimInspiralPNEvolveOrbitSpinTaylorT4";
    INT4 intreturn;
    XLALSimInspiralSpinTaylorT4Coeffs params;/* Frequently used coefficients */
    ark4GSLIntegrator *integrator = NULL;     /* GSL integrator object */
    REAL8 yinit[LAL_NUM_ST4_VARIABLES];       /* initial values of parameters */
    REAL8Array *yout;	 /* time series of variables returned from integrator */
    /* intermediate variables */
    UINT4 i, cutlen, len;
    int sgn, offset;
    REAL8 m1m2, m2m1, dm, M, eta, Mchirp, norm, dtStart, dtEnd, lengths, wEnd;
    LIGOTimeGPS tStart = LIGOTIMEGPSZERO;
    REAL8 m1M, m2M; /* m1/M, m2/M */

    /* Check start and end frequencies are positive */
    if( fStart <= 0. )
    {
        XLALPrintError("XLAL Error - %s: fStart = %f must be > 0.\n", 
                __func__, fStart );
        XLAL_ERROR(func, XLAL_EINVAL);
    }
    if( fEnd < 0. ) /* fEnd = 0 allowed as special case */
    {
        XLALPrintError("XLAL Error - %s: fEnd = %f must be >= 0.\n", 
                __func__, fEnd );
        XLAL_ERROR(func, XLAL_EINVAL);
    }

    /* Set sign of time step according to direction of integration */
    if( fEnd < fStart && fEnd != 0. )
        sgn = -1;
    else
        sgn = 1;

    /* Check start and end frequencies are positive */
    if( fStart <= 0. )
    {
        XLALPrintError("XLAL Error - %s: fStart = %f must be > 0.\n", 
                __func__, fStart );
        XLAL_ERROR(func, XLAL_EINVAL);
    }
    if( fEnd < 0. ) /* fEnd = 0 allowed as special case */
    {
        XLALPrintError("XLAL Error - %s: fEnd = %f must be >= 0.\n", 
                __func__, fEnd );
        XLAL_ERROR(func, XLAL_EINVAL);
    }

    /* Zero the coefficients */
    memset(&params, 0, sizeof(XLALSimInspiralSpinTaylorT4Coeffs));

    /* Define mass variables and other coefficients */
    m1m2 = m1 / m2;
    m2m1 = m2 / m1;
    m1 *= LAL_G_SI / LAL_C_SI / LAL_C_SI / LAL_C_SI; /* convert m1 from kg to seconds */
    m2 *= LAL_G_SI / LAL_C_SI / LAL_C_SI / LAL_C_SI; /* convert m2 from kg to seconds */
    M = m1 + m2;
    dm = (m1 - m2)/M;
    m1M = m1 / M;
    m2M = m2 / M;
    eta = m1 * m2 / M / M;
    Mchirp = M * pow(eta, 3./5.);
    params.M = M;
    params.Mchirp = Mchirp;
    params.eta = eta;
    params.m1M = m1M;
    params.m2M = m2M;
    params.wdotnewt = (96.0/5.0) * eta;
    params.fStart = fStart;
    params.fEnd = fEnd;
	
    /** 
     * Set coefficients up to PN order phaseO. 
     * epnorb is the binary energy and
     * wdotorb is the derivative of the orbital frequency \f$\dot{\omega}\f$.
     * These are just the non-spinning contributions.
     * Spin corrections must be recomputed at every step
     * because the relative orientations of S, L can change
     *
     * The values can be found in Buonanno, Iyer, Ochsner, Pan and Sathyaprakash
     * Phys. Rev. D 80, 084043 (2009) arXiv:0907.0700 (aka \"BIOPS\")
     * Eq. 3.1 for the energy and Eq. 3.6 for \f$\dot{\omega}\f$
     *
     * Note that Eq. 3.6 actually gives dv/dt, but this relates to \f$\omega\f$
     * by \f$d (M \omega)/dt = d (v^3)/dt = 3 v^2 dv/dt\f$
     * so the PN corrections are the same 
     * but the leading order is 3 v^2 times Eq. 3.6
     */
    switch( phaseO )
    {
        case -1: /* highest available PN order */
        case 8:
        /* case LAL_PNORDER_THREE_POINT_FIVE: */
        case 7:
            params.wdotcoeff[7] = (LAL_PI/12096.0) 
                    * (-13245.0 + 717350.0*eta + 731960.0*eta*eta);
            params.Ecoeff[7] = 0.;
        /* case LAL_PNORDER_THREE: */
        case 6:
            params.wdotcoeff[6] = 16447322263./139708800. - 1712./105. 
                    * LAL_GAMMA - 56198689./217728. * eta + LAL_PI * LAL_PI 
                    * (16./3. + 451./48. * eta) + 541./896. * eta * eta 
                    - 5605./2592. * eta * eta * eta - 856./105. * log(16.);
            params.wdotlogcoeff = - 1712./315.;
            params.Ecoeff[6] = - 675./64. + ( 34445./576. 
                    - 205./96. * LAL_PI * LAL_PI ) * eta
                    - (155./96.) *eta * eta - 35./5184. * eta * eta * eta;
        /* case LAL_PNORDER_TWO_POINT_FIVE: */
        case 5:
            params.wdotcoeff[5] = -(1./672.) * LAL_PI * (4159. + 15876.*eta);
            params.Ecoeff[5] = 0.;
        /* case LAL_PNORDER_TWO: */
        case 4:
            params.wdotcoeff[4] = (34103. + 122949.*eta 
                    + 59472.*eta*eta)/18144.;
            params.Ecoeff[4] = (-81. + 57.*eta - eta*eta)/24.;
        /*case LAL_PNORDER_ONE_POINT_FIVE:*/
        case 3:
            params.wdotcoeff[3] = 4. * LAL_PI;
            params.Ecoeff[3] = 0.;
        /*case LAL_PNORDER_ONE:*/
        case 2:
            params.wdotcoeff[2] = -(1./336.) * (743. + 924.*eta);
            params.Ecoeff[2] = -(1.0/12.0) * (9.0 + eta);
        /*case LAL_PNORDER_HALF:*/
        case 1:
            params.wdotcoeff[1] = 0.;
            params.Ecoeff[1] = 0.;
        /*case LAL_PNORDER_NEWTONIAN:*/
        case 0:
            params.wdotcoeff[0] = 1.;
            params.Ecoeff[0] = 1.;
            break;
        default: 
            XLALPrintError("XLAL Error - %s: Invalid phase. PN order %s\n", 
                    __func__, phaseO );
            XLAL_ERROR(func, XLAL_EINVAL);
            break;
    }

  /* The \dot\omega PN coefficients in the d.e. are shifted for modGR signals */
  
    params.wdotcoeff[0]*=1.0+dxis[0];
    params.wdotcoeff[1]=dxis[1];
    params.wdotcoeff[2]*=1.0+dxis[2];
    params.wdotcoeff[3]*=1.0+dxis[3];
    params.wdotcoeff[4]*=1.0+dxis[4];
    params.wdotcoeff[5]*=1.0+dxis[5];
    params.wdotcoeff[6]*=1.0+dxis[6];
    params.wdotlogcoeff*=1.0+dxis[7];
    params.wdotcoeff[7]*=1.0+dxis[8];


    /**
     * Compute the non-dynamical coefficients of spin corrections 
     * to the evolution equations for omega, L, S1 and S2 and binary energy E.
     * Flags control which spin corrections are included
     */
    params.wdotSO15s1 		= 0.;
    params.wdotSO15s2 		= 0.;
    params.wdotSS2 			= 0.;
    params.wdotQM2S1 		= 0.;
    params.wdotQM2S1L 		= 0.;
    params.wdotQM2S2 		= 0.;
    params.wdotQM2S2L 		= 0.;
    params.wdotSO25s1 		= 0.;
    params.wdotSO25s2 		= 0.;
    params.ESO15s1 			= 0.;
    params.ESO15s2 			= 0.;
    params.ESS2 			= 0.;
    params.EQM2S1 			= 0.;
    params.EQM2S1L 			= 0.;
    params.EQM2S2 			= 0.;
    params.EQM2S2L 			= 0.;
    params.ESO25s1 			= 0.;
    params.ESO25s2 			= 0.;
    params.LNhatSO15s1 		= 0.;
    params.LNhatSO15s2  	= 0.;
    params.LNhatSS2 		= 0.;
    
    if( (interactionFlags & LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_15PN) == LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_15PN )
    {
        params.wdotSO15s1 	= - ( 113. + 75. * m2m1 ) / 12.;
        params.wdotSO15s2 	= - ( 113. + 75. * m1m2 ) / 12.;
        params.ESO15s1 		= 8./3. + 2. * m2m1;
        params.ESO15s2 		= 8./3. + 2. * m1m2;
        params.LNhatSO15s1 	= 2. + 3./2. * m2m1;
        params.LNhatSO15s2	= 2. + 3./2. * m1m2;
    }
    if( (interactionFlags & LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_2PN) == LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_2PN )
    {
        params.wdotSS2 		= - 1. / 48. / eta;
        params.ESS2 		= 1. / eta;
        params.LNhatSS2 	= -1.5 / eta;
    }
    if( (interactionFlags & LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_SELF_2PN) == LAL_SIM_INSPIRAL_INTERACTION_SPIN_SPIN_SELF_2PN )
    {
        params.wdotSSselfS1	= 7. / 96. / m1M / m1M;
        params.wdotSSselfS1L= -1. / 96. / m1M / m1M;
        params.wdotSSselfS2	= 7. / 96. / m2M / m2M;
        params.wdotSSselfS2L= -1. / 96. / m2M / m2M;
    }
    if( (interactionFlags & LAL_SIM_INSPIRAL_INTERACTION_QUAD_MONO_2PN) == LAL_SIM_INSPIRAL_INTERACTION_QUAD_MONO_2PN )
    {
		params.wdotQM2S1 	= -quadparam1 * 5. / 2. / m1M / m1M;
		params.wdotQM2S1L 	= quadparam1 * 15. / 2. / m1M / m1M;
		params.wdotQM2S2 	= -quadparam2 * 5. / 2. / m2M / m2M;
		params.wdotQM2S2L 	= quadparam2 * 15. / 2. / m2M / m2M;
		params.EQM2S1 		= quadparam1 / 2. / m1M / m1M;
		params.EQM2S1L 		= -quadparam1 * 3. / 2. / m1M / m1M;
		params.EQM2S2 		= quadparam2 / 2. / m2M / m2M;
		params.EQM2S2L 		= -quadparam2 * 3. / 2. / m2M / m2M;
    }
    if( (interactionFlags & LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_25PN) == LAL_SIM_INSPIRAL_INTERACTION_SPIN_ORBIT_25PN )
    {
        params.wdotSO25s1 = -5861./144. + 1001.*eta/12. + (dm/m1M) * (809./84. - 281.*eta/8.);
        params.wdotSO25s2 = -5861./144. + 1001.*eta/12. + (dm/m2M) * (-809./84. + 281.*eta/8.);
        params.ESO25s1 = 11. - 61.*eta/9. + (dm/m1M) * (-3. + 10.*eta/3.);
        params.ESO25s2 = 11. - 61.*eta/9. + (dm/m2M) * (3. - 10.*eta/3.);
    }
	
  /* The \dot\omega spin-related coefficients in the d.e. are shifted for modGR signals */

	params.wdotSO15s1 *= 1.0 + dxis[3];
	params.wdotSO15s2 *= 1.0 + dxis[3];
	params.wdotSS2 *= 1.0 + dxis[4];
	params.wdotSSselfS1 *= 1.0 + dxis[4];
	params.wdotSSselfS1L *= 1.0 + dxis[4];
	params.wdotSSselfS2 *= 1.0 + dxis[4];
	params.wdotSSselfS2L *= 1.0 + dxis[4];
	params.wdotSO25s1 *= 1.0 + dxis[5];
	params.wdotSO25s2 *= 1.0 + dxis[5];
	
    /**
     * Compute the coefficients of tidal corrections 
     * to the evolution equations for omega and binary energy E.
     * Flags control which tidal corrections are included.
     * Coefficients found from Eqs. 2.11 and 3.10 of 
     * Vines, Flanagan, Hinderer, PRD 83, 084051 (2011).
     */
    params.wdottidal5pn = 0.;
    params.wdottidal6pn = 0.;
    params.Etidal5pn = 0.;
    params.Etidal6pn = 0.;
    if( interactionFlags >= LAL_SIM_INSPIRAL_INTERACTION_TIDAL_5PN)  /* HERE lambda_i HAS NO INTRINSIC (M/m_i)^5 FACTOR */
    {
        params.wdottidal5pn = lambda1 * 6. * (1. + 11. * m2M) / m1M
                + lambda2 * 6. * (1. + 11. * m1M) / m2M;
        params.Etidal5pn = - 9. * m2m1 * lambda1 - 9. * m1m2 * lambda2;
    }
    if( interactionFlags >= LAL_SIM_INSPIRAL_INTERACTION_TIDAL_6PN )
    {
        params.wdottidal6pn = lambda1 * (4421./28. - 12263./28. * m1M 
                + 1893./2. * m1M * m1M - 661 * m1M * m1M * m1M) / (2 * m1M)
                + lambda2 * (4421./28. - 12263./28. * m2M + 1893./2. * m2M * m2M
                - 661 * m2M * m2M * m2M) / (2 * m2M);
        params.Etidal6pn = - 11./2. * m2m1 
                * (3. + 2. * m1M + 3. * m1M * m1M) * lambda1 
                - 11./2. * m1m2 * (3. + 2. * m2M + 3. * m2M * m2M) * lambda2;
    }
	   
    /* Estimate length of waveform using Newtonian t(f) formula */
    /* Time from freq. = fStart to infinity */
    dtStart = (5.0/256.0) * pow(LAL_PI,-8.0/3.0) 
            * pow(Mchirp * fStart,-5.0/3.0) / fStart;
    /* Time from freq. = fEnd to infinity. Set to zero if fEnd=0 */
    dtEnd = (fEnd == 0. ? 0. : (5.0/256.0) * pow(LAL_PI,-8.0/3.0) 
            * pow(Mchirp * fEnd,-5.0/3.0) / fEnd);
    /* Time in sec from fStart to fEnd. Note it can be positive or negative */
    lengths = dtStart - dtEnd;

    /* Put initial values into a single array for the integrator */
    yinit[0] = 0.; /* without loss of generality, set initial orbital phase=0 */
    yinit[1] = LAL_PI * M * fStart;  /* \hat{omega} = (pi M f) */
    /* LNh(x,y,z) */
    yinit[2] = lnhatx;
    yinit[3] = lnhaty;
    yinit[4] = lnhatz;
    /* S1(x,y,z) */
    norm = m1 * m1 / M / M;
    yinit[5] = norm * s1x;
    yinit[6] = norm * s1y;
    yinit[7] = norm * s1z;
    /* S2(x,y,z) */
    norm = m2 * m2 / M / M;
    yinit[8] = norm * s2x;
    yinit[9] = norm * s2y;
    yinit[10]= norm * s2z;
    /* E1(x,y,z) */
    yinit[11] = e1x;
    yinit[12] = e1y;
    yinit[13] = e1z;

    /* initialize the integrator */
    integrator = XLALAdaptiveRungeKutta4Init(LAL_NUM_ST4_VARIABLES,
            XLALSimInspiralSpinTaylorT4Derivatives,
            XLALSimInspiralSpinTaylorT4StoppingTest,
            LAL_ST4_ABSOLUTE_TOLERANCE, LAL_ST4_RELATIVE_TOLERANCE);
    if( !integrator )
    {
        XLALPrintError("XLAL Error - %s: Cannot allocate integrator\n", 
                __func__);
        XLAL_ERROR(func, XLAL_EFUNC);
    }

    /* stop the integration only when the test is true */
    integrator->stopontestonly = 1;

    /* run the integration; note: time is measured in \hat{t} = t / M */
    len = XLALAdaptiveRungeKutta4Hermite(integrator, (void *) &params, yinit,
            0.0, lengths/M, sgn*deltaT/M, &yout);

    intreturn = integrator->returncode;
    XLALAdaptiveRungeKutta4Free(integrator);

    if (!len) 
    {
        XLALPrintError("XLAL Error - %s: integration failed with errorcode %d.\n", __func__, intreturn);
        XLAL_ERROR(func, XLAL_EFUNC);
    }

    /* Print warning about abnormal termination */
    if (intreturn != 0 && intreturn != LALSIMINSPIRAL_ST4_TEST_ENERGY 
            && intreturn != LALSIMINSPIRAL_ST4_TEST_OMEGADOT 
            && intreturn != LALSIMINSPIRAL_ST4_TEST_FREQBOUND)
    {
        XLALPrintWarning("XLAL Warning - %s: integration terminated with code %d.\n Waveform parameters were m1 = %e, m2 = %e, s1 = (%e,%e,%e), s2 = (%e,%e,%e), inc = %e.\n", __func__, intreturn, m1 * pow(LAL_C_SI, 3.0) / LAL_G_SI / LAL_MSUN_SI, m2 * pow(LAL_C_SI, 3.0) / LAL_G_SI / LAL_MSUN_SI, s1x, s1y, s1z, s2x, s2y, s2z, acos(lnhatz));
    }

    /* 
     * If ending frequency was non-zero, we may have overshot somewhat.
     * The integrator takes one adaptive stride past fEnd, 
     * but this may include several smaller interpolation steps.
     * Therefore, 'cutlen' will be the index of the first interpolated step
     * to cross fEnd and 'len' is the full length returned from the integrator.
     * If fEnd == 0, we integrated as far as possible and 'cutlen' = 'len'.
     */
    cutlen = len;
    if( fEnd != 0. && fEnd < fStart )
    {
        wEnd = LAL_PI * M * fEnd; /* Ending dimensionless freq. \hat{omega} */
        /* Integrator returns \hat{omega} in array 'yout'
           in range data[2*len] to data[2*len+(len-1)]. 
           Start at end and find where we cross wEnd */
        while( yout->data[2*len+cutlen-1] < wEnd )
            cutlen--;
        if( cutlen < len )
            cutlen++; /* while loop exits on wrong side of fEnd, so increment */
    }
    else if( fEnd > fStart )
    {
        wEnd = LAL_PI * M * fEnd; /* Ending dimensionless freq. \hat{omega} */
        /* Integrator returns \hat{omega} in array 'yout'
           in range data[2*len] to data[2*len+(len-1)]. 
           Start at end and find where we cross wEnd */
        while( yout->data[2*len+cutlen-1] > wEnd )
            cutlen--;
        if( cutlen < len )
            cutlen++; /* while loop exits on wrong side of fEnd, so increment */
    }

    /* Adjust tStart so last sample is at time=0 */
    XLALGPSAdd(&tStart, -1.0*(cutlen-1)*deltaT);

    /* allocate memory for output vectors */
    *V = XLALCreateREAL8TimeSeries( "PN_EXPANSION_PARAMETER", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *Phi = XLALCreateREAL8TimeSeries( "ORBITAL_PHASE", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S1x = XLALCreateREAL8TimeSeries( "SPIN1_X_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S1y = XLALCreateREAL8TimeSeries( "SPIN1_Y_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S1z = XLALCreateREAL8TimeSeries( "SPIN1_Z_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S2x = XLALCreateREAL8TimeSeries( "SPIN2_X_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S2y = XLALCreateREAL8TimeSeries( "SPIN2_Y_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *S2z = XLALCreateREAL8TimeSeries( "SPIN2_Z_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *LNhatx = XLALCreateREAL8TimeSeries( "LNHAT_X_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *LNhaty = XLALCreateREAL8TimeSeries( "LNHAT_Y_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *LNhatz = XLALCreateREAL8TimeSeries( "LNHAT_Z_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *E1x = XLALCreateREAL8TimeSeries( "E1_BASIS_X_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *E1y = XLALCreateREAL8TimeSeries( "E1_BASIS_Y_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    *E1z = XLALCreateREAL8TimeSeries( "E1_BASIS_Z_COMPONENT", &tStart, 0., 
            deltaT, &lalDimensionlessUnit, cutlen); 
    if ( !V || !Phi || !S1x || !S1y || !S1z || !S2x || !S2y || !S2z 
            || !LNhatx || !LNhaty || !LNhatz || !E1x || !E1y || !E1z )
    {
        XLALDestroyREAL8Array(yout);
        XLAL_ERROR(func, XLAL_EFUNC);
    }

    /* If we integrated backwards, offset & sgn will reverse order of samples */
    if( fEnd < fStart && fEnd != 0. )
        offset = cutlen-1;
    else
        offset = 0;

    /* Copy dynamical variables from yout array to output time series.
     * Note the first 'len' members of yout are the time steps. 
     * Also, the for loop only goes to 'cutlen', in case we overshot fEnd.
     * If we integrated backwards, we copy backwards from 'cutlen'.
     */
    for( i = 0; i < cutlen; i++ )
    {	
        int j = sgn*i+offset;
        (*Phi)->data->data[j] 		= yout->data[len+i];
        (*V)->data->data[j] 		= cbrt(yout->data[2*len+i]);
        (*LNhatx)->data->data[j] 	= yout->data[3*len+i];
        (*LNhaty)->data->data[j] 	= yout->data[4*len+i];
        (*LNhatz)->data->data[j] 	= yout->data[5*len+i];
        (*S1x)->data->data[j] 		= yout->data[6*len+i];
        (*S1y)->data->data[j] 		= yout->data[7*len+i];
        (*S1z)->data->data[j] 		= yout->data[8*len+i];
        (*S2x)->data->data[j] 		= yout->data[9*len+i];
        (*S2y)->data->data[j] 		= yout->data[10*len+i];
        (*S2z)->data->data[j] 		= yout->data[11*len+i];
        (*E1x)->data->data[j] 		= yout->data[12*len+i];
        (*E1y)->data->data[j] 		= yout->data[13*len+i];
        (*E1z)->data->data[j] 		= yout->data[14*len+i];
    }

    XLALDestroyREAL8Array(yout);

    return XLAL_SUCCESS;
}

/**
 * Internal function called by the integration routine.
 * Stops the integration if
 * 		1) The energy decreases with increasing orbital frequency
 *		2) The orbital frequency begins decreasing
 *		3) The orbital frequency becomes infinite 
 *		4) The orbital frequency has gone outside the requested bounds
 */
static int XLALSimInspiralSpinTaylorT4StoppingTest(
	double t, 
	const double values[],
	double dvalues[], 
	void *mparams
	)
{
//	static const char *func = "XLALSimInspiralSpinTaylorT4StoppingTest";
    REAL8 omega, v, test, omegaStart, omegaEnd;
    XLALSimInspiralSpinTaylorT4Coeffs *params 
            = (XLALSimInspiralSpinTaylorT4Coeffs*) mparams;
    /* Spin-corrections to energy (including dynamical terms) */
    REAL8 Espin15 = 0., Espin2 = 0., Espin25 = 0.;

    UNUSED(t);

    omega = values[1];
    v = pow(omega,1./3.);


    /* omega = PI G M f_GW / c^3
     * Note params->M is really G M /c^3 (i.e. M is in seconds) */
    omegaStart = LAL_PI * params->M * params->fStart;
    omegaEnd = LAL_PI * params->M * params->fEnd;

    if( params->ESO15s1 != 0. || params->ESO15s2 != 0. || params->ESS2 != 0. )
    { 	/* Some spin terms are non-zero, so compute spins dot L */ 
        REAL8 LNhx, LNhy, LNhz, S1x, S1y, S1z, S2x, S2y, S2z;
        REAL8 LNdotS1, LNdotS2;

        LNhx = values[2]; LNhy  = values[3]; LNhz = values[4] ;
        S1x  = values[5]; S1y   = values[6]; S1z  = values[7] ;
        S2x  = values[8]; S2y   = values[9]; S2z  = values[10];

        LNdotS1 = (LNhx*S1x + LNhy*S1y + LNhz*S1z);
        LNdotS2 = (LNhx*S2x + LNhy*S2y + LNhz*S2z);

        if( params->ESO15s1 != 0. || params->ESO15s2 != 0. )
        {   /* Compute 1.5PN SO correction to energy */
            Espin15 += params->ESO15s1 * LNdotS1 + params->ESO15s2 * LNdotS2;
        }

        if( params->ESS2 != 0. )
        {   /* Compute 2PN SS correction to energy */
            REAL8 S1dotS2;
            S1dotS2 = (S1x*S2x + S1y*S2y + S1z*S2z);
            Espin2 += params->ESS2  * (S1dotS2 - 3. * LNdotS1 * LNdotS2);
        }

        if( params->EQM2S1 != 0. )
        {   /* Compute 2PN quadrupole-monopole correction to energy */
            // See last line of Eq. 6 of astro-ph/0504538
            // or 2nd and 3rd lines of Eq. (C4) in arXiv:0810.5336v3
            REAL8 S1sq = (S1x*S1x + S1y*S1y + S1z*S1z);
            REAL8 S2sq = (S2x*S2x + S2y*S2y + S2z*S2z);
            Espin2 += params->EQM2S1 * S1sq
                    + params->EQM2S2 * S2sq
                    + params->EQM2S1L * LNdotS1 * LNdotS1
                    + params->EQM2S2L * LNdotS2 * LNdotS2;
        }

        if( params->ESO25s1 != 0. || params->wdotSO25s2 != 0. )
        {   /* Compute 2.5PN SO correction to energy */
            // See Eq. 7.9 of gr-qc/0605140v4
            // Note that S_l/M^2 = (m1/M)^2 chi1 + (m2/M)^2 chi2
            // and Sigma_l/M^2 = (m2/M) chi2 - (m1/M) chi1
            Espin25 += params->ESO25s1 * LNdotS1 + params->ESO25s2 * LNdotS2;
        }
    }

    /**
     * We are testing if the orbital energy increases with \f$\omega\f$. 
     * We should be losing energy to GW flux, so if E increases 
     * we stop integration because the dynamics are becoming unphysical. 
     * 'test' is the PN expansion of \f$dE/d\omega\f$ without the prefactor, 
     * i.e. \f$dE/d\omega = dE/dv * dv/d\omega = - (M^2*eta/6) * test\f$
     * Therefore, the energy is increasing with \f$\omega\f$ iff. test < 0.
     */
    test = 2. + v * v * ( 4. * params->Ecoeff[2] 
            + v * ( 5. * (params->Ecoeff[3] + Espin15) 
            + v * ( 6. * (params->Ecoeff[4] + Espin2)
            + v * ( 7. * (params->Ecoeff[5] + Espin25)
            + v * ( 8. *  params->Ecoeff[6]
            + v * ( 9. *  params->Ecoeff[7]
			+ v * v * v * ( 12. * params->Etidal5pn
			+ v * v * ( 14. * params->Etidal6pn ) ) ) ) ) ) ) );
    if( omegaEnd != 0. && omegaEnd > omegaStart && omega > omegaEnd) /* freq. above bound */
        return LALSIMINSPIRAL_ST4_TEST_FREQBOUND;
    else if( omegaEnd != 0. && omegaEnd < omegaStart && omega < omegaEnd) /* freq. below bound */
        return LALSIMINSPIRAL_ST4_TEST_FREQBOUND;
    else if (test < 0.0) /* energy test fails! */
        return LALSIMINSPIRAL_ST4_TEST_ENERGY;
    else if (dvalues[1] < 0.0) /* omegadot < 0! */
        return LALSIMINSPIRAL_ST4_TEST_OMEGADOT;
    else if isnan(omega) /* omega is nan! */
        return LALSIMINSPIRAL_ST4_TEST_OMEGANAN;
    else /* Step successful, continue integrating */
        return GSL_SUCCESS;
}

/**
 * Internal function called by the integration routine.
 * Given the values of all the dynamical variables 'values' at time 't',
 * This function computes their derivatives 'dvalues'
 * so the ODE integrator can take a step
 * All non-dynamical quantities (masses, etc.) are passed in \"mparams\"
 *
 * The derivatives for \f$\omega\f$, L_N, S1, S2 can be found 
 * as Eqs. (1), (8) - (10) of gr-qc/0405090
 * The derivative of E1 is Eq. (15)-(16) of gr-qc/0310034
 */
static int XLALSimInspiralSpinTaylorT4Derivatives(
	double t, 
	const double values[],
	double dvalues[], 
	void *mparams
	) 
{
//	static const char *func = "XLALSimInspiralSpinTaylorT4Derivatives";
    /* coordinates and derivatives */
    REAL8 LNhx, LNhy, LNhz, S1x, S1y, S1z, S2x, S2y, S2z, E1x, E1y, E1z;
    REAL8 omega, ds, domega, dLNhx, dLNhy, dLNhz;
    REAL8 dS1x, dS1y, dS1z, dS2x, dS2y, dS2z, dE1x, dE1y, dE1z;

    /* auxiliary variables */
    REAL8 v, v2, v3, v4, v5, v7, v11, omega2, omega2by2;
    REAL8 LNdotS1, LNdotS2, threeLNdotS1, threeLNdotS2, S1dotS2;
    REAL8 v5etaLNhatSO15s1, v5etaLNhatSO15s2;
    REAL8 OmegaLx, OmegaLy, OmegaLz, OmegaLdotLN;
    REAL8 OmegaEx, OmegaEy, OmegaEz, OmegaSx, OmegaSy, OmegaSz;
    REAL8 wspin15 = 0., wspin2 = 0., wspin25 = 0.;

    XLALSimInspiralSpinTaylorT4Coeffs *params 
            = (XLALSimInspiralSpinTaylorT4Coeffs*) mparams;

    UNUSED(t);

    /* copy variables */
    // UNUSED!!: s    = values[0] ;
    omega   	= values[1] ;
    LNhx = values[2] ; LNhy    	= values[3] ; LNhz 	= values[4] ;
    S1x  = values[5] ; S1y     	= values[6] ; S1z 	= values[7] ;
    S2x  = values[8] ; S2y     	= values[9] ; S2z 	= values[10];
    E1x  = values[11]; E1y    	= values[12]; E1z 	= values[13];

    if (omega <= 0.0) /* orbital frequency must be positive! */
    {
        return LALSIMINSPIRAL_ST4_DERIVATIVE_OMEGANONPOS;
    }

    v = cbrt(omega);
    v2  = v * v; v3 = v2 * v; v4 = v3 * v; 
    v5 = v * v4; v7 = v4 * v3; v11 = v7 * v4;

    LNdotS1 = (LNhx*S1x + LNhy*S1y + LNhz*S1z);
    LNdotS2 = (LNhx*S2x + LNhy*S2y + LNhz*S2z);
    S1dotS2 = (S1x*S2x  + S1y*S2y  + S1z*S2z );

    /** 
     * domega
     * 
     * Note we are actually computing \f$d \hat{\omega} / d \hat{t}\f$
     * where \f$\hat{\omega} = M \omega\f$ and \f$\hat{t} = t / M\f$
     * Therefore \f$domega = M^2 * d\omega / dt\f$
     *
     * See Eqs. (1)-(7) of gr-qc/0405090 But note that our spin variables 
     * are scaled by component masses relative to that paper.
     * i.e. \f$S_i = (m_i/M)^2 * \hat{S_i}\f$
     *
     * non-spinning coefficients of \f$\dot{\omega}\f$ (params->wdotcoeff[i])
     * should have been set before this function was called
     */
    if( params->wdotSO15s1 != 0. || params->wdotSO15s2 != 0. )
    {	/* Compute 1.5PN SO correction to omega derivative */
        wspin15 = params->wdotSO15s1 * LNdotS1 + params->wdotSO15s2 * LNdotS2;
    }
    if( params->wdotSS2 != 0. )
    {	/* Compute 2PN SS correction to omega derivative */
        wspin2 = params->wdotSS2 * (247. * S1dotS2 - 721. * LNdotS1 * LNdotS2);
    }
    if( params->wdotQM2S1 != 0. || params->wdotSSselfS1 != 0.)
    {	/* Compute 2PN QM and self-SS corrections to omega derivative */
        // This is equivalent to Eqs. 9c + 9d of astro-ph/0504538
        REAL8 S1sq = (S1x*S1x + S1y*S1y + S1z*S1z);
        REAL8 S2sq = (S2x*S2x + S2y*S2y + S2z*S2z);
        wspin2 += params->wdotQM2S1 * S1sq
                + params->wdotQM2S2 * S2sq
                + params->wdotQM2S1L * LNdotS1 * LNdotS1
                + params->wdotQM2S2L * LNdotS2 * LNdotS2
                + params->wdotSSselfS1 * S1sq
                + params->wdotSSselfS2 * S2sq
                + params->wdotSSselfS1L * LNdotS1 * LNdotS1
                + params->wdotSSselfS2L * LNdotS2 * LNdotS2;
    }
    if( params->wdotSO25s1 != 0. || params->wdotSO25s2 != 0. )
    {	/* Compute 2.5PN SO correction to omega derivative */
        // See Eq. 8.3 of gr-qc/0605140v4
        // Note that S_l/M^2 = (m1/M)^2 chi1 + (m2/M)^2 chi2
        // and Sigma_l/M^2 = (m2/M) chi2 - (m1/M) chi1
        wspin25 = params->wdotSO25s1 * LNdotS1 + params->wdotSO25s2 * LNdotS2;
    }

    domega  = params->wdotnewt * v11 * ( params->wdotcoeff[0] 
            + v * ( params->wdotcoeff[1] 
            + v * ( params->wdotcoeff[2]
            + v * ( params->wdotcoeff[3] + wspin15 
            + v * ( params->wdotcoeff[4] + wspin2 
            + v * ( params->wdotcoeff[5] + wspin25 
            + v * ( params->wdotcoeff[6] + params->wdotlogcoeff * log(omega)
            + v * ( params->wdotcoeff[7] 
            + v3 * ( params->wdottidal5pn
            + v2 * ( params->wdottidal6pn ) ) ) ) ) ) ) ) ) );

    /**
     * dLN
     * 
     * \f$d \hat{L_N}/d \hat{t} = M * d\hat{L_N} / dt = \Omega_L x \hat{L_N}\f$
     * This is Eq. (10) of gr-qc/0405090 ( times M b/c we use \f$\hat{t}\f$)
     */
    omega2 = omega * omega;
    /* \Omega_L vector */
    OmegaLx = omega2 * (params->LNhatSO15s1 * S1x + params->LNhatSO15s2 * S2x)
            + v7 * params->LNhatSS2 * (LNdotS2 * S1x + LNdotS1 * S2x);
    OmegaLy = omega2 * (params->LNhatSO15s1 * S1y + params->LNhatSO15s2 * S2y)
            + v7 * params->LNhatSS2 * (LNdotS2 * S1y + LNdotS1 * S2y);
    OmegaLz = omega2 * (params->LNhatSO15s1 * S1z + params->LNhatSO15s2 * S2z)
            + v7 * params->LNhatSS2 * (LNdotS2 * S1z + LNdotS1 * S2z);

    /* Take cross product of \Omega_L with \hat{L_N} */
    dLNhx = (-OmegaLz*LNhy + OmegaLy*LNhz);
    dLNhy = (-OmegaLx*LNhz + OmegaLz*LNhx);
    dLNhz = (-OmegaLy*LNhx + OmegaLx*LNhy);

    /**
     * dE1
     * 
     * d E_1 / d \hat{t} = M * d E_1 / dt
     * Computed from \Omega_L and \hat{L_N} with Eq. (15)-(16) of gr-qc/0310034
     */
    OmegaLdotLN = OmegaLx * LNhx + OmegaLy * LNhy + OmegaLz * LNhz;
    /* \Omega_E vector */
    OmegaEx = OmegaLx - OmegaLdotLN * LNhx;
    OmegaEy = OmegaLy - OmegaLdotLN * LNhy;
    OmegaEz = OmegaLz - OmegaLdotLN * LNhz;

    /* Take cross product of \Omega_E with E_1 */
    dE1x = (-OmegaEz*E1y + OmegaEy*E1z);
    dE1y = (-OmegaEx*E1z + OmegaEz*E1x);
    dE1z = (-OmegaEy*E1x + OmegaEx*E1y);

    /**
     * dS1
     * 
     * d S_1 / d \hat{t} = M * d S_1 / dt = \Omega_{S1} x S_1
     * This is Eq. (8) of gr-qc/0405090.
     * However, that paper uses spin variables which are M^2 times our spins
     */
    /* \Omega_{S1} vector */
    omega2by2 = omega2 * 0.5;
    threeLNdotS2 = 3. * LNdotS2;
    v5etaLNhatSO15s1 = v5 * params->eta * params->LNhatSO15s1;
    OmegaSx = v5etaLNhatSO15s1 * LNhx
            + omega2by2 * (S2x - threeLNdotS2 * LNhx);
    OmegaSy = v5etaLNhatSO15s1 * LNhy
            + omega2by2 * (S2y - threeLNdotS2 * LNhy);
    OmegaSz = v5etaLNhatSO15s1 * LNhz
            + omega2by2 * (S2z - threeLNdotS2 * LNhz);

    /* Take cross product of \Omega_{S1} with S_1 */
    dS1x = (-OmegaSz*S1y + OmegaSy*S1z);
    dS1y = (-OmegaSx*S1z + OmegaSz*S1x);
    dS1z = (-OmegaSy*S1x + OmegaSx*S1y);

    /**
     * dS2
     * 
     * d S_2 / d \hat{t} = M * d S_2 / dt = \Omega_{S2} x S_2
     * This is Eq. (9) of gr-qc/0405090.
     * However, that paper uses spin variables which are M^2 times our spins
     */
    /* \Omega_{S2} vector */
    threeLNdotS1 = 3. * LNdotS1;
    v5etaLNhatSO15s2 = v5 * params->eta * params->LNhatSO15s2;
    OmegaSx = v5etaLNhatSO15s2 * LNhx
            + omega2by2 * (S1x - threeLNdotS1 * LNhx);
    OmegaSy = v5etaLNhatSO15s2 * LNhy
            + omega2by2 * (S1y - threeLNdotS1 * LNhy);
    OmegaSz = v5etaLNhatSO15s2 * LNhz
            + omega2by2 * (S1z - threeLNdotS1 * LNhz);

    /* Take cross product of \Omega_{S2} with S_2 */
    dS2x = (-OmegaSz*S2y + OmegaSy*S2z);
    dS2y = (-OmegaSx*S2z + OmegaSz*S2x);
    dS2z = (-OmegaSy*S2x + OmegaSx*S2y);

    /* dphi = d \phi / d \hat{t} = M d \phi /dt = M \omega = \hat{\omega} */
    ds = omega;

    dvalues[0]    = ds   ; dvalues[1]     = domega;
    dvalues[2]    = dLNhx; dvalues[3]     = dLNhy ; dvalues[4]    = dLNhz;
    dvalues[5]    = dS1x ; dvalues[6]     = dS1y  ; dvalues[7]    = dS1z ;
    dvalues[8]    = dS2x ; dvalues[9]     = dS2y  ; dvalues[10]   = dS2z ;
    dvalues[11]   = dE1x ; dvalues[12]    = dE1y  ; dvalues[13]   = dE1z ;

    return GSL_SUCCESS;
}

/* Appends the start and end time series together, skipping the redundant first
 * sample of end.  Frees end before returning a pointer to the result, which is
 * the resized start series.  */
static REAL8TimeSeries *appendTSandFree(REAL8TimeSeries *start, 
        REAL8TimeSeries *end) {
    unsigned int origlen = start->data->length;
    start = XLALResizeREAL8TimeSeries(start, 0, 
            start->data->length + end->data->length - 1);
    
    memcpy(start->data->data + origlen, end->data->data+1, 
            (end->data->length-1)*sizeof(REAL8));

    XLALGPSAdd(&(start->epoch), -end->deltaT*(end->data->length - 1));

    XLALDestroyREAL8TimeSeries(end);

    return start;        
}


/**
 * Driver routine to compute a precessing post-Newtonian inspiral waveform
 * with phasing computed from energy balance using the so-called \"T4\" method.
 *
 * This routine allows the user to specify different pN orders
 * for the phasing and amplitude of the waveform.
 * 
 * The reference frequency fRef is used as follows:
 * 1) if fRef = 0: The initial values of s1, s2, lnhat and e1 will be the
 *    values at frequency fStart. The orbital phase of the last sample is set
 *    to phiRef (i.e. phiRef is the "coalescence phase", roughly speaking).
 *    THIS IS THE DEFAULT BEHAVIOR CONSISTENT WITH OTHER APPROXIMANTS
 * 
 * 2) If fRef = fStart: The initial values of s1, s2, lnhat and e1 will be the 
 *    values at frequency fStart. phiRef is used to set the orbital phase
 *    of the first sample at fStart.
 * 
 * 3) If fRef > fStart: The initial values of s1, s2, lnhat and e1 will be the
 *    values at frequency fRef. phiRef is used to set the orbital phase at fRef.
 *    The code will integrate forwards and backwards from fRef and stitch the
 *    two together to create a complete waveform. This allows one to specify
 *    the orientation of the binary in-band (or at any arbitrary point).
 *    Otherwise, the user can only directly control the initial orientation.
 *
 * 4) fRef < 0 or fRef >= Schwarz. ISCO are forbidden and the code will abort.
 */
int XLALSimInspiralSpinTaylorT4(
	REAL8TimeSeries **hplus,        /**< +-polarization waveform */
	REAL8TimeSeries **hcross,       /**< x-polarization waveform */
	REAL8 phiRef,                   /**< orbital phase at reference pt. */
	REAL8 v0,                       /**< tail gauge term (default = 1) */
	REAL8 deltaT,                   /**< sampling interval (s) */
	REAL8 m1,                       /**< mass of companion 1 (kg) */
	REAL8 m2,                       /**< mass of companion 2 (kg) */
	REAL8 fStart,                   /**< start GW frequency (Hz) */
	REAL8 fRef,                     /**< reference GW frequency (Hz) */
	REAL8 r,                        /**< distance of source (m) */
	REAL8 s1x,                      /**< initial value of S1x */
	REAL8 s1y,                      /**< initial value of S1y */
	REAL8 s1z,                      /**< initial value of S1z */
	REAL8 s2x,                      /**< initial value of S2x */
	REAL8 s2y,                      /**< initial value of S2y */
	REAL8 s2z,                      /**< initial value of S2z */
	REAL8 lnhatx,                   /**< initial value of LNhatx */
	REAL8 lnhaty,                   /**< initial value of LNhaty */
	REAL8 lnhatz,                   /**< initial value of LNhatz */
	REAL8 e1x,                      /**< initial value of E1x */
	REAL8 e1y,                      /**< initial value of E1y */
	REAL8 e1z,                      /**< initial value of E1z */
	REAL8 lambda1,                  /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
	REAL8 lambda2,                  /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
	REAL8 qmparam1,               /**< quadrupole-monopole parameter of mass 1 **/
	REAL8 qmparam2,               /**< quadrupole-monopole parameter of mass 2 **/
	LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
	int phaseO,                     /**< twice PN phase order */
	int amplitudeO,                  /**< twice PN amplitude order */
    REAL8 *dxis                   /**< testing GR parameters */
	)
{
	static const char *func = "XLALSimInspiralSpinTaylorT4";
    REAL8TimeSeries *V, *Phi, *S1x, *S1y, *S1z, *S2x, *S2y, *S2z;
    REAL8TimeSeries *LNhatx, *LNhaty, *LNhatz, *E1x, *E1y, *E1z;
    int status, n;
    unsigned int i;
    REAL8 fS, fE, phiShift;
    /* The Schwarzschild ISCO frequency - for sanity checking fRef */
    REAL8 fISCO = pow(LAL_C_SI,3) / (pow(6.,3./2.)*LAL_PI*(m1+m2)*LAL_G_SI);

    /* Sanity check fRef value */
    if( fRef < 0. )
    {
        XLALPrintError("XLAL Error - %s: fRef = %f must be >= 0\n", 
                __func__, fRef);
        XLAL_ERROR(func, XLAL_EINVAL);
    }
    if( fRef != 0. && fRef < fStart )
    {
        XLALPrintError("XLAL Error - %s: fRef = %f must be > fStart = %f\n", 
                __func__, fRef, fStart);
        XLAL_ERROR(func, XLAL_EINVAL);
    }
    if( fRef >= fISCO )
    {
        XLALPrintError("XLAL Error - %s: fRef = %f must be < Schwar. ISCO=%f\n",
                __func__, fRef, fISCO);
        XLAL_ERROR(func, XLAL_EINVAL);
    }

    /* if fRef=0, just integrate from start to end. Let phiRef=phiC */
    if( fRef == 0. )
    {
        fS = fStart;
        fE = 0.;
        /* Evolve the dynamical variables */
        n = XLALSimInspiralPNEvolveOrbitSpinTaylorT4(&V, &Phi, 
                &S1x, &S1y, &S1z, &S2x, &S2y, &S2z, 
                &LNhatx, &LNhaty, &LNhatz, &E1x, &E1y, &E1z, 
                deltaT, m1, m2, fS, fE, s1x, s1y, s1z, s2x, s2y, s2z, 
                lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, 
                lambda1, lambda2, qmparam1,qmparam2, interactionFlags, 
                phaseO, dxis);
        if( n < 0 )
            XLAL_ERROR(func, XLAL_EFUNC);

        /* Apply phase shift so orbital phase ends with desired value */
        phiShift = phiRef - Phi->data->data[Phi->data->length-1];
        for( i=0; i < Phi->data->length; i++)
        {
            Phi->data->data[i] += phiShift;
        }
    }
    /* if fRef=fStart, just integrate from start to end. Let phiRef=phiStart */
    else if( fRef == fStart )
    {
        fS = fStart;
        fE = 0.;
        /* Evolve the dynamical variables */
        n = XLALSimInspiralPNEvolveOrbitSpinTaylorT4(&V, &Phi, 
                &S1x, &S1y, &S1z, &S2x, &S2y, &S2z, 
                &LNhatx, &LNhaty, &LNhatz, &E1x, &E1y, &E1z, 
                deltaT, m1, m2, fS, fE, s1x, s1y, s1z, s2x, s2y, s2z, 
                lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, 
                lambda1, lambda2, qmparam1, qmparam2, interactionFlags, 
                phaseO, dxis);
        if( n < 0 )
            XLAL_ERROR(func, XLAL_EFUNC);

        /* Apply phase shift so orbital phase starts with desired value */
        phiShift = phiRef - Phi->data->data[0];
        for( i=0; i < Phi->data->length; i++)
        {
            Phi->data->data[i] += phiShift;
        }
    }
    else /* Start in middle, integrate backward and forward, stitch together */
    {
        REAL8TimeSeries *V1=NULL, *Phi1=NULL, *S1x1=NULL, *S1y1=NULL, *S1z1=NULL, *S2x1=NULL, *S2y1=NULL, *S2z1=NULL;
        REAL8TimeSeries *LNhatx1=NULL, *LNhaty1=NULL, *LNhatz1=NULL, *E1x1=NULL, *E1y1=NULL, *E1z1=NULL;
        REAL8TimeSeries *V2=NULL, *Phi2=NULL, *S1x2=NULL, *S1y2=NULL, *S1z2=NULL, *S2x2=NULL, *S2y2=NULL, *S2z2=NULL;
        REAL8TimeSeries *LNhatx2=NULL, *LNhaty2=NULL, *LNhatz2=NULL, *E1x2=NULL, *E1y2=NULL, *E1z2=NULL;

        /* Integrate backward to fStart */
        fS = fRef;
        fE = fStart;
        n = XLALSimInspiralPNEvolveOrbitSpinTaylorT4(&V1, &Phi1, 
                &S1x1, &S1y1, &S1z1, &S2x1, &S2y1, &S2z1, 
                &LNhatx1, &LNhaty1, &LNhatz1, &E1x1, &E1y1, &E1z1,
                deltaT, m1, m2, fS, fE, s1x, s1y, s1z, s2x, s2y,
                s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, 
                lambda1, lambda2, qmparam1, qmparam2, interactionFlags, 
                phaseO, dxis);
        
        /* Apply phase shift so orbital phase has desired value at fRef */
        phiShift = phiRef - Phi1->data->data[Phi1->data->length-1];
        for( i=0; i < Phi1->data->length; i++)
        {
            Phi1->data->data[i] += phiShift;
        }

        /* Integrate forward to end of waveform */
        fS = fRef;
        fE = 0.;
        n = XLALSimInspiralPNEvolveOrbitSpinTaylorT4(&V2, &Phi2, 
                &S1x2, &S1y2, &S1z2, &S2x2, &S2y2, &S2z2, 
                &LNhatx2, &LNhaty2, &LNhatz2, &E1x2, &E1y2, &E1z2,
                deltaT, m1, m2, fS, fE, s1x, s1y, s1z, s2x, s2y,
                s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, 
                lambda1, lambda2, qmparam1, qmparam2, interactionFlags, 
                phaseO, dxis);
        
        /* Apply phase shift so orbital phase has desired value at fRef */
        phiShift = phiRef - Phi2->data->data[0];
        for( i=0; i < Phi2->data->length; i++)
        {
            Phi2->data->data[i] += phiShift;
        }

        /* Stitch 2nd set of vectors onto 1st set. Free 2nd set. */
        V = appendTSandFree(V1, V2); 
        Phi = appendTSandFree(Phi1, Phi2);
        S1x = appendTSandFree(S1x1, S1x2);
        S1y = appendTSandFree(S1y1, S1y2);
        S1z = appendTSandFree(S1z1, S1z2);
        S2x = appendTSandFree(S2x1, S2x2);
        S2y = appendTSandFree(S2y1, S2y2);
        S2z = appendTSandFree(S2z1, S2z2);
        LNhatx = appendTSandFree(LNhatx1, LNhatx2);
        LNhaty = appendTSandFree(LNhaty1, LNhaty2);
        LNhatz = appendTSandFree(LNhatz1, LNhatz2);
        E1x = appendTSandFree(E1x1, E1x2);
        E1y = appendTSandFree(E1y1, E1y2);
        E1z = appendTSandFree(E1z1, E1z2);
    }


    /* Use the dynamical variables to build the polarizations */
    status = XLALSimInspiralPrecessingPolarizationWaveforms(hplus, hcross,
            V, Phi, S1x, S1y, S1z, S2x, S2y, S2z, LNhatx, LNhaty, LNhatz, 
            E1x, E1y, E1z, m1, m2, r, v0, amplitudeO);

    /* Destroy vectors of dynamical variables, check for errors then exit */
    XLALDestroyREAL8TimeSeries(V);
    XLALDestroyREAL8TimeSeries(Phi);
    XLALDestroyREAL8TimeSeries(S1x);
    XLALDestroyREAL8TimeSeries(S1y);
    XLALDestroyREAL8TimeSeries(S1z);
    XLALDestroyREAL8TimeSeries(S2x);
    XLALDestroyREAL8TimeSeries(S2y);
    XLALDestroyREAL8TimeSeries(S2z);
    XLALDestroyREAL8TimeSeries(LNhatx);
    XLALDestroyREAL8TimeSeries(LNhaty);
    XLALDestroyREAL8TimeSeries(LNhatz);
    XLALDestroyREAL8TimeSeries(E1x);
    XLALDestroyREAL8TimeSeries(E1y);
    XLALDestroyREAL8TimeSeries(E1z);
    if( status < 0 )
        XLAL_ERROR(func, XLAL_EFUNC);

    return n;
}

/**
 * Driver routine to compute the physical template family "Q" vectors using
 * the \"T4\" method. Note that PTF describes single spin systems
 *
 * This routine requires leading-order amplitude dependence
 * but allows the user to specify the phase PN order
 */
int XLALSimInspiralSpinTaylorT4PTFQVecs(
        REAL8TimeSeries **Q1,            /**< Q1 output vector */
        REAL8TimeSeries **Q2,            /**< Q2 output vector */
        REAL8TimeSeries **Q3,            /**< Q3 output vector */
        REAL8TimeSeries **Q4,            /**< Q4 output vector */
        REAL8TimeSeries **Q5,            /**< Q5 output vector */
        REAL8 deltaT,                   /**< sampling interval (s) */
        REAL8 m1,                       /**< mass of companion 1 (kg) */
        REAL8 m2,                       /**< mass of companion 2 (kg) */
        REAL8 chi1,                     /**< spin magnitude (|S1|) */
        REAL8 kappa1,                    /**< L . S1 (1 if they are aligned) */
        REAL8 fStart,                   /**< start GW frequency (Hz) */
        REAL8 lambda1,                  /**< (tidal deformability of mass 1) / (total mass)^5 (dimensionless) */
        REAL8 lambda2,                  /**< (tidal deformability of mass 2) / (total mass)^5 (dimensionless) */
        REAL8 qmparam1,               /**< quadrupole-monopole parameter of mass 1 **/
        REAL8 qmparam2,               /**< quadrupole-monopole parameter of mass 2 **/
        LALSimInspiralInteraction interactionFlags, /**< flag to control spin and tidal effects */
        int phaseO,                      /**< twice PN phase order */
        REAL8 *dxis                   /**< testing GR parameters */
        )
{
	static const char *func = "XLALSimInspiralSpinTaylorT4PTFQVecs";
    /* To generate the QVecs we need to choose a specific frame 
     * This frame is set so that inclination, and most other extrinsic
     * angles are 0. This does not lead to loss in generality as PTF maximizes
     * over these angles. This follows the PBCV convention
     */
    REAL8 fRef = 0.;
    REAL8 r = 10E6 * LAL_PC_SI; /* Setting an arbitrary distance of 10 MPc */
    REAL8 s1x = chi1 * pow((1 - kappa1*kappa1),0.5);
    REAL8 s1z = chi1 * kappa1;
    REAL8 s1y,s2x,s2y,s2z,lnhatx,lnhaty,lnhatz,e1x,e1y,e1z;
    s1y = s2x = s2y = s2z = lnhatx = lnhaty = e1y = e1z = 0;     
    lnhatz = e1x = 1.;

    REAL8TimeSeries *V, *Phi, *S1x, *S1y, *S1z, *S2x, *S2y, *S2z;
    REAL8TimeSeries *LNhatx, *LNhaty, *LNhatz, *E1x, *E1y, *E1z;
    int status, n;

    /* Evolve the dynamical variables */
    n = XLALSimInspiralPNEvolveOrbitSpinTaylorT4(&V, &Phi, &S1x, &S1y, &S1z,
            &S2x, &S2y, &S2z, &LNhatx, &LNhaty, &LNhatz, &E1x, &E1y, &E1z,
            deltaT, m1, m2, fStart, fRef, s1x, s1y, s1z, s2x, s2y,
            s2z, lnhatx, lnhaty, lnhatz, e1x, e1y, e1z, 
            lambda1, lambda2, qmparam1, qmparam2, interactionFlags, 
            phaseO, dxis);
    if( n < 0 )
        XLAL_ERROR(func, XLAL_EFUNC);

    /* Use the dynamical variables to build the polarizations */
    status = XLALSimInspiralPrecessingPTFQWaveforms(Q1, Q2, Q3, Q4, Q5,
            V, Phi, S1x, S1y, S1z, S2x, S2y, S2z, LNhatx, LNhaty, LNhatz,
            E1x, E1y, E1z, m1, m2, r);

    /* Destroy vectors of dynamical variables, check for errors then exit */
    XLALDestroyREAL8TimeSeries(V);
    XLALDestroyREAL8TimeSeries(Phi);
    XLALDestroyREAL8TimeSeries(S1x);
    XLALDestroyREAL8TimeSeries(S1y);
    XLALDestroyREAL8TimeSeries(S1z);
    XLALDestroyREAL8TimeSeries(S2x);
    XLALDestroyREAL8TimeSeries(S2y);
    XLALDestroyREAL8TimeSeries(S2z);
    XLALDestroyREAL8TimeSeries(LNhatx);
    XLALDestroyREAL8TimeSeries(LNhaty);
    XLALDestroyREAL8TimeSeries(LNhatz);
    XLALDestroyREAL8TimeSeries(E1x);
    XLALDestroyREAL8TimeSeries(E1y);
    XLALDestroyREAL8TimeSeries(E1z);
    if( status < 0 )
        XLAL_ERROR(func, XLAL_EFUNC);

    return n;
}

/**
 * Computes polarizations h+ and hx for a spinning, precessing binary
 * when provided time series of all the dynamical quantities.
 * Amplitude can be chosen between 1.5PN and Newtonian orders (inclusive).
 *
 * Based on K.G. Arun, Alesssandra Buonanno, Guillaume Faye and Evan Ochsner
 * \"Higher-order spin effects in the amplitude and phase of gravitational
 * waveforms emitted by inspiraling compact binaries: Ready-to-use
 * gravitational waveforms\", Phys Rev. D 79, 104023 (2009), arXiv:0810.5336
 *
 * HOWEVER, the formulae have been adapted to use the output of the so-called
 * \"Frameless\" convention for evolving precessing binary dynamics,
 * which is not susceptible to hitting coordinate singularities.
 *
 * FIXME: Clean up and commit Mathematica NB Showing correctness. Cite here.
 *
 * NOTE: The vectors MUST be given in the so-called radiation frame where
 * Z is the direction of propagation, X is the principal '+' axis and Y = Z x X
 */
int XLALSimInspiralPrecessingPolarizationWaveforms(
	REAL8TimeSeries **hplus,  /**< +-polarization waveform [returned] */
	REAL8TimeSeries **hcross, /**< x-polarization waveform [returned] */
	REAL8TimeSeries *V,       /**< post-Newtonian parameter */
	REAL8TimeSeries *Phi,     /**< orbital phase */
	REAL8TimeSeries *S1x,	  /**< Spin1 vector x component */
	REAL8TimeSeries *S1y,	  /**< Spin1 vector y component */
	REAL8TimeSeries *S1z,	  /**< Spin1 vector z component */
	REAL8TimeSeries *S2x,	  /**< Spin2 vector x component */
	REAL8TimeSeries *S2y,	  /**< Spin2 vector y component */
	REAL8TimeSeries *S2z,	  /**< Spin2 vector z component */
	REAL8TimeSeries *LNhatx,  /**< unit orbital ang. mom. x comp. */
	REAL8TimeSeries *LNhaty,  /**< unit orbital ang. mom. y comp. */
	REAL8TimeSeries *LNhatz,  /**< unit orbital ang. mom. z comp. */
	REAL8TimeSeries *E1x,	  /**< orbital plane basis vector x comp. */
	REAL8TimeSeries *E1y,	  /**< orbital plane basis vector y comp. */
	REAL8TimeSeries *E1z,	  /**< orbital plane basis vector z comp. */
	REAL8 m1,                 /**< mass of companion 1 (kg) */
	REAL8 m2,                 /**< mass of companion 2 (kg) */
	REAL8 r,                  /**< distance of source (m) */
	REAL8 v0,                 /**< tail-term gauge choice (default = 1) */
	INT4 ampO	 	  /**< twice amp. post-Newtonian order */
	)
{
	static const char *func = "XLALSimInspiralPrecessingPolarizationWaveforms";
    REAL8 s1x, s1y, s1z, s2x, s2y, s2z, lnhx, lnhy, lnhz;
    REAL8 e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, lx, ly, lz;
    REAL8 nx2, ny2, nz2, nz3, lx2, ly2, lz2, lz3;
    REAL8 hplus0, hcross0, hplus05, hcross05, hplus1, hcross1;
    REAL8 hplus15, hcross15, hplusSpin1, hcrossSpin1;
    REAL8 hplusSpin15, hcrossSpin15, hplusTail15, hcrossTail15; 
    REAL8 M, eta, dm, phi, v, v2, dist, ampfac, logfac = 0.;
    INT4 idx, len;

    /* Macros to check time series vectors */
    LAL_CHECK_VALID_SERIES(V, 			XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(Phi, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1z, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2z, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatx, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhaty, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatz, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1x, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1y, 		XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1z, 		XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, Phi, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1z, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2z, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatx, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhaty, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatz, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1x, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1y, 	XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1z, 	XLAL_FAILURE);

    /* Allocate polarization vectors and set to 0 */
    *hplus = XLALCreateREAL8TimeSeries( "H_PLUS", &V->epoch, 
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *hcross = XLALCreateREAL8TimeSeries( "H_CROSS", &V->epoch, 
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    if ( ! hplus || ! hcross )
        XLAL_ERROR(func, XLAL_EFUNC);
    memset((*hplus)->data->data, 0, 
            (*hplus)->data->length*sizeof(*(*hplus)->data->data));
    memset((*hcross)->data->data, 0, 
            (*hcross)->data->length*sizeof(*(*hcross)->data->data));

    M = m1 + m2;
    eta = m1 * m2 / M / M; // symmetric mass ratio - '\nu' in the paper
    dm = (m1 - m2) / M;    // frac. mass difference - \delta m/m in the paper
    dist = r / LAL_C_SI;   // r (m) / c (m/s) --> dist in units of seconds
    /* convert mass from kg to s, so ampfac ~ M/dist is dimensionless */
    ampfac = 2. * M * LAL_G_SI * pow(LAL_C_SI, -3) * eta / dist;
    
    /* loop over time steps and compute polarizations h+ and hx */
    len = V->data->length;
    for(idx = 0; idx < len; idx++)
    {
        /* Abbreviated names in lower case for time series at this sample */
        phi  = Phi->data->data[idx]; 	v = V->data->data[idx];     v2 = v * v;
        lnhx = LNhatx->data->data[idx]; e1x = E1x->data->data[idx];
        lnhy = LNhaty->data->data[idx];	e1y = E1y->data->data[idx];
        lnhz = LNhatz->data->data[idx];	e1z = E1z->data->data[idx];
        s1x  = S1x->data->data[idx];	s2x = S2x->data->data[idx];
        s1y  = S1y->data->data[idx];	s2y = S2y->data->data[idx];
        s1z  = S1z->data->data[idx];	s2z = S2z->data->data[idx];

        /* E2 = LNhat x E1 */
        e2x = lnhy*e1z - lnhz*e1y;
        e2y = lnhz*e1x - lnhx*e1z;
        e2z = lnhx*e1y - lnhy*e1x;

        /* Unit orbital separation vector */
        nx = e1x*cos(phi) + e2x*sin(phi);
        ny = e1y*cos(phi) + e2y*sin(phi);
        nz = e1z*cos(phi) + e2z*sin(phi);

        /* Unit inst. orbital velocity vector */
        lx = e2x*cos(phi) - e1x*sin(phi);
        ly = e2y*cos(phi) - e1y*sin(phi);
        lz = e2z*cos(phi) - e1z*sin(phi);

        /* Powers of vector components */
        nx2 = nx*nx;	ny2 = ny*ny;	nz2 = nz*nz;	nz3 = nz*nz2;
        lx2 = lx*lx;	ly2 = ly*ly;	lz2 = lz*lz;	lz3 = lz*lz2;

        /* 
         * First set all h+/x coefficients to 0. Then use a switch to
         * set proper non-zero values up to order ampO. Note we
         * fall through the PN orders and break only after Newt. order
         */
        hplus0 = hplus05 = hplus1 = hplus15 = hplusTail15 = 0.;
        hcross0 = hcross05 = hcross1 = hcross15 = hcrossTail15 = 0.;
        hplusSpin1 = hplusSpin15 = hcrossSpin1 = hcrossSpin15 = 0.;

        switch( ampO )
        {
            /*
             * case LAL_PNORDER_THREE_POINT_FIVE:
             * case LAL_PNORDER_THREE:
             * case LAL_PNORDER_TWO_POINT_FIVE:
             * case LAL_PNORDER_TWO:
             */
            case 7:
            case 6:
            case 5:
            case 4:
                XLALPrintError("XLAL Error - %s: Amp. corrections not known "
                        "to PN order %d, highest is %d\n", __func__, ampO,
                        MAX_PRECESSING_AMP_PN_ORDER );
                XLAL_ERROR(func, XLAL_EINVAL);
                break;
            case -1: /* Use highest known PN order - move if new orders added */
            /*case LAL_PNORDER_ONE_POINT_FIVE:*/
            case 3:
                /* 1.5PN non-spinning amp. corrections */
                hplus15 = (dm*(2*lx*nx*nz*(-95 + 90*lz2 - 65*nz2 
                        - 2*eta*(-9 + 90*lz2 - 65*nz2)) - 2*ly*ny*nz
                        * (-95 + 90*lz2 - 65*nz2 - 2*eta*(-9 + 90*lz2 - 65*nz2))
                        + 6*lx2*lz*(13 - 4*lz2 + 29*nz2 + eta*(-2 + 8*lz2 
                        - 58*nz2)) - 6*ly2*lz*(13 - 4*lz2 + 29*nz2 + eta
                        * (-2 + 8*lz2 - 58*nz2)) - lz*(nx2 - ny2)*(83 - 6*lz2 
                        + 111*nz2 + 6*eta*(-1 + 2*lz2 - 37*nz2))))/24.;
                hcross15 = (dm*(lz*(6*(19 - 4*eta)*lx*ly + (-101 + 12*eta)
                        * nx*ny) + (-149 + 36*eta) * (ly*nx + lx*ny)*nz 
                        + 6*(-3 + eta) * (2*lx*ly*lz - lz*nx*ny - 3*ly*nx*nz 
                        - 3*lx*ny*nz) + (1 - 2*eta) * (6*lz3*(-4*lx*ly + nx*ny) 
                        + 90*lz2*(ly*nx + lx*ny)*nz + 3*lz*(58*lx*ly 
                        - 37*nx*ny)*nz2 - 65*(ly*nx + lx*ny)*nz3)))/12.;
                /* 1.5PN spinning amp. corrections */
                hplusSpin15 = (6*lz*ny*s1x + 6*dm*lz*ny*s1x - 3*eta*lz*ny*s1x 
                        + 2*ly2*lnhy*s1y + 2*dm*ly2*lnhy*s1y 
                        + 2*eta*ly2*lnhy*s1y + 6*lz*nx*s1y + 6*dm*lz*nx*s1y 
                        - 3*eta*lz*nx*s1y + 8*lnhy*nx2*s1y + 8*dm*lnhy*nx2*s1y 
                        - eta*lnhy*nx2*s1y - 8*lnhy*ny2*s1y - 8*dm*lnhy*ny2*s1y
                        + eta*lnhy*ny2*s1y + 2*ly2*lnhz*s1z + 2*dm*ly2*lnhz*s1z
                        + 2*eta*ly2*lnhz*s1z - 6*ly*nx*s1z - 6*dm*ly*nx*s1z 
                        - 9*eta*ly*nx*s1z + 8*lnhz*nx2*s1z + 8*dm*lnhz*nx2*s1z 
                        - eta*lnhz*nx2*s1z - 8*lnhz*ny2*s1z - 8*dm*lnhz*ny2*s1z
                        + eta*lnhz*ny2*s1z + 6*lz*ny*s2x - 6*dm*lz*ny*s2x 
                        - 3*eta*lz*ny*s2x + lnhx*(2*ly2*((1 + dm + eta)*s1x 
                        + (1 - dm + eta)*s2x) + (nx2 - ny2)*((8 + 8*dm - eta)
                        * s1x - (-8 + 8*dm + eta)*s2x)) + 2*ly2*lnhy*s2y 
                        - 2*dm*ly2*lnhy*s2y + 2*eta*ly2*lnhy*s2y + 6*lz*nx*s2y 
                        - 6*dm*lz*nx*s2y - 3*eta*lz*nx*s2y + 8*lnhy*nx2*s2y 
                        - 8*dm*lnhy*nx2*s2y - eta*lnhy*nx2*s2y - 8*lnhy*ny2*s2y 
                        + 8*dm*lnhy*ny2*s2y + eta*lnhy*ny2*s2y + 2*ly2*lnhz*s2z 
                        - 2*dm*ly2*lnhz*s2z + 2*eta*ly2*lnhz*s2z - 6*ly*nx*s2z 
                        + 6*dm*ly*nx*s2z - 9*eta*ly*nx*s2z + 8*lnhz*nx2*s2z 
                        - 8*dm*lnhz*nx2*s2z - eta*lnhz*nx2*s2z - 8*lnhz*ny2*s2z 
                        + 8*dm*lnhz*ny2*s2z + eta*lnhz*ny2*s2z - 3*lx*ny 
                        * ((2 + 2*dm + 3*eta)*s1z + (2 - 2*dm + 3*eta)*s2z)
                        - 2*lx2*(lnhx*((1 + dm + eta)*s1x + (1 - dm + eta)*s2x) 
                        + lnhy*((1 + dm + eta)*s1y + (1 - dm + eta)*s2y) 
                        + lnhz*((1 + dm + eta)*s1z + (1 - dm + eta)*s2z)))/3.;
                hcrossSpin15 = (-3*lz*(nx*((2 + 2*dm - eta)*s1x 
                        - (-2 + 2*dm + eta)*s2x) + ny*((-2 - 2*dm + eta)*s1y 
                        + (-2 + 2*dm + eta)*s2y)) + ny*(-6*ly*s1z - 6*dm*ly*s1z 
                        - 9*eta*ly*s1z + 16*lnhz*nx*s1z + 16*dm*lnhz*nx*s1z 
                        - 2*eta*lnhz*nx*s1z + 2*lnhx*nx*((8 + 8*dm - eta)*s1x 
                        - (-8 + 8*dm + eta)*s2x) + 2*lnhy*nx*((8 + 8*dm - eta)
                        * s1y - (-8 + 8*dm + eta)*s2y) - 6*ly*s2z + 6*dm*ly*s2z 
                        - 9*eta*ly*s2z + 16*lnhz*nx*s2z - 16*dm*lnhz*nx*s2z 
                        - 2*eta*lnhz*nx*s2z) - lx*(4*lnhx*ly*((1 + dm + eta)*s1x
                        + (1 - dm + eta)*s2x) - 3*nx*((2 + 2*dm + 3*eta)*s1z 
                        + (2 - 2*dm + 3*eta)*s2z) + 4*ly*(lnhy*((1 + dm + eta)
                        * s1y + (1 - dm + eta)*s2y) + lnhz*((1 + dm + eta)*s1z 
                        + (1 - dm + eta)*s2z))))/3.;
                /* 1.5PN tail amp. corrections */
                logfac = log(v/v0);
                hplusTail15 = 2*((lx2 - ly2 - nx2 + ny2)*LAL_PI 
                        + 12*(lx*nx - ly*ny)*logfac);
                hcrossTail15 = 4*((lx*ly - nx*ny)*LAL_PI 
                        + 6*(ly*nx + lx*ny)*logfac);

            /*case LAL_PNORDER_ONE:*/
            case 2:
                /* 1PN non-spinning amp. corrections */
                hplus1 = (-13*lx2 + 13*ly2 + 6*lx2*lz2 - 6*ly2*lz2 
                        + 13*(nx2 - ny2) - 2*lz2*(nx2 - ny2) - 32*lx*lz*nx*nz 
                        + 32*ly*lz*ny*nz - 14*lx2*nz2 + 14*ly2*nz2 
                        + 10*(nx2 - ny2)*nz2)/6. + (eta*(lx2 - 18*lx2*lz2 
                        + 96*lx*lz*nx*nz - 96*ly*lz*ny*nz + 42*lx2*nz2 
                        + ly2*(-1 + 18*lz2 - 42*nz2) + (nx2 - ny2) 
                        * (-1 + 6*lz2 - 30*nz2)))/6.;
                hcross1 = (eta*(lx*ly - nx*ny - 6*(lz2*(3*lx*ly - nx*ny) 
                        - 8*lz*(ly*nx + lx*ny)*nz + (-7*lx*ly 
                        + 5*nx*ny)*nz2)))/3. + (-13*(lx*ly - nx*ny) 
                        + 2*(lz2*(3*lx*ly - nx*ny) - 8*lz*(ly*nx + lx*ny)*nz 
                        + (-7*lx*ly + 5*nx*ny)*nz2))/3.;
                /* 1PN spinning amp. corrections */
                hplusSpin1 = (-(ny*((1 + dm)*s1x + (-1 + dm)*s2x)) 
                        - nx*((1 + dm)*s1y + (-1 + dm)*s2y))/2.;
                hcrossSpin1 = (nx*((1 + dm)*s1x + (-1 + dm)*s2x) 
                        - ny*((1 + dm)*s1y + (-1 + dm)*s2y))/2.;

            /*case LAL_PNORDER_HALF:*/
            case 1:
                /* 0.5PN non-spinning amp. corrections */
                hplus05 = (dm*(-2*lx2*lz + 2*ly2*lz + lz*(nx2 - ny2) 
                        + 6*lx*nx*nz - 6*ly*ny*nz))/2.;
                hcross05 = dm*(-2*lx*ly*lz + lz*nx*ny 
					+ 3*ly*nx*nz + 3*lx*ny*nz);

            /*case LAL_PNORDER_NEWTONIAN:*/
            case 0:
                /* Newtonian order polarizations */
                hplus0 = lx2 - ly2 - nx2 + ny2;
                hcross0 = 2*lx*ly - 2*nx*ny;
                break;
            default: 
                XLALPrintError("XLAL Error - %s: Invalid amp. PN order %s\n", 
                        __func__, ampO );
                XLAL_ERROR(func, XLAL_EINVAL);
                break;
        } /* End switch on ampO */

        /* Fill the output polarization arrays */
        (*hplus)->data->data[idx] = ampfac * v2 * ( hplus0 
                + v * ( hplus05 + v * ( hplus1 + hplusSpin1 
                + v * ( hplus15 + hplusSpin15 + hplusTail15 ) ) ) );
        (*hcross)->data->data[idx] = ampfac * v2 * ( hcross0 
                + v * ( hcross05 + v * ( hcross1 + hcrossSpin1 
                + v * ( hcross15 + hcrossSpin15 + hcrossTail15 ) ) ) );
    } /* end of loop over time samples, idx */
    return XLAL_SUCCESS;
}

/**
 * Compute the physical template family "Q" vectors for a spinning, precessing
 * binary when provided time series of all the dynamical quantities.
 * These vectors always supplied to dominant order.
 *
 * Based on Pan, Buonanno, Chan and Vallisneri PRD69 104017, (see also theses
 * of Diego Fazi and Ian Harry)
 *
 * NOTE: The vectors MUST be given in the so-called radiation frame where
 * Z is the direction of propagation, X is the principal '+' axis and Y = Z x X
 */


int XLALSimInspiralPrecessingPTFQWaveforms(
        REAL8TimeSeries **Q1,     /**< PTF-Q1 waveform [returned] */
        REAL8TimeSeries **Q2,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q3,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q4,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries **Q5,     /**< PTF-Q2 waveform [returned] */
        REAL8TimeSeries *V,       /**< post-Newtonian parameter */
        REAL8TimeSeries *Phi,     /**< orbital phase */
        REAL8TimeSeries *S1x,     /**< Spin1 vector x component */
        REAL8TimeSeries *S1y,     /**< Spin1 vector y component */
        REAL8TimeSeries *S1z,     /**< Spin1 vector z component */
        REAL8TimeSeries *S2x,     /**< Spin2 vector x component */
        REAL8TimeSeries *S2y,     /**< Spin2 vector y component */
        REAL8TimeSeries *S2z,     /**< Spin2 vector z component */
        REAL8TimeSeries *LNhatx,  /**< unit orbital ang. mom. x comp. */
        REAL8TimeSeries *LNhaty,  /**< unit orbital ang. mom. y comp. */
        REAL8TimeSeries *LNhatz,  /**< unit orbital ang. mom. z comp. */
        REAL8TimeSeries *E1x,     /**< orbital plane basis vector x comp. */
        REAL8TimeSeries *E1y,     /**< orbital plane basis vector y comp. */
        REAL8TimeSeries *E1z,     /**< orbital plane basis vector z comp. */
        REAL8 m1,                 /**< mass of companion 1 (kg) */
        REAL8 m2,                 /**< mass of companion 2 (kg) */
        REAL8 r                  /**< distance of source (m) */
        )
{
	static const char *func = "XLALSimInspiralPrecessingPTFQWaveforms";
    REAL8 lnhx, lnhy, lnhz;
    REAL8 e1x, e1y, e1z, e2x, e2y, e2z, nx, ny, nz, lx, ly, lz;
    REAL8 nx2, ny2, nz2, lx2, ly2, lz2;
    REAL8 q1tmp, q2tmp, q3tmp, q4tmp, q5tmp;
    REAL8 M, eta, phi, v, v2, dist, ampfac;
    INT4 idx, len;
    REAL8 sqrt_three = pow(3,0.5);

    /* Macros to check time series vectors */
    LAL_CHECK_VALID_SERIES(V,                   XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(Phi,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S1z,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(S2z,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatx,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhaty,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(LNhatz,              XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1x,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1y,                 XLAL_FAILURE);
    LAL_CHECK_VALID_SERIES(E1z,                 XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, Phi,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S1z,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, S2z,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatx, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhaty, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, LNhatz, XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1x,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1y,    XLAL_FAILURE);
    LAL_CHECK_CONSISTENT_TIME_SERIES(V, E1z,    XLAL_FAILURE);


    /* Allocate polarization vectors and set to 0 */
    *Q1 = XLALCreateREAL8TimeSeries( "PTF_Q_1", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q2 = XLALCreateREAL8TimeSeries( "PTF_Q_2", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q3 = XLALCreateREAL8TimeSeries( "PTF_Q_3", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q4 = XLALCreateREAL8TimeSeries( "PTF_Q_4", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );
    *Q5 = XLALCreateREAL8TimeSeries( "PTF_Q_5", &V->epoch,
            0.0, V->deltaT, &lalStrainUnit, V->data->length );

    if ( ! Q1 || ! Q2 || !Q3 || !Q4 || !Q5 )
        XLAL_ERROR(func, XLAL_EFUNC);
    memset((*Q1)->data->data, 0,
            (*Q1)->data->length*sizeof(*(*Q1)->data->data));
    memset((*Q2)->data->data, 0,
            (*Q2)->data->length*sizeof(*(*Q2)->data->data));
    memset((*Q3)->data->data, 0,
            (*Q3)->data->length*sizeof(*(*Q3)->data->data));
    memset((*Q4)->data->data, 0,
            (*Q4)->data->length*sizeof(*(*Q4)->data->data));
    memset((*Q5)->data->data, 0,
            (*Q5)->data->length*sizeof(*(*Q5)->data->data));

    M = m1 + m2;
    eta = m1 * m2 / M / M; // symmetric mass ratio - '\nu' in the paper
    dist = r / LAL_C_SI;   // r (m) / c (m/s) --> dist in units of seconds
    /* convert mass from kg to s, so ampfac ~ M/dist is dimensionless */
    ampfac = 2. * M * LAL_G_SI * pow(LAL_C_SI, -3) * eta / dist;

    /* loop over time steps and compute the Qi */
    len = V->data->length;
    for(idx = 0; idx < len; idx++)
    {
        /* Abbreviated names in lower case for time series at this sample */
        phi  = Phi->data->data[idx];    v = V->data->data[idx];     v2 = v * v;
        lnhx = LNhatx->data->data[idx]; e1x = E1x->data->data[idx];
        lnhy = LNhaty->data->data[idx]; e1y = E1y->data->data[idx];
        lnhz = LNhatz->data->data[idx]; e1z = E1z->data->data[idx];

        /* E2 = LNhat x E1 */
        e2x = lnhy*e1z - lnhz*e1y;
        e2y = lnhz*e1x - lnhx*e1z;
        e2z = lnhx*e1y - lnhy*e1x;

        /* Unit orbital separation vector */
        nx = e1x*cos(phi) + e2x*sin(phi);
        ny = e1y*cos(phi) + e2y*sin(phi);
        nz = e1z*cos(phi) + e2z*sin(phi);

        /* Unit inst. orbital velocity vector */
        lx = e2x*cos(phi) - e1x*sin(phi);
        ly = e2y*cos(phi) - e1y*sin(phi);
        lz = e2z*cos(phi) - e1z*sin(phi);

        /* Powers of vector components */
        nx2 = nx*nx;    ny2 = ny*ny;    nz2 = nz*nz;
        lx2 = lx*lx;    ly2 = ly*ly;    lz2 = lz*lz;

        /* 
         * NOTE: For PTF waveforms, we must use only the dominant amplitude
         * The Q values are computed from equations 13,14,17, 46 + 47 in PBCV or
         * more simply from equations (3.10) in Diego Fazi's thesis.
         * Note that Q5 is simplified from that shown in Fazi's thsis
         * by using traceless condition. As demonstrated in (6.29)
         * in Ian Harry's thesis.
         */

        q1tmp = lx2 - ly2 - nx2 + ny2;
        q2tmp = 2*lx*ly - 2*nx*ny;
        q3tmp = 2*lx*lz - 2*nx*nz;
        q4tmp = 2*ly*lz - 2*ny*nz;
        q5tmp = sqrt_three * (nz2 - lz2);

        /* Fill the output vectors */
        (*Q1)->data->data[idx] = ampfac * v2 * q1tmp;
        (*Q2)->data->data[idx] = ampfac * v2 * q2tmp;
        (*Q3)->data->data[idx] = ampfac * v2 * q3tmp;
        (*Q4)->data->data[idx] = ampfac * v2 * q4tmp;
        (*Q5)->data->data[idx] = ampfac * v2 * q5tmp;
    }
    return XLAL_SUCCESS;
}

/** Calculates the tidal deformability parameter as a function
 *  of mass, for several different Neutron Star Equations
 *  of State. The mass argument should be the intrinsic mass 
 *  in solar mass units.
 *  The functions are interpolated with 4th order polynomials.
 *  The return value for lambda is in units s^5.
 * **/

REAL8 XLALLambdaOfM_EOS(LALInspiralEOS EoS, REAL8 compMass){
    
    /* For the prefactor check the units of Hinderer et al arXiv:0911.3535v1 
     * There, g cm^2 s^2 is used and we want to translate to s^5 by 
     * multiplying with 10^{-7}*G/c^5 */
    REAL8 prefac = 1.0E29 * LAL_G_SI / pow(LAL_C_SI,5.);
    REAL8 lambda = 0.;

    switch (((int) EoS))
    {
	// Point Particle
		case PP:
		lambda = 0.0;
		break;
    // MS1
        case MS1:
        lambda = prefac*(2.19296 + 20.0273*compMass - 17.9443*compMass*compMass 
        + 5.75129*compMass*compMass*compMass - 0.699095*compMass*compMass*compMass*compMass);
        break;
    // H4
        case H4:
        lambda = prefac*(0.743351 + 15.8917*compMass - 14.7348*compMass*compMass 
        + 5.32863*compMass*compMass*compMass - 0.942625*compMass*compMass*compMass*compMass);
        break; 
    // SQM3
        case SQM3:
        lambda = prefac*(-5.55858 + 20.8977*compMass - 20.5583*compMass*compMass 
        + 9.55465*compMass*compMass*compMass - 1.84933*compMass*compMass*compMass*compMass);
        break;
    // GM1
        case GM1:
        lambda = prefac*( 2.91357797 + 8.56300011*compMass - 6.34530151*compMass*compMass 
        + 0.979368411*compMass*compMass*compMass);
        break;
    // L
        case LEOS:
        lambda = prefac*(1.58993910 + 15.9638882*compMass - 9.58564021*compMass*compMass 
        + 1.34201455*compMass*compMass*compMass);
        break;
    // SLY
        case SLY:
        lambda = prefac*(1.28653162 + 3.77058998*compMass - 3.31593965*compMass*compMass 
        + 0.596473986*compMass*compMass*compMass);
        break;
         
        default:
        lambda = 0.0;
        fprintf(stderr, "Unknown EoS for the calculation of tidal deformability. Lambda is set to 0.\n");
        break;
	}
    return lambda;
}

/** Calculates the quadrupole-monopole parameter as a function
 *  of mass, for several different Neutron Star Equations
 *  of State. The mass argument should be the intrinsic mass 
 *  in solar mass units.
 *  The functions are interpolated with 3rd order polynomials.
 * **/

REAL8 XLALQMparameter_EOS(LALInspiralEOS eos_type, REAL8 m_intr_msun){
  
  REAL8 q = 0.0 ;
  REAL8 m = m_intr_msun ;
  REAL8 m2 = m*m ;
  REAL8 m3 = m2*m ;
  
  switch (eos_type) {
  /* A */
    case AEOS:
    q = -6.41414141*m3 + 30.70779221*m2 - 53.37417027*m + 35.62253247 ;
    break;
  /* AU */
    case AU:
    q = -6.18686869*m3 + 30.15909091*m2 - 52.87806638*m + 35.86616883 ;
    break;
  /* FPS */
    case FPS:
    q = -3.86363636*m3 + 21.03030303*m2 - 42.19448052*m + 32.83722944 ;
    break;
  /* APR */
    case APR:
    q = -10.55555556*m3 + 49.52380952*m2 - 82.77063492*m + 53.02428571 ;
    break;
  /* UU */
    case UU:
    q = -8.03030303*m3 + 37.61363636*m2 - 63.48733766*m + 41.75080087 ;
    break;
  /* L */
    case LEOS:
    q = -2.55905286*m3 + 17.4748882*m2  - 42.58067022*m + 39.98852253 ;
    // this is the old fit q = -6.59090909*m3 + 33.67424242*m2 - 63.77034632*m + 48.98073593 ;
    break;
  /* GM1 */
    case GM1:
    q = -4.97311828*m3 + 29.23153294*m2 - 60.91713992*m + 48.32960152 ;
    break;
  /* SLY */
    case SLY:
    q = -6.27440001*m3 + 32.92777333*m2 - 61.48850029*m + 43.71802667 ;
    break;

    case PP:
    q = 1.0 ;
    break;

  default:
    q = 1.0 ;
    fprintf(stderr, "Unknown EoS for the calculation of QM parameter. QM is set to 1.0 .\n");
    break ;
  }
  
  return q ;
}
