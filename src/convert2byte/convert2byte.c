/****************************************************************
NAME: convert2byte

SYNOPSIS: convert2byte [-look lxs] [-step lxs] [-log <log file>] [-quiet]
                       <infile> <outfile>

DESCRIPTION:
	Convert any ASF image into a viewable byte image.  Allows for
	specification of file size as well as a "look area" in which to
	generate a byte image.

EXTERNAL ASSOCIATES:
    NAME:                USAGE:
    ---------------------------------------------------------------

FILE REFERENCES:
    NAME:                USAGE:
    ---------------------------------------------------------------

PROGRAM HISTORY:
    This program is a merging of amp2img and ui2byte. It converts any ASF image
    file to an image file made up of byte data (excluding complex data).
    Amp2img was a compilation of two programs written by Shusun Li and Tom
    Logan. It fulfilled the original purpose of Tom's with the added flexibility
    of Shusun's. Ui2byte created byte versions of RAMP (AMM-1) imagery.

    VERS:   DATE:    AUTHOR:      PURPOSE:
    ---------------------------------------------------------------
    1.0     2/03     P. Denny     Merge amp2img and ui2byte.

HARDWARE/SOFTWARE LIMITATIONS:
	None known

ALGORITHM DESCRIPTION:

ALGORITHM REFERENCES:

BUGS: None known

*****************************************************************************
*								            *
*   Converts any non-complex data type image to a byte image                *
*   Copyright (C) 2001  ASF Advanced Product Development    	    	    *
*									    *
*   This program is free software; you can redistribute it and/or modify    *
*   it under the terms of the GNU General Public License as published by    *
*   the Free Software Foundation; either version 2 of the License, or       *
*   (at your option) any later version.					    *
*									    *
*   This program is distributed in the hope that it will be useful,	    *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of    	    *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the   	    *
*   GNU General Public License for more details.  (See the file LICENSE     *
*   included in the asf_tools/ directory).				    *
*									    *
*   You should have received a copy of the GNU General Public License       *
*   along with this program; if not, write to the Free Software		    *
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.               *
*									    *
*   ASF Advanced Product Development LAB Contacts:			    *
*	APD E-mail:	apd@asf.alaska.edu 				    *
* 									    *
*	Alaska SAR Facility			APD Web Site:	            *	
*	Geophysical Institute			www.asf.alaska.edu/apd	    *
*       University of Alaska Fairbanks					    *
*	P.O. Box 757320							    *
*	Fairbanks, AK 99775-7320					    *
*									    *
****************************************************************************/

#include "asf.h"
#include "asf_meta.h"

/* For floating point comparisons */
#define MICRON 0.0000001
#define FLOAT_EQUIVALENT(a, b) (abs(a - b) < MICRON ? 1 : 0)

#define WINDOW_SIZE_MULTIPLIER 1

#define VERSION 1.0

/* PROTOTYPES */
void linear_conversion(FILE *fpin, FILE *fpout, meta_parameters *inMeta,
                       meta_parameters *outMeta);
void multilook(FILE *fpin, FILE *fpout,
               meta_parameters *inMeta, meta_parameters *outMeta,
               int lookLine, int lookSample,
               int stepLine, int stepSample);


