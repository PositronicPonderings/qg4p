/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qg4p.h"
qg_bus_t bus; qg_screen_t scr_a, scr_b;
static uint8_t fb[2][480][320][3]; static long sends[2], oob;
static void put(qg_screen_t*s,int x,int y,uint16_t p){int k=(s==&scr_a)?0:1; fb[k][y][x][0]=((p>>11)&31)<<3; fb[k][y][x][1]=((p>>5)&63)<<2; fb[k][y][x][2]=(p&31)<<3;}
static void ff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){ sends[s==&scr_a?0:1]++; if(x<0||y<0||x+w>s->width||y+h>s->height||c>254) oob++;
  for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)put(s,i,j,s->palette[c]);}
static void wr(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t*px){ sends[s==&scr_a?0:1]++; if(x<0||y<0||x+w>s->width||y+h>s->height) oob++;
  for(int j=0;j<h;j++)for(int i=0;i<w;i++)put(s,x+i,y+j,px[j*w+i]);}
static const qg_backend_t tb={.name="T",.fill_rect=ff,.write_rgb565=wr};
void demo_setup(const char*t){(void)t;}
static uint64_t ft; void sleep_ms(uint32_t m){(void)m;} uint64_t time_us_64(void){return ft+=10;}
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w){ s->line_width = w<1?1:w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg){ if(fg<=254)s->fg_color=fg; if(bg<=254)s->bg_color=bg; }
#define printf(...) ((void)0)
#define main demo_main
#include "../hardware/m4_demo.c"
#undef main
#undef printf
static void mk(qg_screen_t*s,int w,int h){ s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->backend=&tb;s->fg_color=QG_WHITE;s->bg_color=QG_BLACK;s->line_width=1;s->text_bg=QG_TRANSPARENT; qg_palette_copy_standard(s->palette);}
static void dump(const char*name,int k,int w,int h){FILE*f=fopen(name,"wb");fprintf(f,"P6 %d %d 255\n",w,h);for(int j=0;j<h;j++)fwrite(fb[k][j],3,w,f);fclose(f);}
int main(void){ mk(&scr_a,240,320); mk(&scr_b,320,480);
 for(int i=0;i<2;i++) qg_screen_set_font(screens[i],0,&f_sans);
 page_fonts(&scr_a); page_fonts(&scr_b); dump("p1_dm.ppm",0,240,320); dump("p1_pl.ppm",1,320,480);
 page_opaque(&scr_a,"DM"); page_opaque(&scr_b,"P"); dump("p2_dm.ppm",0,240,320); dump("p2_pl.ppm",1,320,480);
 page_cursor(&scr_a); page_cursor(&scr_b); dump("p3_dm.ppm",0,240,320); dump("p3_pl.ppm",1,320,480);
 fprintf(stderr,"out-of-bounds=%ld\n",oob); return 0;}
