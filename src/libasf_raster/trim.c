#include "asf.h"
#include "asf_meta.h"
#include "asf_raster.h"

#define VERSION 1.3
#define MINI(a,b) (((a)<(b))?(a):(b))
#define MAXI(a,b) (((a)>(b))?(a):(b))

int trim(char *infile, char *outfile, long long startX, long long startY,
	 long long sizeX, long long sizeY)
{
  meta_parameters *metaIn, *metaOut;
  long long pixelSize, offset;
  long long b,x,y,lastReadY,firstReadX,numInX;
  FILE *in,*out;
  char *buffer;

  // Check the pixel size
  metaIn = meta_read(infile);
  pixelSize = metaIn->general->data_type;
  if (pixelSize==3) pixelSize=4;         // INTEGER32
  else if (pixelSize==5) pixelSize=8;    // REAL64
  else if (pixelSize==6) pixelSize=2;    // COMPLEX_BYTE
  else if (pixelSize==7) pixelSize=4;    // COMPLEX_INTEGER16
  else if (pixelSize==8) pixelSize=8;    // COMPLEX_INTEGER32
  else if (pixelSize==9) pixelSize=8;    // COMPLEX_REAL32
  else if (pixelSize==10) pixelSize=16;  // COMPLEX_REAL64

  const int inMaxX = metaIn->general->sample_count;
  const int inMaxY = metaIn->general->line_count;

  if (sizeX < 0) sizeX = inMaxX - startX;
  if (sizeY < 0) sizeY = inMaxY - startY;

  /* Write out metadata */
  metaOut = meta_read(infile);
  metaOut->general->line_count = sizeY;
  metaOut->general->sample_count = sizeX;
  if (metaOut->sar) {
    if (!meta_is_valid_double(metaOut->sar->line_increment))
      metaOut->sar->line_increment = 1;
    if (!meta_is_valid_double(metaOut->sar->sample_increment))
      metaOut->sar->sample_increment = 1;
    metaOut->general->start_line += startY * metaOut->sar->line_increment;
    metaOut->general->start_sample += startX * metaOut->sar->sample_increment;
  }
  else {
    metaOut->general->start_line += startY;
    metaOut->general->start_sample += startX;
  }

  /* Some sort of conditional on the validity of the corner coordinates would 
     be nice here.*/
  if (metaIn->projection) {
    double bX, mX, bY, mY;
    bY = metaIn->projection->startY;
    bX = metaIn->projection->startX;
    mY = metaIn->projection->perY;
    mX = metaIn->projection->perX;
    metaOut->projection->startY = bY + mY * startY;
    metaOut->projection->startX = bX + mX * startX;
  }

  meta_write(metaOut, outfile);

  /* If everything's OK, then allocate a buffer big enough for one line of 
     output data.*/
  buffer= (char *)MALLOC(pixelSize*(sizeX));

  for (b=0; b<metaIn->general->band_count; ++b) {
    /* Open files */
    in = fopenImage(infile,"rb");
    out = fopenImage(outfile, b>0 ? "ab" : "wb");

    /* If necessary, fill the top of the output with zeros, by loading up a 
       buffer and writing.*/
    for (x=0;x<sizeX*pixelSize;x++)
      buffer[x]=0;
    for (y=0;y<-startY && y<sizeY;y++) {
      FWRITE(buffer,pixelSize,sizeX,out);
      if (y==0)
        asfPrintStatus("   Filling zeros at beginning of output image\n");
    }

    /* Do some calculations on where we should read.*/
    firstReadX=MAXI(0,-startX);
    numInX=MINI(MINI(sizeX,inMaxX-(firstReadX+startX)),sizeX-firstReadX);
    lastReadY=MINI(sizeY,inMaxY-startY);
    offset=0;
    
    for (;y<lastReadY;y++) {
      int inputY=y+startY,
        inputX=firstReadX+startX,
        outputX=firstReadX;
      
      offset=pixelSize*(inputY*inMaxX+inputX) + b*inMaxX*inMaxY*pixelSize;
      
      if (y==lastReadY) asfPrintStatus("   Writing output image\n");
      
      FSEEK64(in,offset,SEEK_SET);
    
      FREAD(buffer+outputX*pixelSize,pixelSize,numInX,in);
      FWRITE(buffer,pixelSize,sizeX,out);
    }

    /* Reset buffer to zeros and fill remaining pixels.*/
    for (x=0;x<sizeX*pixelSize;x++)
      buffer[x]=0;
    for (;y<sizeY;y++) {
      FWRITE(buffer,pixelSize,sizeX,out);
      if (y==sizeY)
        asfPrintStatus("   Filled zeros after writing output image\n");
    }

    FCLOSE(in);
    FCLOSE(out);
  }

  /* We're done.*/
  FREE(buffer);

  meta_free(metaIn);
  meta_free(metaOut);

  return 0;
}

void trim_zeros(char *infile, char *outfile, int * startX, int * endX)
{
  meta_parameters *metaIn;
  FILE *in;
  int i,nl,ns;
  float *buf;

  in = fopenImage(infile,"rb");
  metaIn = meta_read(infile);
  ns = metaIn->general->sample_count;
  nl = metaIn->general->line_count;
  
  *startX = ns-1;
  *endX = 0;

  buf = (float*)MALLOC(sizeof(float)*ns);
  for (i=0; i<nl; ++i) {
      int left = 0, right = ns-1;
      get_float_line(in, metaIn, i, buf);
      while (buf[left] == 0.0 && left<ns-1) ++left;
      while (buf[right] == 0.0 && right>0) --right;
      if (left < *startX) *startX = left;
      if (right > *endX) *endX = right;
  }

  *endX -= *startX;

  fclose(in);
  free(buf);
  meta_free(metaIn);

  trim(infile, outfile, *startX, 0, *endX, nl);
}
