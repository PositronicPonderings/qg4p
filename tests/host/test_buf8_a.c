/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* M8 host tests: real BUF8 backend + flush against a simulated panel. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qg4p.h"
#include "qg_internal.h"
#define PW 320
#define PH 480
/* ---- simulated panel for BUF8 flushes (stubs replace HAL + driver) ---- */
static uint16_t panel[PH][PW]; static int wx0,wy0,wx1,wy1,wcx,wcy; static long streamed; static int last_w,last_h;
void qg_hal_begin(qg_hal_device_t*d){(void)d;} void qg_hal_end(qg_hal_device_t*d){(void)d;}
void qg_hal_write_cmd(qg_hal_device_t*d,uint8_t c){(void)d;(void)c;}
void qg_hal_write_data(qg_hal_device_t*d,const uint8_t*p,size_t n){(void)d;(void)p;(void)n;}
void qg_driver_set_window(qg_hal_device_t*d,uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){(void)d;wx0=x0;wy0=y0;wx1=x1;wy1=y1;wcx=x0;wcy=y0;last_w=x1-x0+1;last_h=y1-y0+1;}
void qg_hal_stream_begin(qg_hal_device_t*d){(void)d;}
void qg_hal_stream_pixels(qg_hal_device_t*d,const uint16_t*p,uint32_t n){(void)d; for(uint32_t i=0;i<n;i++){ panel[wcy][wcx]=p[i]; streamed++; if(++wcx>wx1){wcx=wx0; wcy++;} }}
void qg_hal_stream_end(qg_hal_device_t*d){(void)d;}
/* ---- DIRECT reference screen ---- */
static uint16_t dpanel[PH][PW];
static void dff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)dpanel[j][i]=s->palette[c];}
static void dwr(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t*px){(void)s;for(int j=0;j<h;j++)for(int i=0;i<w;i++)dpanel[y+j][x+i]=px[j*w+i];}
static const qg_backend_t dback={.name="D",.fill_rect=dff,.write_rgb565=dwr};
static uint8_t fb[PW*PH];
static qg_screen_t sd, sb;
static qg_font_t f_body=QG_FONT_INIT(qg_font_sans_16,QG_DEFAULT,1), f_title=QG_FONT_INIT(qg_font_sans_bold_24,QG_YELLOW,1), f_mono=QG_FONT_INIT(qg_font_mono_12,QG_DEFAULT,1);
static void mk(qg_screen_t*s,int w,int h,int buf){ memset(s,0,sizeof *s); s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true; s->fg_color=QG_WHITE;s->bg_color=QG_BLACK;s->line_width=1;s->text_bg=QG_TRANSPARENT;s->tab_width=40;s->wrap=true;s->scroll=true;
  qg_palette_copy_standard(s->palette); if(buf){s->fb=fb; s->backend=&qg_backend_buf8;} else s->backend=&dback;
  qg_screen_set_font(s,0,&f_body); qg_screen_set_font(s,1,&f_title); qg_screen_set_font(s,2,&f_mono);}
void qg_screen_set_line_width(qg_screen_t *s, uint8_t w){ s->line_width = w<1?1:w; }
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg){ if(fg<=254)s->fg_color=fg; if(bg<=254)s->bg_color=bg; }
void qg_screen_flush(qg_screen_t *s){ if(s->backend->flush) s->backend->flush(s); }
static int fails; static void check(int ok,const char*m){ printf("%s  %s\n",ok?"PASS":"FAIL",m); if(!ok)fails++; }
static void scene(qg_screen_t*s){
  qg_cls(s,QG_BLUE);
  qg_screen_set_line_width(s,1); qg_line(s,-5,3,300,200,QG_YELLOW); qg_circle(s,60,60,40,QG_WHITE,QG_RED);
  qg_screen_set_line_width(s,5); qg_arc(s,160,120,70,50,-30,210,QG_LIGHTGREEN); qg_box(s,10,150,120,230,QG_CYAN,QG_DARKGRAY);
  qg_line(s,200,20,300,180,QG_LIGHTRED); qg_screen_set_line_width(s,1);
  qg_ellipse(s,250,300,60,30,QG_TRANSPARENT,QG_MAGENTA);
  qg_print_at(s,14,160,"Opaque? {c:YELLOW}no{c:} {s:2}big",QG_DEFAULT,NULL);
  qg_screen_set_text_bg(s,QG_BLACK); qg_print_at(s,14,250,"Opaque text {f:1}Bold{f:} 123",QG_WHITE,NULL); qg_screen_set_text_bg(s,QG_TRANSPARENT);
  qg_print_box(s,20,330,200,"Wrapped {c:LIGHTGREEN}inside{c:} a box of 200 pixels, centred nicely.",QG_ALIGN_CENTER);
  qg_circle(s,300,460,60,QG_WHITE,QG_GREEN);  /* clipped */ }
