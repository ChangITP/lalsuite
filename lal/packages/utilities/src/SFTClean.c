/*
 *  Copyright (C) 2005 Badri Krishnan, Alicia Sintes  
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
 * \author Badri Krishnan, Alicia Sintes, Greg Mendell
 * \file SFTClean.c
 * \brief Module containing routines for dealing with spectral disturbances in SFTs

   \par Description 
   
   This module contains routines for dealing with lists of known spectral disturbances 
   in the frequency domain, and using them to clean SFTs.  

   The basic input is a text file containing a list of known spectral lines.  An example 
   is the following

   \verbatim
   0.0      0.25     4000     0.0        0.0   0.25Hzlines  
   0.0      60.0     20       0.5        0.5   60Hzlines     
   0.0      16.0     100      0.0        0.0   16Hzlines   
   166.7    0.0      1        0.0        0.0   Calibrationline 
   345.0    0.0      1        3.0        3.0   violinmodes   
   \endverbatim

   The file consists of rows with 6 columns each.  Each row has information about
   a set of spectral lines of the form \f$ f_n = f_0 + n\Delta f \f$.  The first column 
   is the start frequency \f$ f_0 \f$, the second column is the spacing \f$ \Delta f \f$,
   the third column is the total number of lines, the fourth column is the 
   left-width of each line (in Hz), the fifth column is the width on the right, and
   the last column is a brief comment string (no spaces).  If this file is meant to
   be used for cleaning SFTs, then certain features which the user must be aware of
   are explained in the documentation of the function LALCleanCOMPLEX8SFT().  

   \par Uses
   \code
   void LALFindNumberHarmonics (LALStatus, LineHarmonicsInfo, CHAR)
   void LALReadHarmonicsInfo (LALStatus, LineHarmonicsInfo, CHAR)
   void LALHarmonics2Lines (LALStatus, LineNoiseInfo, LineHarmonicsInfo)
   void LALChooseLines (LALStatus, LineNoiseInfo, LineNoiseInfo, REAL8, REAL8)
   void LALCheckLines ( LALStatus, INT4, LineNoiseInfo, REAL8)
   void LALFindNumberLines (LALStatus, LineNoiseInfo, CHAR)
   void LALReadLineInfo (LALStatus, LineNoiseInfo, CHAR)
   void LALCleanCOMPLEX8SFT (LALStatus, SFTtype, INT4, INT4, LineNoiseInfo, RandomParams)
   \endcode


*/

/************************************ <lalVerbatim file="SFTCleanCV">
Author: Sintes, A.M., Krishnan, B.
$Id$
************************************* </lalVerbatim> */

/* REVISIONS: */
/* 09/09/05 gam; if (nLinesOut == 0) still need outLine->nLines = nLinesOut; calling function needs to know this */
/* 09/09/05 gam; make RandomParams *randPar a parameter for CleanCOMPLEX8SFT. Thus only need to */
/*               initialze RandomParams *randPar once and avoid repeatly opening /dev/urandom.  */
/* 09/09/05 gam; prefix function names with LAL in init status macros */
/* 09/09/05 gam; only assert harmonicInfo and fname in LALFindNumberHarmonic and fix assert of fp. */
/*               Other pointers can be unititialized until nHarmonicSets is determined.            */

/* <lalLaTeX>  *******************************************************
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\section{Module \texttt{SFTClean.c}}
\label{ss:SFTClean.c}
Routines for reading SFT binary files

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\subsubsection*{Prototypes}
\vspace{0.1in}
\input{SFTCleanD}
\index{\verb&ReadSFTCleanHeader1()&}
\index{\verb&ReadCOMPLEX8SFTCleanData1()&}
\index{\verb&ReadCOMPLEX16SFTCleanData1()&}
\index{\verb&COMPLEX8SFT2Periodogram1()&}
\index{\verb&COMPLEX16SFT2Periodogram1()&}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\subsubsection*{Description}

SFT cleaning routines 


%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\subsubsection*{Uses}
\begin{verbatim}
LALHO()
\end{verbatim}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\subsubsection*{Notes}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
\vfill{\footnotesize\input{SFTCleanCV}}
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

*********************************************** </lalLaTeX> */


