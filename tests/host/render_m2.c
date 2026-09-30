/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qg4p.h"
static uint8_t fb[2][480][320][3];
static int cur;
static void ff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){
  uint16_t p=s->palette[c]; uint8_t r=((p>>11)&31)<<3, g=((p>>5)&63)<<2, b=(p&31)<<3;
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++){fb[cur][j][i][0]=r;fb[cur][j][i][1]=g;fb[cur][j][i][2]=b;}}
static const qg_backend_t tb={.name="T",.fill_rect=ff};
qg_bus_t bus; qg_screen_t scr_a, scr_b;
void demo_setup(const char*t){(void)t;}
void sleep_ms(uint32_t m){(void)m;} uint64_t time_us_64(void){return 0;}
#define printf(...) ((void)0)
#define main demo_main
#include "../hardware/m2_demo.c"
#undef main
#undef printf
static void mk(qg_screen_t*s,int w,int h){ s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->backend=&tb;s->fg_color=QG_WHITE;s->line_width=1; qg_palette_copy_standard(s->palette);}
static void dump(const char*name,int idx,int w,int h){FILE*f=fopen(name,"wb");fprintf(f,"P6 %d %d 255\n",w,h);for(int j=0;j<h;j++)fwrite(fb[idx][j],3,w,f);fclose(f);}
static void page(page_fn fn,const char*n){ char b[64];
  cur=0; memset(fb,0,sizeof fb); qg_screen_set_line_width(&scr_a,1); fn(&scr_a); sprintf(b,"dm_%s.ppm",n); dump(b,0,240,320);
  cur=1; qg_screen_set_line_width(&scr_b,1); fn(&scr_b); sprintf(b,"pl_%s.ppm",n); dump(b,1,320,480);}
int main(void){ mk(&scr_a,240,320); mk(&scr_b,320,480);
 page(page_lines,"1"); page(page_boxes,"2"); page(page_circles,"3"); page(page_arcs,"4"); page(page_palette,"5"); page(page_clipping,"6"); return 0;}
