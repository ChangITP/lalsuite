/*
*  Copyright (C) 2007 Walter Del Pozzo
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

/* <lalVerbatim> */
#ifndef _LALINSPIRALBRANSDICKE_H  /* Double-include protection. */
#define _LALINSPIRALBRANSDICKE_H

#ifdef  __cplusplus   /* C++ protection. */
extern "C" {
#endif


NRCSID( LALINSPIRALBRANSDICKE, "$Id$" );

    void
    LALInspiralBransDicke (
                                           LALStatus        *status,
                                           REAL4Vector      *signalvec,
                                           InspiralTemplate *params);

    void LALInspiralBransDickePhasing(
                          InspiralTemplate *params,
                          REAL8 f,
                          REAL8 *psif);
                          


#ifdef  __cplusplus
}
#endif  /* C++ protection. */
#endif  /* Double-include protection. */
/* </lalVerbatim> */