#include <lal/SFTClean.h>



NRCSID (SFTCLEANC, "$Id$");

/*
 * The functions that make up the guts of this module
 */



/**
 * Looks into the input file containing list of lines, does some checks on the 
 * file format, and calculates the number of harmonic sets in this file.  
 */
/*<lalVerbatim file="SFTCleanD"> */
void LALFindNumberHarmonics (LALStatus    *status,
			     LineHarmonicsInfo   *harmonicInfo, /**< list of harmonics */
			     CHAR         *fname /**< input filename */)
{/*   *********************************************  </lalVerbatim> */

  FILE *fp = NULL;
  CHAR  dump[128];
  INT4   harmonicCount, r, tempint; 
  REAL8 temp1, temp2, temp3, temp4;   


  INITSTATUS (status, "LALFindNumberHarmonics", SFTCLEANC);
  ATTATCHSTATUSPTR (status);

  /* make sure arguments are not null */
  ASSERT (harmonicInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL ); 
  /* ASSERT (harmonicInfo->nHarmonicSets > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL); 
  ASSERT (harmonicInfo->startFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicInfo->gapFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicInfo->numHarmonics, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (harmonicInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); */ /* 09/09/05 gam */
  ASSERT (fname, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL ); 

  /* open harmonics file for reading */
  fp = fopen( fname, "r");
  /* ASSERT (fname, status, SFTCLEANH_EFILE, SFTCLEANH_MSGEFILE); */ /* 09/09/05 gam */
  ASSERT (fp, status, SFTCLEANH_EFILE, SFTCLEANH_MSGEFILE);

  harmonicCount = 0;

  do {
    r=fscanf(fp,"%lf%lf%d%lf%lf%s\n", &temp1, &temp2, 
	     &tempint, &temp3, &temp4, dump);
    /* make sure the line has the right number of entries or is EOF */
    ASSERT( (r==6)||(r==EOF), status, SFTCLEANH_EHEADER, SFTCLEANH_MSGEVAL);
    if (r==6) harmonicCount++;
  } while ( r != EOF);

  harmonicInfo->nHarmonicSets = harmonicCount;

  fclose(fp);

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);
}


/** Reads in the contents of the input line-info file and fills up
 *  the LineHarmonicsInfo structure.  Appropriate memory must be allocated for
 *  this structure before this function is called.  
 */
