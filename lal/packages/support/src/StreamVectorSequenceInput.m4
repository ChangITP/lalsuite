dnl $Id$
/******************** <lalVerbatim file="StreamVectorSequenceInputCV">
Author: Creighton, T. D.
$Id$
**************************************************** </lalVerbatim> */

/********************************************************** <lalLaTeX>

\subsection{Module \texttt{StreamVectorSequenceInput.c}}
\label{ss:StreamVectorSequenceInput.c}

Reads the entire contents of an input stream into a vector sequence.

\subsubsection*{Prototypes}
\vspace{0.1in}
\input{StreamVectorSequenceInputCP}
\idx{LALCHARReadVectorSequence()}
\idx{LALI2ReadVectorSequence()}
\idx{LALI4ReadVectorSequence()}
\idx{LALI8ReadVectorSequence()}
\idx{LALU2ReadVectorSequence()}
\idx{LALU4ReadVectorSequence()}
\idx{LALU8ReadVectorSequence()}
\idx{LALSReadVectorSequence()}
\idx{LALDReadVectorSequence()}

\subsubsection*{Description}

These routines read data from the I/O stream \verb@*stream@ until the
end-of-input is reached.  Each line is stored as a data vector, and
the vectors are combined into a LAL vector sequence structure
\verb@**sequence@.  Each line vector is padded with zeros to match the
length of the longest line.  The routine passes back a pointer to the
new structure.

The routine \verb@LALCHARReadVectorSequence()@ essentially stores an
image of the I/O stream as a sequence of lines padded with \verb@'\0'@
characters.  However, it will skip over any empty lines, which occur,
for instance, when the end-of-input or a null character \verb@'\0'@
occurs immediately following a newline character \verb@'\n'@.  The
numeric routines will additionally skip blank lines, comment lines, or
other input lines that have no parseable numbers in them.  (As with
the routines in \verb@StreamVectorInput.c@, comment in sindicated by a
\verb@#@ sign at the beginning of a line or a \verb@%@ sign anywhere
in the line, signifying that the remainder of the line is to be
ignored.)  However, if an input line contains \emph{any} parseable
data, then the corresponding vector in the vector sequence will be
allocated (and padded with zeros, if it is shorter than the longest
line).

\subsubsection*{Algorithm}

These functions first create a linked list of vectors, using the
routines in \verb@StreamVectorInput.c@ to read them in.  Once the list
is complete, the longest vector length is determined, and the vector
sequence is created and filled.

The numeric routines skip over blank, comment, or otherwise
unparseable lines by catching and handling the \verb@LEN@ error code
generated by the vector input routine.  However, it is worth pointing
out that the vector input routine will have generated an error message
if the error reporting bit in \verb@lalDebugLevel@ was set.  The
vector sequence input routines will therefore generate a followup
messages indicating that the preceding error was successfully dealt
with.  So you may see pairs of \verb@ABORT:@ and \verb@CONTINUE:@
error messages when reading files containing blank or comment lines.

\subsubsection*{Uses}
\begin{verbatim}
LALCalloc()         LALFree()
LALCHARReadVector() LALCHARDestroyVector() LALCHARCreateVectorSequence()
LALI2ReadVector()   LALI2DestroyVector()   LALI2CreateVectorSequence()
LALI4ReadVector()   LALI4DestroyVector()   LALI4CreateVectorSequence()
LALI8ReadVector()   LALI8DestroyVector()   LALI8CreateVectorSequence()
LALU2ReadVector()   LALU2DestroyVector()   LALU2CreateVectorSequence()
LALU4ReadVector()   LALU4DestroyVector()   LALU4CreateVectorSequence()
LALU8ReadVector()   LALU8DestroyVector()   LALU8CreateVectorSequence()
LALSReadVector()    LALSDestroyVector()    LALSCreateVectorSequence()
LALDReadVector()    LALDDestroyVector()    LALDCreateVectorSequence()
\end{verbatim}

\subsubsection*{Notes}

\vfill{\footnotesize\input{StreamVectorSequenceInputCV}}

******************************************************* </lalLaTeX> */

#include <lal/LALStdlib.h>
#include <lal/AVFactories.h>
#include <lal/SeqFactories.h>
#include <lal/StreamInput.h>

NRCSID( STREAMVECTORSEQUENCEINPUTC, "$Id$" );