int main(int argc, char **argv)
{
	char inFileName[256];           /* Input data file name               */
	char inMetaFileName[256];       /* Input metadata file name           */
	char outFileName[256];          /* Output data file name              */
	char outMetaFileName[256];      /* Output metadata file name          */
	int defaultLookStep_flag=TRUE;  /* Use meta look line & sample?       */
	int multilook_flag=FALSE;       /* Multilook the data or not          */
	int stepLine, stepSample;       /* Step line/samp for multilooking    */
	int lookLine, lookSample;       /* Look line/samp for multilooking    */
	meta_parameters *inMeta;        /* Input metadata structure pointer   */
	meta_parameters *outMeta;       /* Output metadata structure pointer  */
	FILE *inFilePtr;                /* File pointer for input data        */
	FILE *outFilePtr;               /* File pointer for output data       */
	extern int currArg;             /* Pre-initialized to 1               */

/* parse command line */
	logflag=quietflag=FALSE;
	while (currArg < (argc-2)) {
		char *key = argv[currArg++];
		if (strmatch(key,"-multilook")) {
			multilook_flag=TRUE;
		}
		else if (strmatch(key,"-look")) {
			CHECK_ARG(1)
			if (2!=sscanf(GET_ARG(1),"%dx%d",&lookLine,&lookSample)) {
				printf("**ERROR: '%s' does not look like a line x sample (e.g. '5x1').\n",GET_ARG(1));
				usage(argv[0]);
			}
			multilook_flag=TRUE;
       			defaultLookStep_flag=FALSE;
		}
		else if (strmatch(key,"-step")) {
			CHECK_ARG(1)
			if (2!=sscanf(GET_ARG(1),"%dx%d",&stepLine,&stepSample)) {
				printf("**ERROR: '%s' does not look like a line x sample (e.g. '5x1').\n",GET_ARG(1));
				usage(argv[0]);
			}
			multilook_flag=TRUE;
			defaultLookStep_flag=FALSE;
		}
		else if (strmatch(key,"-log")) {
			CHECK_ARG(1);
			strcpy(logFile,GET_ARG(1));
			fLog = FOPEN(logFile, "a");
			logflag=TRUE;
		}
		else if (strmatch(key,"-quiet")) {
			quietflag=TRUE;
		}
		else {printf( "\n**Invalid option:  %s\n",argv[currArg-1]); usage(argv[0]);}
	}
	if ((argc-currArg) < 2) {printf("Insufficient arguments.\n"); usage(argv[0]);}

	strcpy(inFileName, argv[currArg++]);
	create_name(inMetaFileName,inFileName,".meta");

	strcpy(outFileName,argv[currArg]);
	if (!findExt(outFileName))
		strncat(outFileName,".img",256);
	create_name(outMetaFileName,outFileName,".meta");

	StartWatch();
	printf("Date: ");
	fflush(NULL);
	system("date");
	printf("Program: convert2byte\n\n");
	if (logflag) {
		StartWatchLog(fLog);
		fprintf(fLog, "Program: convert2byte\n\n");
		fflush(fLog);
	}

/* Get metadata */
	inMeta = meta_read(inMetaFileName);
	if (inMeta->general->data_type==BYTE) {
		printf("Data type is already byte... Exiting.\n");
		if (logflag) {
			fprintf(fLog,"Data type is already byte... Exiting.\n");
			FCLOSE(fLog);
		}
		return 0;
	}

/* Get statistics for input data (all this -quiet and -log flag business makes
 * it awfully messy, but hey, whachya gonna do? */
	if (!inMeta->stats) {
		char command[1024];
		
		if (!quietflag)
			printf(" There is no statistics structure in the meta file.\n"
			       " To fix this the stats program will be run...\n");
		if (!quietflag && logflag) {
			fprintf(fLog,
			       " There is no statistics structure in the meta file.\n"
			       " To fix this the stats program will be run...\n");
			fflush(fLog);
		}
		sprintf(command,"stats -nostat");
		if (quietflag) sprintf(command,"%s -quiet",command);
		if (logflag)   sprintf(command,"%s -log %s",command,logFile);
		sprintf(command,"%s %s",command,inFileName);

		if (!quietflag)
			printf(" Running command line:  %s\n",command);
		if (!quietflag && logflag) {
			fprintf(fLog," Running command line:  %s\n",command);
			fflush(fLog);
		}
		system(command);

		meta_free(inMeta);
		inMeta = meta_read(inMetaFileName);
	}

	/* if a mask was used in prior stats, move them, and get new stats */
	if (inMeta->stats->mask == inMeta->stats->mask) {
		char command[1024];
		
		if (!quietflag) 
			printf(" It appears that a mask was used in prior statisticas calculations\n"
			       " This program needs statistics without a mask, let's rectify that...\n"
			       " Moving %s to %s.old...\n",
				inMetaFileName, inMetaFileName);
		if (!quietflag && logflag) {
			fprintf(fLog,
			       " It appears that a mask was used in prior statisticas calculations\n"
			       " This program needs statistics without a mask, let's rectify that...\n"
			       " Moving %s to %s.old...\n",
				inMetaFileName, inMetaFileName);
			fflush(fLog);
		}
		sprintf(command,"mv %s %s.old", inMetaFileName, inMetaFileName);
		system(command);

		sprintf(command,"stats -nostat");
		if (quietflag) sprintf(command,"%s -quiet",command);
		if (logflag)   sprintf(command,"%s -log %s",command,logFile);
		sprintf(command,"%s %s",command,inFileName);

		if (!quietflag)
			printf(" Running command line:  %s\n",command);
		if (!quietflag && logflag) {
			fprintf(fLog," Running command line:  %s\n",command);
			fflush(fLog);
		}
		system(command);

		meta_free(inMeta);
		inMeta = meta_read(inMetaFileName);
	}

/* Prepare output meta data for processing & writing */
	outMeta = meta_copy(inMeta);
	outMeta->general->data_type = BYTE;

/* Figure multilooking parameters if necessary */
	if (multilook_flag && defaultLookStep_flag) {
		/* We don't want to multilook any image twice */
		if (inMeta->sar->line_increment==inMeta->sar->look_count)
			stepLine=stepSample=lookLine=lookSample=1;
		else {
			stepLine = inMeta->sar->look_count;
			stepSample = 1;
			lookLine = WINDOW_SIZE_MULTIPLIER * stepLine;
			lookSample = WINDOW_SIZE_MULTIPLIER * stepSample;
			outMeta->general->sample_count = 
				inMeta->general->sample_count / stepSample;
			outMeta->general->line_count = 
				inMeta->general->line_count / stepLine;
			outMeta->sar->line_increment *= stepLine;
			outMeta->sar->sample_increment *= stepSample;
			outMeta->general->x_pixel_size *= stepSample;
			outMeta->general->y_pixel_size *= stepLine;
		}
	}

/* Read data and convert it to byte data */
	inFilePtr = fopenImage(inFileName, "r");
	outFilePtr = FOPEN(outFileName, "w");
	if (multilook_flag) {
		multilook(inFilePtr, outFilePtr, inMeta, outMeta,
		          lookLine, lookSample, stepLine,stepSample);
	}
	else {
		linear_conversion(inFilePtr, outFilePtr, inMeta, outMeta);
	}

	FCLOSE(inFilePtr);
	FCLOSE(outFilePtr);

	meta_write(outMeta,outMetaFileName);

	StopWatch();
	if (logflag) StopWatchLog(fLog);
	if (fLog) FCLOSE(fLog);

	return 0;
}


void usage(char *name) {
 printf("\n"
	"USAGE:\n"
	"   %s [-look lxs] [-step lxs] [-log log_file] [-quiet]\n"
	"                <infile> <outfile>\n",name);
 printf("\n"
	"REQUIRED ARGUMENTS:\n"
	"   infile   Image file to be read in(WITH extension)\n"
	"              accompanied by a .meta file.\n"
	"   outfile  Output image file which will be byte data.\n"
	"              (No extension necessary.)\n");
 printf("\n"
	"OPTIONAL ARGUMENTS:\n"
	"   -look lxs      change number of look lines and samples (default = 5x1).\n"
	"   -step lxs      change number of step lines and samples (default = 5x1).\n"
	"   -log log_file  allows the output to be written to a log file\n"
	"   -quiet         Suppress terminal output.\n");
 printf("\n"
	"DESCRIPTION:\n"
	"   Converts any ASF image into a byte image.\n");
 printf("\n"
	"Version %.2f, ASF SAR Tools\n"
	"\n",VERSION);
 exit(EXIT_FAILURE);
}
