/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qg_image.h"
#include "qg_draw.h"
#define SW 480
#define SH 480
static uint16_t fb[SH][SW]; static uint8_t mask[SH][SW]; static long xfers, oob;
static void ff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){(void)s;(void)x;(void)y;(void)w;(void)h;(void)c;}
static void wr(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t*px){ xfers++;
  if(x<0||y<0||x+w>s->width||y+h>s->height){oob++;return;}
  for(int j=0;j<h;j++)for(int i=0;i<w;i++){ if(mask[y+j][x+i]) oob+=0; fb[y+j][x+i]=px[j*w+i]; mask[y+j][x+i]++; }}
static const qg_backend_t tb={.name="T",.fill_rect=ff,.write_rgb565=wr};
int main(int argc,char**argv){
  if(argc<8){fprintf(stderr,"usage\n");return 2;}
  FILE*f=fopen(argv[1],"rb"); fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
  uint8_t*buf=malloc(n); fread(buf,1,n,f); fclose(f);
  qg_screen_t s; memset(&s,0,sizeof s); s.width=SW; s.height=SH; qg_view_reset(&s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(&s)) k_++; qg_hist_owner[k_] = (&s); (&s)->hist = qg_hist_pool[k_]; (&s)->hist_cap = QG_TEXT_HISTORY_LINES; } s.ready=true; s.backend=&tb;
  qg_image_t img; int err=qg_image_open(&img,buf,(uint32_t)n,(uint8_t)atoi(argv[2]));
  if(err){ printf("open error %d\n",err); return 1;}
  int dx=atoi(argv[3]),dy=atoi(argv[4]),dw=atoi(argv[5]),dh=atoi(argv[6]);
  qg_image_draw_scaled(&s,&img,(int16_t)dx,(int16_t)dy,(int16_t)dw,(int16_t)dh);
  FILE*o=fopen(argv[7],"wb"); fwrite(fb,sizeof fb,1,o); fwrite(mask,sizeof mask,1,o); fclose(o);
  printf("%ld %ld\n",xfers,oob); return 0; }
