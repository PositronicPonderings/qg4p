/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qg4p.h"
static uint8_t fb[2][480][480][3];
qg_bus_t bus; qg_screen_t scr_a, scr_b;
static void ff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){
  uint16_t p=s->palette[c]; uint8_t r=((p>>11)&31)<<3, g=((p>>5)&63)<<2, b=(p&31)<<3; int k=(s==&scr_a)?0:1;
  if(x<0||y<0||x+w>s->width||y+h>s->height){printf("OOB!\n");}
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++){fb[k][j][i][0]=r;fb[k][j][i][1]=g;fb[k][j][i][2]=b;}}
static const qg_backend_t tb={.name="T",.fill_rect=ff};
void demo_setup(const char*t){(void)t;}
static uint64_t fake_t; void sleep_ms(uint32_t m){(void)m;} uint64_t time_us_64(void){return fake_t+=3000;}
static int natw[2]={240,320}, nath[2]={320,480};
qg_err_t qg_screen_set_rotation(qg_screen_t*s,qg_rotation_t r){int k=(s==&scr_a)?0:1; s->rotation=r; if(r&1){s->width=nath[k];s->height=natw[k];}else{s->width=natw[k];s->height=nath[k];} qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; } return QG_OK;}
void qg_screen_set_line_width(qg_screen_t *scr, uint8_t w){ scr->line_width = w<1?1:w; }
#define printf(...) ((void)0)
#define main demo_main
#include "../hardware/m3_demo.c"
#undef main
#undef printf
static void mk(qg_screen_t*s,int w,int h){ s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->backend=&tb;s->fg_color=QG_WHITE;s->line_width=1; qg_palette_copy_standard(s->palette);}
static void dump(const char*name,int k,int w,int h){FILE*f=fopen(name,"wb");fprintf(f,"P6 %d %d 255\n",w,h);for(int j=0;j<h;j++)fwrite(fb[k][j],3,w,f);fclose(f);}
int main(void){ mk(&scr_a,240,320); mk(&scr_b,320,480);
 for(int rot=0;rot<=1;rot++){ memset(fb,0,sizeof fb); for(int i=0;i<2;i++){qg_screen_set_rotation(screens[i],rot?QG_ROT_90:QG_ROT_0); draw_layout(screens[i]);}
   char b[40]; sprintf(b,"l%d_a.ppm",rot); dump(b,0,scr_a.width,scr_a.height); sprintf(b,"l%d_b.ppm",rot); dump(b,1,scr_b.width,scr_b.height);}
 for(int i=0;i<2;i++) qg_screen_set_rotation(screens[i],QG_ROT_0);
 memset(fb,0,sizeof fb); page_dial(); dump("dial_a.ppm",0,240,320); dump("dial_b.ppm",1,320,480);
 return 0;}
