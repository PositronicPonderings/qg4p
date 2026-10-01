/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "qg4p.h"
#include "qg_internal.h"
qg_bus_t bus; qg_screen_t scr_a, scr_b;
static uint16_t ppanel[480][320], dpanel[320][240]; static int wx0,wx1,wcx,wcy; static long oob;
void qg_hal_begin(qg_hal_device_t*d){(void)d;} void qg_hal_end(qg_hal_device_t*d){(void)d;}
void qg_hal_write_cmd(qg_hal_device_t*d,uint8_t c){(void)d;(void)c;}
void qg_hal_write_data(qg_hal_device_t*d,const uint8_t*p,size_t n){(void)d;(void)p;(void)n;}
void qg_driver_set_window(qg_hal_device_t*d,uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){(void)d;(void)y1;wx0=x0;wx1=x1;wcx=x0;wcy=y0;}
void qg_hal_stream_begin(qg_hal_device_t*d){(void)d;}
void qg_hal_stream_pixels(qg_hal_device_t*d,const uint16_t*p,uint32_t n){(void)d; for(uint32_t i=0;i<n;i++){ ppanel[wcy][wcx]=p[i]; if(++wcx>wx1){wcx=wx0; wcy++;} }}
void qg_hal_stream_end(qg_hal_device_t*d){(void)d;}
static void dff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){ if(x<0||y<0||x+w>s->width||y+h>s->height)oob++; for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)dpanel[j][i]=s->palette[c];}
static void dwr(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t*px){ if(x<0||y<0||x+w>s->width||y+h>s->height){oob++;return;} for(int j=0;j<h;j++)for(int i=0;i<w;i++)dpanel[y+j][x+i]=px[j*w+i];}
static const qg_backend_t dback={.name="DIRECT",.fill_rect=dff,.write_rgb565=dwr};
static uint64_t ft; void sleep_ms(uint32_t m){(void)m;} uint64_t time_us_64(void){return ft+=37;}
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w){ s->line_width = w<1?1:w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg){ if(fg<=254)s->fg_color=fg; if(bg<=254)s->bg_color=bg; }
void qg_screen_flush(qg_screen_t *s){ if(s->backend->flush) s->backend->flush(s); }
void qg_screen_flush_all(qg_screen_t *s){ if(!s->fb) return; qg_int_dirty_all(s); s->backend->flush(s); }
static void mk(qg_screen_t*s,int w,int h,uint8_t*fb){ memset(s,0,sizeof *s); s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->fg_color=QG_WHITE;s->bg_color=QG_BLACK;s->line_width=1;s->text_bg=QG_TRANSPARENT;s->tab_width=40;s->wrap=true;s->scroll=true; qg_palette_copy_standard(s->palette);
  if(fb){s->fb=fb;s->backend=&qg_backend_buf8;} else s->backend=&dback; }
void demo_setup_ex(const char*t,uint8_t*fb,uint32_t n){(void)t;(void)n; mk(&scr_a,240,320,NULL); mk(&scr_b,320,480,fb);}
#define printf(...) ((void)0)
#define main demo_main
#include "../hardware/m8_demo.c"
#undef main
#undef printf
static void dump(const char*n,int pl){ FILE*f=fopen(n,"wb"); int w=pl?320:240,h=pl?480:320; fprintf(f,"P6 %d %d 255\n",w,h);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){ uint16_t p=pl?ppanel[y][x]:dpanel[y][x]; uint8_t c[3]={(uint8_t)(((p>>11)&31)<<3),(uint8_t)(((p>>5)&63)<<2),(uint8_t)((p&31)<<3)}; fwrite(c,1,3,f);} fclose(f);}
int main(void){ demo_setup_ex("",fb_b,sizeof fb_b);
 for(int i=0;i<2;i++){ qg_screen_set_font(screens[i],0,&f_body); qg_screen_set_font(screens[i],1,&f_title); qg_screen_set_font(screens[i],2,&f_mono);}
 qg_image_open(&d20,img_d20,img_d20_size,QG_IMAGE_TRANSPARENT); qg_image_open(&landscape,img_landscape,img_landscape_size,0);
 qg_screen_flush_all(&scr_b);
 page_flicker(); dump("t1_a.ppm",0); dump("t1_b.ppm",1);
 page_full_scene(); dump("t2_a.ppm",0); dump("t2_b.ppm",1);
 page_paint(); dump("t3_a.ppm",0); dump("t3_b.ppm",1);
 page_palette(); dump("t4_a.ppm",0); dump("t4_b.ppm",1);
 page_scroll(); dump("t5_a.ppm",0); dump("t5_b.ppm",1);
 fprintf(stderr,"out-of-bounds=%ld\n",oob); return 0;}