/* Define linked-list of pointers to vectors of arbitrary type. */
typedef union tagVector {
  CHARVector *CHV;
  INT2Vector *I2V;
  INT4Vector *I4V;
  INT8Vector *I8V;
  UINT2Vector *U2V;
  UINT4Vector *U4V;
  UINT8Vector *U8V;
  REAL4Vector *SV;
  REAL8Vector *DV;
} Vector;
typedef struct tagVectorList {
  Vector vector;
  struct tagVectorList *next;
} VectorList;

static const VectorList empty;


#define FREECHARVECTORLIST                                           \
do {                                                                 \
  if ( head.vector.CHV ) {                                           \
    TRY( LALCHARDestroyVector( stat->statusPtr,                      \
			     &(head.vector.CHV) ), stat );           \
  }                                                                  \
  here = head.next;                                                  \
  while ( here ) {                                                   \
    VectorList *nextPtr = here->next;                                \
    if ( here->vector.CHV ) {                                        \
      TRY( LALCHARDestroyVector( stat->statusPtr,                    \
			       &(here->vector.CHV) ), stat );        \
    }                                                                \
    LALFree( here );                                                 \
    here = nextPtr;                                                  \
  }                                                                  \
} while (0)

/* <lalVerbatim file="StreamVectorSequenceInputCP"> */
void
LALCHARReadVectorSequence( LALStatus          *stat,
			   CHARVectorSequence **sequence,
			   FILE               *stream )
{ /* </lalVerbatim> */
  VectorList head = empty;   /* head of linked list of vectors */
  VectorList *here;          /* pointer to current position in list */
  CHAR *data;                /* pointer to vector data */
  UINT4 nRows, nCols;        /* number and length of lines */
  CreateVectorSequenceIn in; /* parameters for creating sequence */

  INITSTATUS( stat, "LALCHARReadVectorSequence",
	      STREAMVECTORSEQUENCEINPUTC );
  ATTATCHSTATUSPTR( stat );

  /* Read the first line. */
  if ( !feof( stream ) ) {
    TRY( LALCHARReadVector( stat->statusPtr, &(head.vector.CHV), stream ),
	 stat );
  }
  here = &head;

  /* As long as lines remain... */
  while ( !feof( stream ) ) {

    /* Allocate space for next line. */
    here->next = (VectorList *)LALCalloc( 1, sizeof(VectorList) );
    if ( !here->next ) {
      FREECHARVECTORLIST;
      ABORT( stat, STREAMINPUTH_EMEM, STREAMINPUTH_MSGEMEM );
    }

    /* Read in next line. */
    here = here->next;
    LALCHARReadVector( stat->statusPtr, &(here->vector.CHV), stream );
    BEGINFAIL( stat ) {
      FREECHARVECTORLIST;
      ABORT( stat, STREAMINPUTH_EMEM, STREAMINPUTH_MSGEMEM );
    } ENDFAIL( stat );
  }

  /* Lines have been read.  Now determine the maximum line length, and
     allocate the vector sequence.  Ignore lines containing only a
     single '\0' character. */
  nRows = nCols = 0;
  here = &head;
  while ( here ) {
    if ( here->vector.CHV->length > 1 ) {
      nRows++;
      if ( here->vector.CHV->length > nCols )
	nCols = here->vector.CHV->length;
    }
    here = here->next;
  }
  in.length = nRows;
  in.vectorLength = nCols;
  LALCHARCreateVectorSequence( stat->statusPtr, sequence, &in );
  BEGINFAIL( stat ) {
    FREECHARVECTORLIST;
    ABORT( stat, STREAMINPUTH_EMEM, STREAMINPUTH_MSGEMEM );
  } ENDFAIL( stat );

  /* Now assign data to the sequence, padding with zeros as
     necessary. */
  here = &head;
  data = (*sequence)->data;
  while ( here ) {
    UINT4 length = here->vector.CHV->length;
    if ( length > 1 ) {
      memcpy( data, here->vector.CHV->data, length );
      if ( nCols - length > 0 )
	memset( data + length, 0, nCols - length );
      data += nCols;
    }
    here = here->next;
  }

  /* Free memory and exit. */
  FREECHARVECTORLIST;
  DETATCHSTATUSPTR( stat );
  RETURN( stat );
}

define(`TYPECODE',`I2')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`I4')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`I8')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`U2')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`U4')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`U8')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`S')dnl
include(`LALReadVectorSequence.m4')dnl

define(`TYPECODE',`D')dnl
include(`LALReadVectorSequence.m4')dnl
