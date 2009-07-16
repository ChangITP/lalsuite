/* 

LALInferenceTemplate.c:	Bayesian Followup, template calls to LAL template functions. Temporary GeneratePPN

Copyright 2009 Ilya Mandel, Vivien Raymond, Christian Roever, Marc van der Sluys and John Veitch

*/

#include <stdio.h>
#include <stdlib.h>
#include <lal/LALInspiral.h>
#include <lal/SeqFactories.h>
#include "LALInference.h"


void LALTemplateWrapper(LALIFOData *IFOdata){

	static LALStatus stat;								/* status structure */	

	REAL4 m1=*(REAL4 *)getVariable(IFOdata->modelParams,"m1");			/* binary masses */	
	REAL4 m2=*(REAL4 *)getVariable(IFOdata->modelParams,"m2");
	
	REAL4 dist=1.0;//*(REAL4 *)getVariable(IFOdata->modelParams,"dist");      /* binary distance SET AS FIDUCIAL */
	REAL4 inc=*(REAL4 *)getVariable(IFOdata->modelParams,"inc");		/* inclination and coalescence phase */
	//REAL4 phii=*(REAL4 *)getVariable(IFOdata->modelParams,"phii");

	
	REAL4 f_min = IFOdata->fLow; 
	REAL4 f_max = IFOdata->fHigh;			/* start and stop frequencies */
	
	
	REAL8 dt = 0.01;//ifo->timeData->deltaT;					/* sampling interval */
	REAL8 deltat = 0.01;//ifo->timeData->deltaT;				/* wave sampling interval */
	INT4 order = 4;										/* PN order */
	
	/* Other variables. */
	UINT4 i;                      /* index */
	PPNParamStruc params;         /* input parameters */
	CoherentGW waveform;          /* output waveform */
	

	/* Make sure that values won't crash the system or anything. */
//	CHECKVAL( order, -1, 5 );
//	CHECKVAL( dt, LAL_REAL4_MIN, LAL_REAL4_MAX );
//	CHECKVAL( deltat, 0.0, LAL_REAL4_MAX );
	
	
	/*******************************************************************
	 * INPUT SETUP                                                     *
	 *******************************************************************/
	
	/* Fixed parameters. Set them when injecting....*/
	params.position.latitude = 0.0;//*(REAL4 *)getVariable(ifo->modelParams,"latitude");
	params.position.longitude = 0.0;//*(REAL4 *)getVariable(ifo->modelParams,"longitude");
	params.position.system = COORDINATESYSTEM_EQUATORIAL;
	params.psi = 0.0;
	params.lengthIn = 0;
	params.epoch.gpsSeconds = (INT4)(0);
	params.epoch.gpsNanoSeconds = (INT4)(0);

	/* Variable parameters. */

	params.deltaT = dt;
	params.mTot = m1 + m2;
	params.eta = m1*m2/( params.mTot*params.mTot );
	params.inc = inc;
	params.phi = 0.0;
	params.d = dist*LAL_PC_SI*1.0e3;
	params.fStartIn = f_min;
	params.fStopIn = f_max;

	/* PPN parameter. */
	params.ppn = NULL;
	LALSCreateVector( &stat, &(params.ppn), order + 1 );
	params.ppn->data[0] = 1.0;
	if ( order > 0 )
		params.ppn->data[1] = 0.0;
	for ( i = 2; i <= (UINT4)( order ); i++ )
		params.ppn->data[i] = 1.0;
	/* Output parameters. */
	memset( &waveform, 0, sizeof(CoherentGW) );
	
	
	/*******************************************************************
	 * OUTPUT GENERATION                                               *
	 *******************************************************************/

	/* Generate waveform. */
	LALGeneratePPNInspiral( &stat, &waveform, &params );
	
	/* Print termination information. */
	//snprintf( message, MSGLENGTH, "%d: %s", params.termCode,
	//		 params.termDescription );
	//INFO( message );
	
	/* Print coalescence phase.
	 snprintf( message, MSGLENGTH,
	 "Waveform ends %.3f cycles before coalescence",
	 -waveform.phi->data->data[waveform.phi->data->length-1]
	 / (REAL4)( LAL_TWOPI ) ); */
	/*{
		INT4 code = sprintf( message,
							"Waveform ends %.3f cycles before coalescence",
							-waveform.phi->data->data[waveform.phi->data->length
													  -1]
							/ (REAL4)( LAL_TWOPI ) );
		if ( code >= MSGLENGTH || code < 0 ) {
			ERROR( GENERATEPPNINSPIRALTESTC_EPRINT,
				  GENERATEPPNINSPIRALTESTC_MSGEPRINT, 0 );
			return GENERATEPPNINSPIRALTESTC_EPRINT;
		}
	}
	INFO( message );*/
	
	/* Check if sampling interval was too large. */
	if ( params.dfdt > 2.0 ) {
		printf(
				 "Waveform sampling interval is too large:\n"
				 "\tmaximum df*dt = %f", params.dfdt );
		//WARNING( message );
	}
	
	/* Renormalize phase. */
	//phii -= waveform.phi->data->data[0];
	//for ( i = 0; i < waveform.phi->data->length; i++ )
	//	waveform.phi->data->data[i] += phii;
	
	/* Write output. */
//		if ( ( fp = fopen( outfile, "w" ) ) == NULL ) {
//			ERROR( GENERATEPPNINSPIRALTESTC_EFILE,
//				  GENERATEPPNINSPIRALTESTC_MSGEFILE, outfile );
//			return GENERATEPPNINSPIRALTESTC_EFILE;
//		}
		
		/* Amplitude and phase functions: */
//		if ( deltat == 0.0 ) {
//			REAL8 t = 0.0; /* time */
//			for ( i = 0; i < waveform.a->data->length; i++, t += dt )
//				fprintf( fp, "%f %.3f %10.3e %10.3e %10.3e\n", t,
//						waveform.phi->data->data[i],
//						waveform.f->data->data[i],
//						waveform.a->data->data[2*i],
//						waveform.a->data->data[2*i+1] );
//		}
		
		/* Waveform: */
			REAL8 t = 0.0;
			REAL8 x = 0.0;
			REAL8 dx = deltat/dt;
			REAL8 xMax = waveform.a->data->length - 1;
			REAL8 *phiData = waveform.phi->data->data;
			//REAL4 *fData = waveform.f->data->data;
			REAL4 *aData = waveform.a->data->data;
			for ( ; x < xMax; x += dx, t += deltat ) {
				UINT4 j = floor( x );
				REAL8 frac = x - j;
				REAL8 p = frac*phiData[j+1] + ( 1.0 - frac )*phiData[j];
				//REAL8 f = frac*fData[j+1] + ( 1.0 - frac )*fData[j];
				REAL8 ap = frac*aData[2*j+2] + ( 1.0 - frac )*aData[2*j];
				REAL8 ac = frac*aData[2*j+3] + ( 1.0 - frac )*aData[2*j+1];
				
				if(j<10){
					fprintf(stdout,"%13.6e\t%13.6e\n",ap*cos( p ),ac*sin( p ));
				}
			}
		
		
	
	/*******************************************************************
	 * CLEANUP                                                         *
	 *******************************************************************/
	
	LALSDestroyVector( &stat, &(params.ppn) );
	LALSDestroyVectorSequence( &stat, &(waveform.a->data) );
	LALSDestroyVector( &stat, &(waveform.f->data) );
	LALDDestroyVector( &stat, &(waveform.phi->data) );
	LALFree( waveform.a );
	LALFree( waveform.f );
	LALFree( waveform.phi );
	
	LALCheckMemoryLeaks();
//	INFO( GENERATEPPNINSPIRALTESTC_MSGENORM );
//	return GENERATEPPNINSPIRALTESTC_ENORM;
	
}
