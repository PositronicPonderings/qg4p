/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qg4p.h"
qg_bus_t bus; qg_screen_t scr_a, scr_b;
#define TH 1600
static uint8_t fbA[480][320], fbB[TH][320]; static int SW, SHH;
static void ffA(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){(void)s;for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)fbA[j][i]=(uint8_t)c;}
static void ffB(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){(void)s;for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)fbB[j][i]=(uint8_t)c;}
static const qg_backend_t ta={.name="A",.fill_rect=ffA}, tbk={.name="B",.fill_rect=ffB};
void demo_setup(const char*t){(void)t;}
void sleep_ms(uint32_t m){(void)m;} uint64_t time_us_64(void){return 0;}
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w){ s->line_width = w<1?1:w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg){ if(fg<=254)s->fg_color=fg; if(bg<=254)s->bg_color=bg; }
#define printf(...) ((void)0)
#define main demo_main
#include "../hardware/m5_demo.c"
#undef main
#undef printf
static void mk(qg_screen_t*s,int w,int h,const qg_backend_t*b){ memset(s,0,sizeof *s); s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->backend=b;s->fg_color=QG_WHITE;s->bg_color=QG_BLACK;s->line_width=1;s->text_bg=QG_TRANSPARENT; s->tab_width=40; s->wrap=true; s->scroll=true; qg_palette_copy_standard(s->palette);
 qg_screen_set_font(s,0,&f_body); qg_screen_set_font(s,1,&f_title); qg_screen_set_font(s,2,&f_mono);}
static int total_diff = 0;
int main(void){
 int dims[2][2]={{240,320},{320,480}};
 for(int k=0; k<2; k++) for(int bg=0; bg<2; bg++){ SW=dims[k][0]; SHH=dims[k][1];
  memset(fbA,0,sizeof fbA); memset(fbB,0,sizeof fbB);
  mk(&scr_a,SW,SHH,&ta); rng_state=2024; if(bg){ scr_a.text_bg=QG_BLACK; }
  /* page_scroll calls fresh() which resets text_bg; replicate it inline */
  qg_screen_t *s=&scr_a; char buf[48];
  for(int pass=0;pass<2;pass++){
    fresh(s); if(bg) qg_screen_set_text_bg(s,QG_DARKGRAY);
    qg_println(s,"{f:1}Roll log");
    for(int i=1;i<=LOG_LINES;i++){ int r=roll(20); const char*col=(r==20)?"LIGHTGREEN":(r==1)?"LIGHTRED":"WHITE";
      qg_screen_set_colors(s,QG_LIGHTGRAY,QG_DEFAULT); snprintf(buf,sizeof buf,"#%02d  d20 = ",i); qg_print(s,buf);
      qg_screen_set_colors(s,QG_WHITE,QG_DEFAULT); snprintf(buf,sizeof buf,"{c:%s}%d",col,r); qg_println(s,buf);}
    if(pass==0){ /* tall reference */ } 
    if(pass==0){ int bottom=s->cursor_y; (void)bottom; }
    if(pass==0){ /* switch to reference screen */ static int done; (void)done; }
    if(pass==0){ int last_bottom = s->cursor_y; printf_dummy: ;
      /* reference run */
      mk(&scr_a,SW,TH,&tbk); rng_state=2024; s=&scr_a; (void)last_bottom; }
  }
  /* reference bottom = cursor_y after run (top of the next line = bottom of last) */
  int bottom = scr_a.cursor_y;
  int diff=0; for(int j=0;j<SHH;j++) for(int i=0;i<SW;i++) if(fbA[j][i]!=fbB[bottom-SHH+j][i]) diff++;
  total_diff += diff;
  fprintf(stderr,"%dx%d %s background: scrolled vs reference window -> %d differing pixels\n", SW,SHH,bg?"opaque":"transparent", diff);
 }
 return total_diff != 0;}
