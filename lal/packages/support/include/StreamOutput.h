/********************************* <lalVerbatim file="StreamOutputHV">
Author: Creighton, T. D.
$Id$
**************************************************** </lalVerbatim> */

/********************************************************** <lalLaTeX>

\section{Header \texttt{StreamOutput.h}}
\label{s:StreamOutput.h}

Provides routines to write data from LAL data structures to an open
stream.

\subsection*{Synopsis}
\begin{verbatim}
#include "StreamOutput.h"
\end{verbatim}

\noindent This header provides prototypes for routines that write the
contents of LAL time and frequency series structures to a file (or
other I/O) stream, in a standard format.  The routines do not provide
a system-level interface to create files and open or close file
streams; they simply assume that they have been passed an open,
writeable stream.  Nonetheless, because they involve I/O stream
manipulation, these routines are placed in the \verb@lalsupport@
library rather than in \verb@lal@ proper.

******************************************************* </lalLaTeX> */

#ifndef _STREAMOUTPUT_H
#define _STREAMOUTPUT_H

#include <lal/LALStdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

NRCSID( STREAMOUTPUTH, "$Id$" );

/********************************************************** <lalLaTeX>
\subsection*{Error conditions}
****************************************** </lalLaTeX><lalErrTable> */
#define STREAMOUTPUTH_ENUL 1
#define STREAMOUTPUTH_EPRN 2

#define STREAMOUTPUTH_MSGENUL "Unexpected null pointer in arguments"
#define STREAMOUTPUTH_MSGEPRN "Print statement failed"
/******************************************** </lalErrTable><lalLaTeX>

\subsection*{Types}

******************************************************* </lalLaTeX> */

/* <lalLaTeX>
\vfill{\footnotesize\input{StreamOutputHV}}
</lalLaTeX> */

/* Function prototypes. */

/* <lalLaTeX>
\newpage\input{StreamSeriesOutputC}
</lalLaTeX> */
void
LALI2WriteTSeries( LALStatus  *stat, FILE *stream, INT2TimeSeries *series );
void
LALI4WriteTSeries( LALStatus  *stat, FILE *stream, INT4TimeSeries *series );
void
LALI8WriteTSeries( LALStatus  *stat, FILE *stream, INT8TimeSeries *series );
void
LALU2WriteTSeries( LALStatus  *stat, FILE *stream, UINT2TimeSeries *series );
void
LALU4WriteTSeries( LALStatus  *stat, FILE *stream, UINT4TimeSeries *series );
void
LALU8WriteTSeries( LALStatus  *stat, FILE *stream, UINT8TimeSeries *series );
void
LALSWriteTSeries( LALStatus  *stat, FILE *stream, REAL4TimeSeries *series );
void
LALDWriteTSeries( LALStatus  *stat, FILE *stream, REAL8TimeSeries *series );
void
LALCWriteTSeries( LALStatus  *stat, FILE *stream, COMPLEX8TimeSeries *series );
void
LALZWriteTSeries( LALStatus  *stat, FILE *stream, COMPLEX16TimeSeries *series );

void
LALI2WriteTVectorSeries( LALStatus  *stat, FILE *stream, INT2TimeVectorSeries *series );
void
LALI4WriteTVectorSeries( LALStatus  *stat, FILE *stream, INT4TimeVectorSeries *series );
void
LALI8WriteTVectorSeries( LALStatus  *stat, FILE *stream, INT8TimeVectorSeries *series );
void
LALU2WriteTVectorSeries( LALStatus  *stat, FILE *stream, UINT2TimeVectorSeries *series );
void
LALU4WriteTVectorSeries( LALStatus  *stat, FILE *stream, UINT4TimeVectorSeries *series );
void
LALU8WriteTVectorSeries( LALStatus  *stat, FILE *stream, UINT8TimeVectorSeries *series );
void
LALSWriteTVectorSeries( LALStatus  *stat, FILE *stream, REAL4TimeVectorSeries *series );
void
LALDWriteTVectorSeries( LALStatus  *stat, FILE *stream, REAL8TimeVectorSeries *series );
void
LALCWriteTVectorSeries( LALStatus  *stat, FILE *stream, COMPLEX8TimeVectorSeries *series );
void
LALZWriteTVectorSeries( LALStatus  *stat, FILE *stream, COMPLEX16TimeVectorSeries *series );

void
LALI2WriteTArraySeries( LALStatus  *stat, FILE *stream, INT2TimeArraySeries *series );
void
LALI4WriteTArraySeries( LALStatus  *stat, FILE *stream, INT4TimeArraySeries *series );
void
LALI8WriteTArraySeries( LALStatus  *stat, FILE *stream, INT8TimeArraySeries *series );
void
LALU2WriteTArraySeries( LALStatus  *stat, FILE *stream, UINT2TimeArraySeries *series );
void
LALU4WriteTArraySeries( LALStatus  *stat, FILE *stream, UINT4TimeArraySeries *series );
void
LALU8WriteTArraySeries( LALStatus  *stat, FILE *stream, UINT8TimeArraySeries *series );
void
LALSWriteTArraySeries( LALStatus  *stat, FILE *stream, REAL4TimeArraySeries *series );
void
LALDWriteTArraySeries( LALStatus  *stat, FILE *stream, REAL8TimeArraySeries *series );
void
LALCWriteTArraySeries( LALStatus  *stat, FILE *stream, COMPLEX8TimeArraySeries *series );
void
LALZWriteTArraySeries( LALStatus  *stat, FILE *stream, COMPLEX16TimeArraySeries *series );

void
LALI2WriteFSeries( LALStatus  *stat, FILE *stream, INT2FrequencySeries *series );
void
LALI4WriteFSeries( LALStatus  *stat, FILE *stream, INT4FrequencySeries *series );
void
LALI8WriteFSeries( LALStatus  *stat, FILE *stream, INT8FrequencySeries *series );
void
LALU2WriteFSeries( LALStatus  *stat, FILE *stream, UINT2FrequencySeries *series );
void
LALU4WriteFSeries( LALStatus  *stat, FILE *stream, UINT4FrequencySeries *series );
void
LALU8WriteFSeries( LALStatus  *stat, FILE *stream, UINT8FrequencySeries *series );
void
LALSWriteFSeries( LALStatus  *stat, FILE *stream, REAL4FrequencySeries *series );
void
LALDWriteFSeries( LALStatus  *stat, FILE *stream, REAL8FrequencySeries *series );
void
LALCWriteFSeries( LALStatus  *stat, FILE *stream, COMPLEX8FrequencySeries *series );
void
LALZWriteFSeries( LALStatus  *stat, FILE *stream, COMPLEX16FrequencySeries *series );

#ifdef __cplusplus
}
#endif

#endif /* _STREAMOUTPUT_H */