/*  <lalVerbatim file="SFTCleanD"> */
void  LALReadHarmonicsInfo (LALStatus          *status,
			    LineHarmonicsInfo  *harmonicsInfo, /**< list of harmonics */
			    CHAR               *fname /**< input file */)  
{/*   *********************************************  </lalVerbatim> */
  /* this reads the information about the lines: central frequency, left wing and 
     right wing */
  FILE    *fp = NULL;
  INT4    r, count, nHarmonicSets;
  REAL8   *startFreq=NULL;
  REAL8   *gapFreq=NULL;
  INT4    *numHarmonics=NULL;
  REAL8   *leftWing=NULL;
  REAL8   *rightWing=NULL;
  CHAR    dump[128];

  INITSTATUS (status, "LALReadHarmonicsInfo", SFTCLEANC);
  ATTATCHSTATUSPTR (status);  

  /* make sure arguments are not null */
  ASSERT (harmonicsInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (harmonicsInfo->nHarmonicSets > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (harmonicsInfo->startFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->gapFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->numHarmonics, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (harmonicsInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (fname, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
 
  /* open line noise file for reading */
  fp = fopen( fname, "r");
  ASSERT (fp, status, SFTCLEANH_EFILE,  SFTCLEANH_MSGEFILE);

  nHarmonicSets = harmonicsInfo->nHarmonicSets;
  startFreq = harmonicsInfo->startFreq;
  gapFreq = harmonicsInfo->gapFreq;
  numHarmonics = harmonicsInfo->numHarmonics; 
  leftWing = harmonicsInfo->leftWing;
  rightWing = harmonicsInfo->rightWing;

  /* read line information from file */
  for (count = 0; count < nHarmonicSets; count++){
    r=fscanf(fp,"%lf%lf%d%lf%lf%s\n", startFreq+count, gapFreq+count, numHarmonics+count, 
	     leftWing+count, rightWing+count, dump);
    ASSERT(r==6, status, SFTCLEANH_EHEADER, SFTCLEANH_MSGEVAL);
  }

  fclose(fp);

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);

}

/** Converts the list of harmonic sets into an explicit list of spectral 
 *  lines.
 */
/*  <lalVerbatim file="SFTCleanD"> */
void  LALHarmonics2Lines (LALStatus          *status,
			  LineNoiseInfo      *lineInfo, /**< output list of explicit lines */
			  LineHarmonicsInfo  *harmonicsInfo) /**< input list of harmonics */
{/*   *********************************************  </lalVerbatim> */
  /* this reads the information about the lines: central frequency, left wing and 
     right wing */
  
  INT4    count1, count2, nHarmonicSets, maxCount, position;
  REAL8   *startFreq;
  REAL8   *gapFreq;
  INT4    *numHarmonics;
  REAL8   *leftWing;
  REAL8   *rightWing;
  REAL8   f0, deltaf, leftDeltaf, rightDeltaf;


  INITSTATUS (status, "LALHarmonics2Lines", SFTCLEANC);
  ATTATCHSTATUSPTR (status);  

  /* make sure arguments are not null */
  ASSERT (harmonicsInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (harmonicsInfo->nHarmonicSets > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (harmonicsInfo->startFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->gapFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->numHarmonics, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (harmonicsInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (harmonicsInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 

  ASSERT (lineInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->nLines > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL); 
  ASSERT (lineInfo->lineFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
 
  nHarmonicSets = harmonicsInfo->nHarmonicSets;
  startFreq = harmonicsInfo->startFreq;
  gapFreq = harmonicsInfo->gapFreq;
  numHarmonics = harmonicsInfo->numHarmonics; 
  leftWing = harmonicsInfo->leftWing;
  rightWing = harmonicsInfo->rightWing;

  position = 0;
  for (count1=0; count1 < nHarmonicSets; count1++)
    {
      maxCount = *(numHarmonics + count1);
      f0 = *(startFreq + count1);
      deltaf = *(gapFreq + count1);
      leftDeltaf = *(leftWing + count1);
      rightDeltaf = *(rightWing + count1);
      for (count2 = 0; count2 < maxCount; count2++)
	{
	  *(lineInfo->lineFreq + count2 + position) = f0 + count2 * deltaf;
	  *(lineInfo->leftWing + count2 + position) = leftDeltaf;
	  *(lineInfo->rightWing + count2 + position) = rightDeltaf;
	}
      position += maxCount;
    }


  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);

}




/** 
 * Finds total number of spectral-lines contained in case the input file is 
 * a list of explicit spectral lines -- obsolete.  
 * Use instead LALFindNumberHarmonics(). 
 */
/*  <lalVerbatim file="SFTCleanD"> */
void LALFindNumberLines (LALStatus          *status,
		      LineNoiseInfo      *lineInfo,            
		      CHAR               *fname)
{/*   *********************************************  </lalVerbatim> */
  /* this function counts the number of lines present in the file "fname" and  
     checks that the format of the lines is correct */

  FILE *fp = NULL;
  REAL8 temp1,temp2,temp3;
  INT4  lineCount, r;
  CHAR  dump[128]; 

  INITSTATUS (status, "LALFindNumberLines", SFTCLEANC);
  ATTATCHSTATUSPTR (status);  

  /* make sure arguments are not null */
  ASSERT (lineInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (fname, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);

  /* open line noise file for reading */
  fp = fopen( fname, "r");
  ASSERT (fp, status, SFTCLEANH_EFILE,  SFTCLEANH_MSGEFILE);

  lineCount = 0;
  do {
    r=fscanf(fp,"%lf%lf%lf%s\n", &temp1, &temp2, &temp3, dump);
    /* make sure the line has the right number of entries or is EOF */
    ASSERT( (r==4)||(r==EOF), status, SFTCLEANH_EHEADER, SFTCLEANH_MSGEVAL);
    if (r==4) lineCount++;
  } while ( r != EOF);

  lineInfo->nLines = lineCount;

  fclose(fp);

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);
}

/** Reads information from file containing list of explicit lines - obsolete. 
 *  Use instead LALReadHarmonicsInfo()  
 */
/*  <lalVerbatim file="SFTCleanD"> */
void  LALReadLineInfo (LALStatus     *status,
		    LineNoiseInfo *lineInfo,  
		    CHAR          *fname)
{/*   *********************************************  </lalVerbatim> */
  /* this reads the information about the lines: central frequency, left wing and 
     right wing */
  FILE    *fp = NULL;
  INT4    r, count, nLines;
  REAL8   *lineFreq=NULL;
  REAL8   *leftWing=NULL;
  REAL8   *rightWing=NULL;
  CHAR    dump[128];

  INITSTATUS (status, "LALReadLineInfo", SFTCLEANC);
  ATTATCHSTATUSPTR (status);  

  /* make sure arguments are not null */
  ASSERT (lineInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->nLines > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (lineInfo->lineFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (fname, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
 
  /* open line noise file for reading */
  fp = fopen( fname, "r");
  ASSERT (fp, status, SFTCLEANH_EFILE,  SFTCLEANH_MSGEFILE);

  nLines = lineInfo->nLines;
  lineFreq = lineInfo->lineFreq;
  leftWing = lineInfo->leftWing;
  rightWing = lineInfo->rightWing;
  /* read line information from file */
  for (count = 0; count < nLines; count++){
    r=fscanf(fp,"%lf%lf%lf%s\n", lineFreq+count, leftWing+count, rightWing+count, dump);
    ASSERT(r==4, status, SFTCLEANH_EHEADER, SFTCLEANH_MSGEVAL);
  }

  fclose(fp);

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);

}


/** Takes a set of spectral lines and a frequency range as input and 
 * returns those lines which lie within the specified frequency range.  The output
 * is a reduced list of spectral lines which either lie completely within the 
 * frequency range or whose wings overlap with the frequency range.  This is useful
 * for discarding unnecessary lines to save computational cost and memory. 
 */
/*  <lalVerbatim file="SFTCleanD"> */
void LALChooseLines (LALStatus        *status,
		     LineNoiseInfo    *outLine,  /**< reduced list of lines */
		     LineNoiseInfo    *inLine,   /**< input list of lines */
		     REAL8            fMin,      /**< start of frequency band */
		     REAL8            fMax       /**< end of frequency band */
		  )
{/*   *********************************************  </lalVerbatim> */

  INT4 nLinesIn, nLinesOut, j;
  REAL8 lineFreq, leftWing, rightWing;

  INITSTATUS (status, "LALChooseLines", SFTCLEANC);
  ATTATCHSTATUSPTR (status);   

  /* some sanity checks */
  ASSERT (outLine, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (inLine, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (inLine->nLines > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (outLine->nLines > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (inLine->nLines - outLine->nLines == 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (fMin > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (fMax > fMin, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);

  nLinesIn = inLine->nLines;
  nLinesOut = 0;  
  /* loop over lines in inLine structure and see if they are within bounds */
  for(j=0; j<nLinesIn; j++)
    {
      lineFreq = inLine->lineFreq[j];
      leftWing = inLine->leftWing[j];
      rightWing = inLine->rightWing[j];
      if ( (lineFreq >= fMin) && (lineFreq <= fMax))
	{
	  outLine->lineFreq[nLinesOut] = lineFreq;
	  outLine->leftWing[nLinesOut] = leftWing;
	  outLine->rightWing[nLinesOut] = rightWing;
	  nLinesOut++;
	} 
      else if ( (lineFreq < fMin) && (lineFreq + rightWing > fMin) )
	{
	  outLine->lineFreq[nLinesOut] = lineFreq;
	  outLine->leftWing[nLinesOut] = leftWing;
	  outLine->rightWing[nLinesOut] = rightWing;
	  nLinesOut++;
	}
      else if ( (lineFreq > fMax) && (lineFreq - leftWing < fMax) )
	{
	  outLine->lineFreq[nLinesOut] = lineFreq;
	  outLine->leftWing[nLinesOut] = leftWing;
	  outLine->rightWing[nLinesOut] = rightWing;
	  nLinesOut++;
	}
    }

  /* if there are no lines inband then free memory */
  if (nLinesOut == 0)
    {
      outLine->nLines = nLinesOut; /* 09/09/05 gam; calling function needs to know this. */
      LALFree(outLine->lineFreq);
      LALFree(outLine->leftWing);
      LALFree(outLine->rightWing);
    }
  else  /* else reallocate memory for outLine */
    {
      outLine->nLines = nLinesOut;
      outLine->lineFreq = (REAL8 *)LALRealloc(outLine->lineFreq, nLinesOut*sizeof(REAL8));
      outLine->leftWing = (REAL8 *)LALRealloc(outLine->leftWing, nLinesOut*sizeof(REAL8));
      outLine->rightWing = (REAL8 *)LALRealloc(outLine->rightWing, nLinesOut*sizeof(REAL8));
    }

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);
}

#define TRUE (1==1)
#define FALSE (1==0)

/** Function to count how many lines affect a given frequency.  Input is a 
 * list of lines and a frequency.  The output is an integer which is equal to the 
 * number of lines which intersect this frequency.  An output of zero indicates
 * that the frequencty is not influenced by the lines.  Note that the doppler
 * broadening of the lines is taken into account while deciding whether the 
 * frequency is affected or not. 
 */
/*  <lalVerbatim file="SFTCleanD"> */
void LALCheckLines ( LALStatus           *status,
		     INT4                *countL, /**< number of lines affecting frequency */
		     LineNoiseInfo       *lines, /**< list of lines */
		     REAL8               freq)   /**< frequency to be checked */
{/*   *********************************************  </lalVerbatim> */

  INT4 nLines, j;
  REAL8 lineFreq, leftWing, rightWing, doppler;

  INITSTATUS (status, "LALCheckLines", SFTCLEANC);
  ATTATCHSTATUSPTR (status);   

  /* sanity checks */
  ASSERT (lines, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (countL, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (lines->nLines > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);

  /* loop over lines and check if freq is affected by the lines */
  nLines = lines->nLines;
  *countL = 0;
  for (j=0; j<nLines; j++)
    {
      lineFreq = lines->lineFreq[j];
      leftWing = lines->leftWing[j];
      rightWing = lines->rightWing[j];
      /* add doppler band to wings */
      doppler = VTOT * (lineFreq + rightWing);
      leftWing += doppler;
      rightWing += doppler;
      /* now chech if freq lies in range */
      if ( (freq <= lineFreq + rightWing) && (freq >= lineFreq - leftWing))
	*countL += 1;  
    }
      
  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);
}


/**
 * Function for cleaning a SFT given a set of known spectral disturbances.  
 * The algorithm is the following.  For each 
 * spectral line, first the central frequency of the line is converted to a 
 * bin index by floor(tBase * freq + 0.5).  If the wings are set to zero, then only this 
 * central bin is cleaned.  Note that if the frequency lies between two exactly 
 * resolved frequencies, then only one of these bins is cleaned.  The user must 
 * know about the SFT timebase and make sure that the central frequency is an
 * exactly resolved one.  The wingsize is calculated in bins according to 
 * floor(tBase * wingsize).  This is done separately for the left and right wings.  
 * Note the use of the floor function.  If the wingsize corresponds to say 2.5 bins, then
 * only 2 bins will be cleaned in addition to the central frequency.  
 *
 * Having decided which bins ar eto be cleaned, the next step is to produce random noise 
 * to replace the data in these bins. The fake random noise must mimic the 
 * behavior of the true noise in the vicinity of the spectral disturbance, and we must 
 * therefore estimate the noise floor in the vicinity of the disturbance.  
 * The user specifies a "window size" for cleaning,  and this determines how many data 
 * points from the SFT are to be used for estimating this noise floor.  
 * Consider the number of frequency bins on each side of the spectral disturbance  
 * (excluding the wings) given by the user specified window size.  The function calculates 
 * the SFT power in these bins and calculates their median.  The median is then converted
 * to a standard deviation. 
 */
/* *******************************  <lalVerbatim file="SFTCleanD"> */
void LALCleanCOMPLEX8SFT (LALStatus          *status,
			  SFTtype            *sft,  /**< SFT to be cleaned */
			  INT4               width, /**< maximum width to be cleaned -- set sufficiently large if all bins in each line are to be cleaned*/      
			  INT4               window,/**< window size for noise floor estimation in vicinity of a line */
			  LineNoiseInfo      *lineInfo, /**< list of lines */ 
			  RandomParams       *randPar /**< parameters for generating random noise */)
{ /*   *********************************************  </lalVerbatim> */
  /* function to clean the SFT based on the line information read earlier */
  
  INT4     nLines, count, leftCount, rightCount, lineBin, minBin, maxBin, k, tempk;
  INT4     leftWingBins, rightWingBins, length, sumBins;
  REAL8    deltaF, f0, tBase, bias;
  REAL8    stdPow, medianPow;
  REAL8    *tempDataPow=NULL;
  REAL8    *lineFreq=NULL;
  REAL8    *leftWing=NULL;
  REAL8    *rightWing=NULL;
  COMPLEX8 *inData;
  /* FILE *fp=NULL;   
  INT4 seed, ranCount;  
  RandomParams *randPar=NULL; */ /* 09/09/05 gam; randPar now a function argument */
  static REAL4Vector *ranVector=NULL; 
  REAL4 *randVal;
  /* --------------------------------------------- */
  INITSTATUS (status, "LALCleanCOMPLEX8SFT", SFTCLEANC);
  ATTATCHSTATUSPTR (status);   
  
  /*   Make sure the arguments are not NULL: */ 
  ASSERT (sft,   status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  inData = sft->data->data;
  ASSERT (inData, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (lineInfo, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL);
  ASSERT (lineInfo->nLines, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (lineInfo->lineFreq, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->leftWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (lineInfo->rightWing, status, SFTCLEANH_ENULL, SFTCLEANH_MSGENULL); 
  ASSERT (window > 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  ASSERT (width >= 0, status, SFTCLEANH_EVAL, SFTCLEANH_MSGEVAL);
  length = sft->data->length;
  ASSERT (length > 0, status, SFTCLEANH_EHEADER, SFTCLEANH_MSGEVAL);

  /* get the value of RngMedBias from the window size */
  TRY( LALRngMedBias( status->statusPtr, &bias, 2*window ), status ); 
  
  /* copy pointers from input */
  nLines = lineInfo->nLines;
  lineFreq = lineInfo->lineFreq;
  leftWing = lineInfo->leftWing;
  rightWing = lineInfo->rightWing;
  
  deltaF = sft->deltaF;
  tBase = 1.0/deltaF;
  f0 = sft->f0;
  minBin = floor(f0/deltaF + 0.5);
  maxBin = minBin + length - 1;

  /* allocate memory for storing sft power */
  tempDataPow = LALMalloc(2*window*sizeof(REAL8));
  
  /* 09/09/05 gam; randPar now a function argument */
  /* fp=fopen("/dev/urandom", "r");
  ASSERT(fp, status, SFTCLEANH_EFILE, SFTCLEANH_MSGEFILE);  

  ranCount = fread(&seed, sizeof(seed), 1, fp);
  ASSERT(ranCount==1, status, SFTCLEANH_EREAD, SFTCLEANH_MSGEREAD);  

  fclose(fp); */

  /* calculate total number of bins to see how many random numbers must be generated */
  sumBins = 0;
  for (count = 0; count < nLines; count++)
    {
      { 
	INT4 tempSumBins;
	tempSumBins = floor(tBase*leftWing[count]) + floor(tBase*rightWing[count]);  
	sumBins += tempSumBins < 2*width ? tempSumBins : 2*width;
      } 
    }

  /* TRY ( LALCreateRandomParams (status->statusPtr, &randPar, seed), status); */ /* 09/09/05 gam; randPar now a function argument */
  TRY ( LALCreateVector (status->statusPtr, &ranVector, 2*(sumBins + nLines)), status);
  TRY ( LALNormalDeviates (status->statusPtr, ranVector, randPar), status);
  /* TRY ( LALDestroyRandomParams (status->statusPtr, &randPar), status);  */ /* 09/09/05 gam; randPar now a function argument */

  tempk = 0;  
  /* loop over the lines */
  for (count = 0; count < nLines; count++){
    
    /* find frequency bins for line frequency and wings */
    lineBin = floor(tBase * lineFreq[count] + 0.5);
    leftWingBins = floor(tBase * leftWing[count]);
    rightWingBins = floor(tBase * rightWing[count]);
    
    /* check that frequency is within band of sft and line is not too wide*/
    if ((lineBin >= minBin) && (lineBin <= maxBin) && (leftWingBins <= width) && (rightWingBins <= width)){

      /* estimate the sft power in "window" # of bins each side */
      for (k = 0; k < window ; k++){
	if (maxBin - lineBin - rightWingBins - k > 0)
	  inData = sft->data->data + lineBin - minBin + rightWingBins + k + 1;
	else
	  inData = sft->data->data + length - 1;

	tempDataPow[k] = (inData->re)*(inData->re) + (inData->im)*(inData->im);
	
	if (lineBin - minBin -leftWingBins - k > 0)
	  inData = sft->data->data + lineBin - minBin - leftWingBins - k - 1;
	else 
	  inData = sft->data->data;

	tempDataPow[k+window] = (inData->re)*(inData->re) + (inData->im)*(inData->im);   
      }
    
      gsl_sort( tempDataPow, 1, 2*window);
      medianPow = gsl_stats_median_from_sorted_data(tempDataPow, 1, 2*window);
      stdPow = sqrt(medianPow/(2 * bias));
      
      /* set sft value at central frequency to noise */
      inData = sft->data->data + lineBin - minBin;

      randVal = ranVector->data + tempk;  
      inData->re = stdPow * (*randVal); 
      tempk++;

      randVal = ranVector->data + tempk;  
      inData->im = stdPow * (*randVal); 
      tempk++;
    
      /* now go left and set the left wing to noise */
      /* make sure that we are always within the sft band */
      /* and set bins to zero only if Wing width is smaller than "width" */ 
      if ((leftWingBins <= width)){
	for (leftCount = 0; leftCount < leftWingBins; leftCount++){
	  if ( (lineBin - minBin - leftCount > 0)){
	    inData = sft->data->data + lineBin - minBin - leftCount - 1;

	    randVal = ranVector->data + tempk;  
	    inData->re = stdPow * (*randVal); 
	    tempk++;

	    randVal = ranVector->data + tempk;  
	    inData->im = stdPow * (*randVal); 
	    tempk++;
	  }
	}
      }
      
      /* now go right making sure again to stay within the sft band */
      if ((rightWingBins <= width )){
	for (rightCount = 0; rightCount < rightWingBins; rightCount++){
	  if ( (maxBin - lineBin - rightCount > 0)){
	    inData = sft->data->data + lineBin - minBin + rightCount + 1;

	    randVal = ranVector->data + tempk;  
	    inData->re = stdPow * (*randVal); 
            tempk++;

	    randVal = ranVector->data + tempk;  
	    inData->im = stdPow * (*randVal);
	    tempk++;
	  }
	}
      }
    }
  } /* end loop over lines */

  /* free memory */
  LALFree(tempDataPow);
  TRY (LALDestroyVector (status->statusPtr, &ranVector), status);

  DETATCHSTATUSPTR (status);
  /* normal exit */
  RETURN (status);
}









