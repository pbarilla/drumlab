#include "drum.c"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
int main(){
 double fs=44100; double fl[]={30,61,200,1000,5000,15000}; double gl[]={-40,-39,-30,-20,-10,0};
 for(int fi=0;fi<6;fi++)for(int gi=0;gi<6;gi++){
  double f=fl[fi],g=gl[gi];
  /* params: x such that f=20*1000^x ; gain */
  int pf=(int)lround(log(f/20)/log(1000)*65535); int pg=(int)lround((g/80+0.5)*65535);
  uint16_t p[21]={0}; p[15]=pf; p[16]=pg; p[17]=60000; p[13]=0;
  /* take what drum_trig builds */
  drum_t v; drum_trig(&v,p,127);
  double fq=20*pow(1000,pf/65535.0), gg=(pg/65535.0-0.5)*80;
  double A=pow(10,gg/40), w0=2*M_PI*fq/fs, al=sin(w0)/2, c=cos(w0), a0=1+al/A;
  double b0=(1+al*A)/a0,b1=-2*c/a0,b2=(1-al*A)/a0,a1=b1,a2=(1-al/A)/a0;
  double z1=0,z2=0; double se=0,sr=0; unsigned s=1;
  /* run both on noise */
  v.eq.z1=v.eq.z2=0;
  for(int i=0;i<44100;i++){ s=s*1664525u+1013904223u; double x=((s>>8)/16777216.0*2-1)*0.5;
   int32_t xi=(int32_t)(x*1048576); double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y;
   double yo=bq_run(&v.eq,xi)/1048576.0; if(i>4000){se+=(yo-y)*(yo-y);sr+=y*y;} }
  printf("f=%6.0f g=%+5.0f  err %6.1f dB  use_eq=%d\n",f,g,10*log10(se/sr+1e-30),v.use_eq);
 }}
