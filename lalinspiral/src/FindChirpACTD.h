
/*-----------------------------------------------------------------------
 *
 * File Name: FindChirpACTD.h
 *
 * Author: McKechan, D. J. A.
 *
 * Revision: $Id$
 *
 *-----------------------------------------------------------------------
 */

#if 0
<lalVerbatim file="FindChirpACTDHV">
Author: McKechan, D. J. A.
$Id$
</lalVerbatim>

<lalLaTeX>
\section{Header \texttt{FindChirpACTD.h}}
\label{s:FindChirpACTD.h}

Provides structures and functions for amplitude corrected time domain
templates using AmpCorPPN.

\subsection*{Synopsis}

\begin{verbatim}
#include <lal/FindChirpACTD.h>
\end{verbatim}

</lalLaTeX>
#endif


#ifndef _FINDCHIRPACTDH_H
#define _FINDCHIRPACTDH_H

#ifdef  __cplusplus
extern "C" {
#pragma }
#endif


NRCSID (FINDCHIRPACTDH, "$Id$");

#if 0
<lalLaTeX>
\subsection*{Error codes}
</lalLaTeX>
#endif
/* <lalErrTable> */
#define FINDCHIRPACTDH_EQMAS  1
#define FINDCHIRPACTDH_EACTDV 2

#define FINDCHIRPACTDH_MSGEQMAS "AmpCorPPN template equal mass"
#define FINDCHIRPACTDH_MSGEACTDV "Num ACTDVecs too few"
/* </lalErrTable> */

#if 0
<lalLaTeX>
Define number of vectors, 6 for 0.5PN.
</lalLaTeX>
#endif
#define NACTDVECS (3)
#define NACTDTILDEVECS (6)

#if 0
<lalLaTeX>
\subsection*{Types}

None.
</lalLaTeX>
#endif


#if 0
<lalLaTeX>
\vfill{\footnotesize\input{FindChirpACTDHV}}
</lalLaTeX>
#endif

#if 0
<lalLaTeX>
\newpage\input{FindChirpACTDTemplateC}
</lalLaTeX>
#endif

void
LALFindChirpACTDTemplate (
    LALStatus                  *status,
    FindChirpTemplate          *fcTmplt,
    InspiralTemplate           *theTmplt,
    FindChirpTmpltParams       *params
    );

#if 0
<lalLaTeX>
\newpage\input{FindChirpACTDNormalizeC}
</lalLaTeX>
#endif

void
LALFindChirpACTDNormalize(
    LALStatus                  *status,
    FindChirpTemplate          *fcTmplt,
    FindChirpTmpltParams       *tmpltParams,
    FindChirpDataParams        *params
    );

#if 0
<lalLaTeX>
\newpage\input{FindChirpACTDFilterSegmentC}
</lalLaTeX>
#endif

void
LALFindChirpACTDFilterSegment (
    LALStatus                  *status,
    SnglInspiralTable         **eventList,
    FindChirpFilterInput       *input,
    FindChirpFilterParams      *params
    );

#if 0
<lalLaTeX>
\newpage\input{XLALFindChirpACTDApplyConstraintC}
</lalLaTeX>
#endif
INT4 XLALFindChirpACTDApplyConstraint(
      INT4                        qidx,
      FindChirpFilterInput      * restrict input,
      FindChirpFilterParams     * restrict params );

#if 0
<lalLaTeX>
\newpage\input{XLALFindChirpACTDInnerProductC}
</lalLaTeX>
#endif

REAL4  XLALFindChirpACTDInnerProduct(
    COMPLEX8Vector *a,
    COMPLEX8Vector *b,
    COMPLEX8       *wtilde,
    REAL4           lower,
    REAL4           deltaT,
    UINT4           numPoints
    );

#if 0
<lalLaTeX>
\newpage\input{XLALFindChirpACTDCorrelateC}
</lalLaTeX>
#endif

INT4 XLALFindChirpACTDCorrelate(
               COMPLEX8Vector  *a,
               COMPLEX8Vector  *b,
               REAL4Vector     *out,
               COMPLEX8        *wtilde,
               REAL4            flower,
               REAL4            deltaT,
               UINT4            numPoints
               );

#if 0
<lalLaTeX>
\newpage\input{XLALFindChirpACTDMiniMaxC}
</lalLaTeX>
#endif

INT4 XLALFindChirpACTDMiniMax(
               COMPLEX8Vector  *a1,
               COMPLEX8Vector  *a2,
               COMPLEX8Vector  *b1,
               COMPLEX8Vector  *b2,
               COMPLEX8        *wtilde,
               REAL4           fLow,
               REAL4           deltaT,
               REAL4           *min,
               REAL4           *max
               );



#ifdef  __cplusplus
#pragma {
}
#endif

#endif /* _FINDCHIRPACTDH_H */
