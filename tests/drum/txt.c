#include <math.h>
#include "po32.h"
#include "drum.c"
#include "po32tx.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){
 static uint8_t frame[8192]; po32_builder_t b; po32_builder_init(&b,frame,sizeof frame);
 static po32tx_t T; po32tx_begin(&T);
 for(int i=0;i<argc-1;i++){FILE*f=fopen(argv[1+i],"rb");char t[4096];size_t l=fread(t,1,sizeof t,f);fclose(f);
  po32_patch_params_t P; po32_patch_parse_mtdrum_text(t,l,&P); uint8_t e[64];size_t el; po32_encode_patch(&P,e,sizeof e,&el);
  uint16_t u[21]; for(int k=0;k<21;k++)u[k]=e[2*k]|(e[2*k+1]<<8);
  for(int side=0;side<2;side++){po32_patch_packet_t pk={0};pk.instrument=i+1;pk.side=side;pk.params=P;po32_packet_t out;po32_packet_encode(PO32_TAG_PATCH,&pk,&out);po32_builder_append(&b,&out);
   po32tx_patch(&T,i,side,u);}}
 size_t fl; po32_builder_finish(&b,&fl); int tl=po32tx_finish(&T);
 printf("libpo frame %zu, ours %d, identical %d\n",fl,tl,(size_t)tl==fl&&!memcmp(frame,T.buf,fl));
 /* audio from ours, decoded by libpo */
 int N=po32_render_sample_count(fl,44100)+2000; static int32_t pcm[400000]; po32tx_play(&T); int n=0; while(n<N-1000&&po32tx_render(&T,pcm+n,1000,26000))n+=1000; n+=1000; if(n>N)n=N;
 float*a=calloc(n,4); for(int i=0;i<n;i++)a[i]=pcm[i]/32768.0f;
 static uint8_t back[8192]; size_t bl; po32_decode_result_t r; po32_status_t s=po32_decode_f32(a,n,44100,&r,back,sizeof back,&bl);
 printf("decode of OUR audio: status %d packets %d done %d, bytes match %d (%zu samples, %.2f s)\n",s,r.packet_count,r.done,bl==fl&&!memcmp(back,frame,fl),(size_t)n,n/44100.0);
 /* also compare against libpo's own render */
 float*ref=calloc(po32_render_sample_count(fl,44100),4); po32_render_dpsk_f32(frame,fl,44100,ref,po32_render_sample_count(fl,44100));
 double se=0,sr=0; size_t m=po32_render_sample_count(fl,44100); for(size_t i=0;i<m&&i<(size_t)n;i++){double d=a[i]-ref[i]*26000/32768.0;se+=d*d;sr+=(double)ref[i]*ref[i];}
 printf("rms diff vs libpo render: %.4f of %.4f\n",sqrt(se/m),sqrt(sr/m));
 {FILE*w=fopen("ours.wav","wb");unsigned d=n*2,x=36+d,fm=16,br=88200,sr2=44100;unsigned short pc=1,ch=1,ba=2,bt=16;fwrite("RIFF",1,4,w);fwrite(&x,4,1,w);fwrite("WAVEfmt ",1,8,w);fwrite(&fm,4,1,w);fwrite(&pc,2,1,w);fwrite(&ch,2,1,w);fwrite(&sr2,4,1,w);fwrite(&br,4,1,w);fwrite(&ba,2,1,w);fwrite(&bt,2,1,w);fwrite("data",1,4,w);fwrite(&d,4,1,w);for(int i=0;i<n;i++){short q=pcm[i];fwrite(&q,2,1,w);}fclose(w);}
 return 0;}
