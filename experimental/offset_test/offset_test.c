/*==================BEGIN ASF AUTO-GENERATED DOCUMENTATION==================*/
/*
ABOUT EDITING THIS DOCUMENTATION:
If you wish to edit the documentation for this program, you need to change the
following defines. For the short ones (like ASF_NAME_STRING) this is no big
deal. However, for some of the longer ones, such as ASF_COPYRIGHT_STRING, it
can be a daunting task to get all the newlines in correctly, etc. In order to
help you with this task, there is a tool, edit_man_header.pl. The tool *only*
works with this portion of the code, so fear not. It will scan in defines of
the format #define ASF_<something>_STRING between the two auto-generated
documentation markers, format them for a text editor, run that editor, allow
you to edit the text in a clean manner, and then automatically generate these
defines, formatted appropriately. The only warning is that any text between
those two markers and not part of one of those defines will not be preserved,
and that all of this auto-generated code will be at the top of the source
file. Save yourself the time and trouble, and use edit_man_header.pl. :)
*/

#define ASF_NAME_STRING \
"offset_test"

#define ASF_USAGE_STRING \
"<file1> <file2> <out>"

#define ASF_DESCRIPTION_STRING #define ASF_INPUT_STRING \
"The basenames of two data files that are to be compared."

#define ASF_OUTPUT_STRING \
"File containing the measured offsets and a level of doubt for these\n"\
"offsets calculated for a regular grid of 20x20 image chips. The level\n"\
"of doubt is calculated by the largest neighbor value over the maximum peak."

#define ASF_OPTIONS_STRING \
"None."

#define ASF_EXAMPLES_STRING \
"offset_test test_old test_new test.out"

#define ASF_LIMITATIONS_STRING \
"None known."

#define ASF_COPYRIGHT_STRING \
"Copyright (c) 2004, Geophysical Institute, University of Alaska Fairbanks\n"\
"All rights reserved.\n"\
"\n"\
"Redistribution and use in source and binary forms, with or without\n"\
"modification, are permitted provided that the following conditions are met:\n"\
"\n"\
"    * Redistributions of source code must retain the above copyright notice,\n"\
"      this list of conditions and the following disclaimer.\n"\
"    * Redistributions in binary form must reproduce the above copyright\n"\
"      notice, this list of conditions and the following disclaimer in the\n"\
"      documentation and/or other materials provided with the distribution.\n"\
"    * Neither the name of the Geophysical Institute nor the names of its\n"\
"      contributors may be used to endorse or promote products derived from\n"\
"      this software without specific prior written permission.\n"\
"\n"\
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n"\
"AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n"\
"IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\n"\
"ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE\n"\
"LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\n"\
"CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\n"\
"SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\n"\
"INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\n"\
"CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\n"\
"ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\n"\
"POSSIBILITY OF SUCH DAMAGE.\n"\
"\n"\
"       For more information contact us at:\n"\
"\n"\
"       Alaska Satellite Facility\n"\
"       Geophysical Institute\n"\
"       University of Alaska Fairbanks\n"\
"       P.O. Box 757320\n"\
"       Fairbanks, AK 99775-7320\n"\
"\n"\
"       http://www.asf.alaska.edu\n"\
"       uso@asf.alaska.edu"

#define ASF_PROGRAM_HISTORY_STRING \
"None."

/*===================END ASF AUTO-GENERATED DOCUMENTATION===================*/

#include "asf.h"
#include "ifm.h"
#include "asf_meta.h"

#define borderX 80	/* Distances from edge of image to start correlating.*/
#define borderY 80
#define maxDoubt 0.5	/* Points will larger margin will be flagged */
#define maxDisp 1.8	/* Forward and reverse correlations which differ 
			   by more than this will be deleted.*/
#define VERSION 1.0

/*Read-only, informational globals:*/
int lines, samples;		/* Lines and samples of source images. */
int srcSize=64;
int pointNo=0, gridResolution=20;
int chipX, chipY,            /*Chip location (top left corner) in second image*/
    chipDX,chipDY;           /*Chip size in second image.*/
int searchX,searchY;         /*Maximum distance to search for peak*/


/*Function declarations */
void usage(char *name);

bool getNextPoint(int *x1,int *y1,int *x2,int *y2);

void topOffPeak(float *peaks, int i, int j, int maxI, float *di, float *dj);

void findPeak(int x1,int y1, char *szImg1, int x2, int y2, char *szImg2,
	      float *peakX, float *peakY, float *snr);


