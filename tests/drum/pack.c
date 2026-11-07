#include <math.h>
#include "po32.h"
#include "drum.c"
#include "po32tx.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char**argv){
 static uint8_t frame[16384]; po32_builder_t b; po32_builder_init(&b,frame,sizeof frame);
 static po32tx_t T; po32tx_begin(&T);
 srand(7);
 /* 32 patches: random params */
 for(int slot=0;slot<16;slot++)for(int side=0;side<2;side++){uint16_t u[21];po32_patch_params_t P;float*f=(float*)&P;for(int k=0;k<21;k++){u[k]=rand()&0xFFFF;f[k]=u[k]/65535.0f;}
  po32_patch_packet_t pk={0};pk.instrument=slot+1;pk.side=side;pk.params=P;po32_packet_t o;if(po32_packet_encode(PO32_TAG_PATCH,&pk,&o)){puts("patch enc");return 1;}po32_builder_append(&b,&o);po32tx_patch(&T,slot,side,u);}
 /* 16 patterns */
 for(int pn=0;pn<16;pn++){
  po32_pattern_packet_t pp; po32_pattern_init(&pp,pn);
  uint8_t trig[4][16]={{0}},lock[4][16][2]={{{0}}}; uint32_t acc=0;
  for(int s=0;s<16;s++){ if(rand()%4==0)acc|=1u<<s; for(int l=0;l<4;l++) if(rand()%3==0){int grp=rand()%4;int inst=l+1+4*grp;int fr=1+rand()%15;
     po32_pattern_set_trigger(&pp,s,inst,fr);} if(rand()%5==0&&acc>>s&1)po32_pattern_set_accent(&pp,s,1);}
  /* read back what libpo built, to feed ours the same bytes */
  po32_packet_t o; if(po32_packet_encode(PO32_TAG_PATTERN,&pp,&o)){puts("pat enc");return 1;}
  if(o.payload_len!=211){puts("len");return 1;}
  int pos=1; for(int l=0;l<4;l++){for(int s=0;s<16;s++)trig[l][s]=o.payload[pos++];for(int s=0;s<16;s++){lock[l][s][0]=o.payload[pos++];lock[l][s][1]=o.payload[pos++];}}
  acc=o.payload[209]|(o.payload[210]<<8);
  po32_builder_append(&b,&o); po32tx_pattern(&T,pn,trig,lock,acc);}
 /* state */
 po32_state_packet_t st; memset(&st,0,sizeof st); po32_morph_pairs_default(st.morph_pairs,16); st.tempo=120; st.swing_times_12=6; st.pattern_count=5; uint8_t ch[5]={1,1,2,3,16}; for(int i=0;i<5;i++)st.pattern_numbers[i]=ch[i];
 {po32_packet_t o; if(po32_packet_encode(PO32_TAG_STATE,&st,&o)){puts("st enc");return 1;} po32_builder_append(&b,&o);}
 uint8_t knob[16][2]; for(int i=0;i<16;i++){knob[i][0]=0x80;knob[i][1]=0x01;} po32tx_state(&T,knob,120,6,ch,5);
 size_t fl; po32_builder_finish(&b,&fl); int tl=po32tx_finish(&T);
 printf("libpo frame %zu bytes, ours %d, identical %d\n",fl,tl,(size_t)tl==fl&&!memcmp(frame,T.buf,fl));
 /* audio of ours decoded by libpo */
 size_t N=po32_render_sample_count(fl,44100)+3000; int32_t*pcm=malloc(N*4); po32tx_play(&T); size_t n=0; while(n+1000<=N&&po32tx_render(&T,pcm+n,1000,26000))n+=1000; n+=1000; if(n>N)n=N;
 float*a=malloc(n*4); for(size_t i=0;i<n;i++)a[i]=pcm[i]/32768.0f;
 static uint8_t back[16384]; size_t bl; po32_decode_result_t r; po32_status_t s=po32_decode_f32(a,n,44100,&r,back,sizeof back,&bl);
 printf("decoded: status %d packets %d done %d bytes match %d; audio %.1f s\n",s,r.packet_count,r.done,bl==fl&&!memcmp(back,frame,fl),n/44100.0);
 return 0;}
