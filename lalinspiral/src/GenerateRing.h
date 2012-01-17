/*
*  Copyright (C) 2007 Bernd Machenschalk, Duncan Brown
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

/**
 * \defgroup GenerateRing_h GenerateRing_h
 * \ingroup inject
 */

/**
\author Goggin, L., and Brown, D. A.
\file
\ingroup GenerateRing_h

\brief Provides routines to generate ringdown waveforms.

\heading{Synopsis}
\code
#include <lal/GenerateRing.h>
\endcode

*/

#ifndef _GENERATERING_H
#define _GENERATERING_H

#include <lal/LALStdlib.h>
#include <lal/SimulateCoherentGW.h>
#include <lal/SkyCoordinates.h>
#include <lal/LIGOMetadataTables.h>

NRCSID (GENERATERINGH,"$Id$");

#if defined(__cplusplus)
extern "C" {
#elif 0
} /* so that editors will match preceding brace */
#endif


/**  \name Error Codes */ /*@{*/
#define GENERATERINGH_ENUL 1
#define GENERATERINGH_EOUT 2
#define GENERATERINGH_EMEM 3
#define GENERATERINGH_ETYP 4
#define GENERATERINGH_ELEN 5

#define GENERATERINGH_MSGENUL "Unexpected null pointer in arguments"
#define GENERATERINGH_MSGEOUT "Output field a, f, phi, or shift already exists"
#define GENERATERINGH_MSGEMEM "Out of memory"
#define GENERATERINGH_MSGETYP "Waveform type not implemented"
#define GENERATERINGH_MSGELEN "Waveform length not correctly specified"
/*@}*/

typedef enum{
  Ringdown
} SimRingType;


/** This structure stores the parameters for constructing a burst gravitational
    waveform
*/
typedef struct tagRingParamStruc {
  REAL8 deltaT;             /* requested sampling interval (s) */
  CoordinateSystem system;  /* coordinate system to assume for simRingdown */
} RingParamStruc;

/* Function prototypes. */

void
LALGenerateRing(
    LALStatus          *status,
    CoherentGW         *output,
    REAL4TimeSeries    *series,
    SimRingdownTable   *simRingdown,
    RingParamStruc     *params
    );

void
LALRingInjectSignals(
    LALStatus               *status,
    REAL4TimeSeries         *series,
    SimRingdownTable        *injections,
    COMPLEX8FrequencySeries *resp,
    INT4                     calType
    );

#if 0
{ /* so that editors will match succeeding brace */
#elif defined(__cplusplus)
}
#endif

#endif /* _GENERATERING_H */