/* Start of main progam */
int main(int argc, char *argv[])
{
  char szOut[255], szImg1[255], szImg2[255];
  int x1, x2, y1, y2;
  int goodPoints=0, attemptedPoints=0;
  FILE *fp_output;
  meta_parameters *masterMeta, *slaveMeta;
  
  /* Check command line args */
  if (argc < 3) usage(argv[0]);
  strcpy(szImg1,argv[1]);
  strcpy(szImg2,argv[2]);
  strcpy(szOut,argv[3]);
	
  /* Read metadata */
  masterMeta = meta_read(szImg1);
  slaveMeta = meta_read(szImg2);
  if (masterMeta->general->line_count != slaveMeta->general->line_count ||
      masterMeta->general->sample_count != slaveMeta->general->sample_count) {
    sprintf(errbuf, "   ERROR: Input images have different dimension\n");
    printf(errbuf);
    printLog(errbuf);
  }
  else {
    lines = masterMeta->general->line_count;
    samples = masterMeta->general->sample_count;
  }
  strcat(szImg1, ".img");
  strcat(szImg2, ".img");    

  /* Create output file */
  fp_output=FOPEN(szOut, "w");
  
  /* Loop over grid, performing forward and backward correlations */
  while (getNextPoint(&x1,&y1,&x2,&y2))
      {
	float dx, dy, doubt, dxFW, dyFW, doubtFW, dxBW, dyBW, doubtBW;
	attemptedPoints++;
	
	/* ...check forward correlation... */
	findPeak(x1,y1,szImg1,x2,y2,szImg2,&dxFW,&dyFW,&doubtFW);
	if (doubtFW < maxDoubt)
	  {
	    /* ...check backward correlation... */
	    findPeak(x2,y2,szImg2,x1,y1,szImg1,&dxBW,&dyBW,&doubtBW);
	    dxBW*=-1.0;dyBW*=-1.0;
	    if ((doubtBW < maxDoubt) &&
		(fabs(dxFW-dxBW) < maxDisp) &&
		(fabs(dyFW-dyBW) < maxDisp))
	      {
		goodPoints++;
		dx = (dxFW+dxBW)/2;
		dy = (dyFW+dyBW)/2;
		doubt = (doubtFW+doubtBW)/2;
		fprintf(fp_output,"%6d %6d %8.5f %8.5f %4.2f\n",
			x1, y1, x2+dx, y2+dy, doubt);
		fflush(fp_output);
	      }
	  }
      }
  if (goodPoints < attemptedPoints)
    printf("\n   WARNING: %i out of %i points moved!\n\n", 
	   (attemptedPoints-goodPoints), attemptedPoints);
  else 
    printf("\n   There is no difference between the images\n\n");
  
  return(0);
}

void usage(char *name)
{
  printf("\noffset_test:\n\n");
  printf("usage:\n   offset_test <file1> <file2> <out>\n\n");
  printf("\t<file1> and <file2> are basenames of two amplitude images\n");
  printf("\t<out> is the output file for reporting individual correlations\n");
  printf("\n"
	 "Offset_test is verifying that two images have no offset relative to each \n"
	 "other. This is achieved by matching small image chips defined on a regular \n"
	 "grid. If no offset can be determined between the two images, it is assumed \n"
	 "that changes in the implementation of a particular tool did not affect \n"
	 "the geometry or geolocation of the image.\n\n");
  printf("Version: %.2f, ASF SAR Tools\n",VERSION);
  exit(EXIT_FAILURE);
}


bool getNextPoint(int *x1,int *y1,int *x2,int *y2)
{
        int unscaledX, unscaledY;
        unscaledX=pointNo%gridResolution;
        unscaledY=pointNo/gridResolution;
        *x1=unscaledX*(samples-2*borderX)/(gridResolution-1)+borderX;
        *y1=unscaledY*(lines-2*borderY)/(gridResolution-1)+borderY;
        *x2=*x1;
        *y2=*y1;
        if (pointNo>=(gridResolution*gridResolution)) 
                return FALSE;
        pointNo++;
        return TRUE;
}