static int panels_equal(int w,int h){ for(int y=0;y<h;y++)for(int x=0;x<w;x++) if(panel[y][x]!=dpanel[y][x]) return 0; return 1; }
int main(void){
  /* A: identical output */
  mk(&sd,PW,PH,0); mk(&sb,PW,PH,1); scene(&sd); scene(&sb); streamed=0; qg_screen_flush(&sb);
  check(panels_equal(PW,PH), "A: shapes, arcs, thick lines, markup, opaque and wrapped text: BUF8+flush == DIRECT, every pixel");
  check(streamed==PW*PH && !sb.dirty, "A: first flush sent the whole screen once, then nothing is pending");
  /* B: dirty rectangle */
  streamed=0; qg_screen_flush(&sb); check(streamed==0, "B: flush with no changes sends nothing");
  qg_box(&sb,100,200,139,229,QG_TRANSPARENT,QG_YELLOW); qg_box(&sd,100,200,139,229,QG_TRANSPARENT,QG_YELLOW);
  streamed=0; qg_screen_flush(&sb);
  check(streamed==40*30 && wx0==100&&wy0==200&&wx1==139&&wy1==229 && panels_equal(PW,PH), "B: a 40x30 box -> exactly a 40x30 window sent, panel still identical");
  qg_pset(&sb,5,5,QG_RED); qg_pset(&sb,300,400,QG_RED); qg_pset(&sd,5,5,QG_RED); qg_pset(&sd,300,400,QG_RED);
  streamed=0; qg_screen_flush(&sb); check(last_w==296&&last_h==396&&panels_equal(PW,PH), "B: two far-apart pixels -> one covering rectangle (296x396)");
  qg_palette_set(&sb,QG_YELLOW,10,200,10); streamed=0; qg_screen_flush(&sb);
  check(streamed==PW*PH, "B: a palette change marks the whole screen for the next flush");
  qg_palette_reset(&sb); qg_screen_flush(&sb);
  check(qg_point(&sb,120,210)==QG_YELLOW && qg_point(&sb,-1,0)==QG_NONE && qg_point(&sd,5,5)==QG_NONE, "B: qg_point reads BUF8 pixels; QG_NONE off-screen and on DIRECT");
  /* C: paint */
  FILE*f=fopen("paint_in.bin","wb"); mk(&sb,PW,PH,1); qg_cls(&sb,QG_BLACK);
  qg_circle(&sb,160,160,120,QG_WHITE,QG_TRANSPARENT); qg_screen_set_line_width(&sb,1);
  for(int i=0;i<12;i++) qg_line(&sb,160,160,(int16_t)(160+110*((i*37)%100-50)/50),(int16_t)(160+110*((i*61)%100-50)/50),QG_WHITE);
  qg_box(&sb,40,300,280,470,QG_WHITE,QG_TRANSPARENT); for(int k=0;k<9;k++) qg_line(&sb,(int16_t)(40+k*30),300,(int16_t)(55+k*30),470,QG_WHITE);
  qg_circle(&sb,160,385,30,QG_RED,QG_TRANSPARENT);
  fwrite(fb,1,sizeof fb,f); fclose(f);
  int r1=qg_paint(&sb,150,150,QG_BLUE,QG_WHITE); int r2=qg_paint(&sb,60,320,QG_GREEN,QG_DEFAULT); int r3=qg_paint(&sb,5,470,QG_DARKGRAY,QG_WHITE);
  f=fopen("paint_out.bin","wb"); fwrite(fb,1,sizeof fb,f); fclose(f);
  printf("      paint results %d %d %d\n",r1,r2,r3);
  check(qg_paint(&sd,1,1,QG_RED,QG_WHITE)==QG_ERR_UNSUPPORTED, "C: qg_paint on DIRECT -> unsupported");
  /* overflow: a comb of many 1-px teeth */
  mk(&sb,PW,PH,1); qg_cls(&sb,QG_BLACK); for(int x=1;x<PW;x+=2) qg_line(&sb,(int16_t)x,0,(int16_t)x,(int16_t)(PH-2),QG_WHITE);
  check(qg_paint(&sb,0,0,QG_RED,QG_WHITE)==QG_OK, "C: comb of 160 one-pixel channels fills without overflow");
  mk(&sb,PW,PH,1); qg_cls(&sb,QG_BLACK); for(int y=0;y<PH;y+=2) for(int x=(y/2)%2;x<PW;x+=2) qg_pset(&sb,(int16_t)x,(int16_t)y,QG_WHITE);
  int ro=qg_paint(&sb,1,1,QG_RED,QG_WHITE); check(ro==QG_OK||ro==QG_ERR_OVERFLOW, "C: pathological checker maze -> finishes or reports QG_ERR_OVERFLOW, never crashes");
  printf("      (checker maze result: %d)\n",ro);
  printf(fails?"\n%d FAILED\n":"\nALL PASS (A-C)\n",fails); return fails; }
