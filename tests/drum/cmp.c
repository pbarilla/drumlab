#include "po32.h"
#include "po32_synth.h"
#include "drum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
/* cmp d.txt [dump.wav-prefix] : prints rms error (dB re ref rms) and peaks */
int main(int argc,char**argv){
 FILE*f=fopen(argv[1],"rb");char t[4096];size_t l=fread(t,1,sizeof t,f);fclose(f);
 po32_patch_params_t P; if(po32_patch_parse_mtdrum_text(t,l,&P)!=PO32_OK){puts("parse fail");return 1;}
 uint8_t b[64];size_t bl; po32_encode_patch(&P,b,sizeof b,&bl);
 uint16_t u[21]; /* skip nothing: encode_patch output is 21 x u16 LE or with command byte? */
 int off = (bl==43)?1:0; for(int i=0;i<21;i++)u[i]=b[off+2*i]|(b[off+2*i+1]<<8);
 po32_patch_params_t Q; po32_decode_patch(b,bl,&Q); /* dequantised */
 int vel=argc>3?atoi(argv[3]):127;
 int N=44100*3; float*r=calloc(N,4); size_t rl; po32_synth_t sy; po32_synth_init(&sy,44100);
 po32_synth_render(&sy,&Q,vel,3.0f,r,N,&rl);
 static drum_t v; drum_trig(&v,u,vel); if(getenv("DBG"))fprintf(stderr,"ring %d turns/sample %.6f corr %.3f\n",v.ring,v.pinc/4294967296.0,v.corr/65536.0); int32_t*o=calloc(N,4); int cnt=0;
 while(cnt<N){int k=1024<N-cnt?1024:N-cnt; int on=drum_render(&v,o+cnt,k); cnt+=k; if(!on)break;}
 double se=0,sr=0,pk=0,pr=0,po=0; for(int i=0;i<cnt;i++){double a=o[i]/1048576.0,d=a-r[i];se+=d*d;sr+=(double)r[i]*r[i]; if(fabs(d)>pk)pk=fabs(d);if(fabs(r[i])>pr)pr=fabs(r[i]);if(fabs(a)>po)po=fabs(a);}
 double tail=0;for(int i=cnt;i<N;i++)tail+=(double)r[i]*r[i];
 printf("len %d  err %.1f dB  peakdiff %.3f  refpk %.3f ourpk %.3f  tail-ref-rms %.1f dB\n",cnt,10*log10((se+1e-20)/(sr+1e-20)),pk,pr,po,10*log10((tail+1e-20)/(sr+1e-20)));
 if(argc>2){FILE*w=fopen(argv[2],"wb");for(int i=0;i<cnt;i++){short a=(short)fmaxf(-32767,fminf(32767,o[i]/32.0f)),c=(short)fmaxf(-32767,fminf(32767,r[i]*32767));fwrite(&a,2,1,w);fwrite(&c,2,1,w);}fclose(w);}
 return 0;}