/*FindPeak: 
  This function computes a correlation peak, with SNR, between
  the two amplitude images at the given points.
*/
void findPeak(int x1, int y1, char *szImg1, int x2, int y2, char *szImg2,
	      float *peakX, float *peakY, float *doubt)
{
  static float *peaks;
  static float *s=NULL, *t, *product; /*Keep working arrays around between calls.*/
  int peakMaxX, peakMaxY, x,y,xOffset,yOffset,count;
  int srcIndex;
  int mX,mY; /* Invariant: 2^mX=ns; 2^mY=nl. */
#define modX(x) ((x+srcSize)%srcSize)  /* Return x, wrapped to [0..srcSize-1] */
#define modY(y)	((y+srcSize)%srcSize)  /* Return y, wrapped to [0..srcSize-1] */
  
  float peakMax, thisMax, peakSum;
  float aveChip=0;
  float scaleFact=1.0/(srcSize*srcSize);
  
  /* Allocate working arrays if we haven't already done so. */
  if (s==NULL)
    {                       
      s = (float *)(MALLOC(srcSize*srcSize*sizeof(float)));
      t = (float *)(MALLOC(srcSize*srcSize*sizeof(float)));
      product = (float *)(MALLOC(srcSize*srcSize*sizeof(float)));
      peaks=(float *)MALLOC(sizeof(float)*srcSize*srcSize);
    }
  
  /* At each grid point, read in a chunk of each image...*/
  readMatrix(szImg1, s, FLOAT, srcSize, srcSize, x1-srcSize/2+1, y1-srcSize/2+1, 
	     samples, lines, 0, 0);
  readMatrix(szImg2, t, FLOAT, srcSize, srcSize, x2-srcSize/2+1, y2-srcSize/2+1, 
	     samples, lines, 0, 0);
  
  /* Compute average brightness of chip */
  for(y=0;y<srcSize;y++)
    {
      srcIndex=y*srcSize;
      for(x=0;x<srcSize;x++) {
	aveChip+=s[x+srcIndex];
      }
    }
  aveChip/=-(float)srcSize*srcSize;

  /* Subtract average brightness from chip 2 */
  for(y=0;y<srcSize;y++)
    {
      srcIndex=y*srcSize;
      for(x=0;x<srcSize;x++)
	t[x+srcIndex]=(t[x+srcIndex]+aveChip)*scaleFact;
    }
  
  /* Add average brightness to chip 1 */
  for(y=0;y<srcSize;y++)
    {
      srcIndex=y*srcSize;
      for(x=0;x<srcSize;x++)
	s[x+srcIndex]=s[x+srcIndex]+aveChip;
    }
  
  /* Do the FFT and back */
  mX = (int)(log(srcSize)/log(2.0)+0.5);
  mY = (int)(log(srcSize)/log(2.0)+0.5);

  fft2dInit(mY,mX); /* Initialization */

  rfft2d(s,mY,mX); /* FFT chip 1 */
  rfft2d(t,mY,mX); /* FFT chip 2 */

  for(y=0;y<srcSize;y++) /* Conjugate chip 2 */
    {
      srcIndex=y*srcSize;
      if (y<2) x=1; else x=0;
      for (;x<srcSize/2;x++)
	t[srcIndex+2*x+1]*=-1.0;
    }	
  
  rspect2dprod(s,t,product,srcSize,srcSize); /* Complex product */
  
  for (y=0;y<4;y++) /* Zero out the low frequencies of the correlation chip */
    {
      srcIndex=y*srcSize;
      for (x=0;x<8;x++) product[srcIndex+x]=0;
      srcIndex=srcSize*(srcSize-1-y);
      for (x=0;x<8;x++) product[srcIndex+x]=0;
    }
  
  rifft2d(product,mY,mX); /* Inverse FFT */
  
  /* Set up search chip size. */
  chipDX = srcSize*3/4;
  chipDY = srcSize*3/4;
  chipX = srcSize/8;
  chipY = srcSize/8;
  searchX = srcSize*3/8;
  searchY = srcSize*3/8;

  /* Find peak */
  float biggestNearby=-100000000000.0;
  int delX=15+chipX/8,delY=15+chipY/8;
  int closeX=5+chipX/16,closeY=5+chipY/16;
  
  /* Search for the peak with the highest correlation strength */
  int bestX,bestY;
  float bestMatch=-100000000000.0;
  float bestLocX=delX,bestLocY=delY;

  for (y=chipY-searchY; y<chipY+searchY; y++)
    for (x=chipX-searchX; x<chipX+searchX; x++)
      {
	int index = srcSize * modY(y) + modX(x);
	if (bestMatch < product[index])
	  {
	    bestMatch = product[index];
	    bestX = x;
	    bestY = y;
	  }
      }
  topOffPeak(product,bestX,bestY,srcSize,&bestLocX,&bestLocY);
  
  /* Compute the doubt in our offset guess, by
     finding the largest of several pixels around our guess. */
  for (y=-delY; y<=delY; y++)
    for (x=-delX; x<=delX; x++)
      if ((abs(y)>closeY) || (abs(x)>closeX))
	{
	  float cor = product[modY(bestY+y)*srcSize+modX(bestX+x)];
	  if (biggestNearby < cor)
	    biggestNearby = cor;
	}

  *doubt=biggestNearby/bestMatch;
  if (*doubt<0) *doubt = 0;
  
  /* Output our guess. */
  *peakX = bestLocX;
  *peakY = bestLocY;
}


/* TopOffPeak:
   Given an array of peak values, use trilinear interpolation to determine the exact 
   (i.e. float) top. This works by finding the peak of a parabola which goes though 
   the highest point, and the three points surrounding it.
*/
void topOffPeak(float *peaks,int i,int j,int maxI,float *di,float *dj)
{
        float a,b,c,d;
        a=peaks[modY(j)*maxI+modX(i-1)];
        b=peaks[modY(j)*maxI+modX(i)];
        c=peaks[modY(j)*maxI+modX(i+1)];
        d=4*((a+c)/2-b);
        if (d!=0)
                *di=i+(a-c)/d;
        else *di=i;
        a=peaks[modY(j-1)*maxI+modX(i)];
        b=peaks[modY(j)*maxI+modX(i)];
        c=peaks[modY(j+1)*maxI+modX(i)];
        d=4*((a+c)/2-b);
        if (d!=0)
                *dj=j+(a-c)/d;
        else *dj=j;
}
