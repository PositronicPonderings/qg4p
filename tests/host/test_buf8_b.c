/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qg4p.h"
#include "qg_internal.h"
#include "demo_images.h"
#define PW 320
#define PH 480
#define TH 1600
static uint16_t panel[PH][PW]; static int wx0,wy0,wx1,wcx,wcy;
void qg_hal_begin(qg_hal_device_t*d){(void)d;} void qg_hal_end(qg_hal_device_t*d){(void)d;}
void qg_hal_write_cmd(qg_hal_device_t*d,uint8_t c){(void)d;(void)c;}
void qg_hal_write_data(qg_hal_device_t*d,const uint8_t*p,size_t n){(void)d;(void)p;(void)n;}
void qg_driver_set_window(qg_hal_device_t*d,uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1){(void)d;(void)y1;wx0=x0;wy0=y0;wx1=x1;wcx=x0;wcy=y0;}
void qg_hal_stream_begin(qg_hal_device_t*d){(void)d;}
void qg_hal_stream_pixels(qg_hal_device_t*d,const uint16_t*p,uint32_t n){(void)d; for(uint32_t i=0;i<n;i++){ panel[wcy][wcx]=p[i]; if(++wcx>wx1){wcx=wx0; wcy++;} }}
void qg_hal_stream_end(qg_hal_device_t*d){(void)d;}
static uint16_t tall[TH][PW];
static void tff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)tall[j][i]=s->palette[c];}
static uint16_t dp[PH][PW];
static void dff(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)dp[j][i]=s->palette[c];}
static void dwr(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t*px){(void)s;for(int j=0;j<h;j++)for(int i=0;i<w;i++)dp[y+j][x+i]=px[j*w+i];}
static const qg_backend_t tback={.name="T",.fill_rect=tff}, dback={.name="D",.fill_rect=dff,.write_rgb565=dwr};
static uint8_t fb[PW*PH];
static qg_font_t f_body=QG_FONT_INIT(qg_font_sans_16,QG_DEFAULT,1), f_title=QG_FONT_INIT(qg_font_sans_bold_24,QG_YELLOW,1);
static void mk(qg_screen_t*s,int w,int h,const qg_backend_t*b,int buf){ memset(s,0,sizeof *s); s->width=w;s->height=h; qg_view_reset(s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(s)) k_++; qg_hist_owner[k_] = (s); (s)->hist = qg_hist_pool[k_]; (s)->hist_cap = QG_TEXT_HISTORY_LINES; }s->ready=true;s->fg_color=QG_WHITE;s->bg_color=QG_BLACK;s->line_width=1;s->text_bg=QG_TRANSPARENT;s->tab_width=40;s->wrap=true;s->scroll=true;
 qg_palette_copy_standard(s->palette); s->backend=b; if(buf){s->fb=fb; s->backend=&qg_backend_buf8;} qg_screen_set_font(s,0,&f_body); qg_screen_set_font(s,1,&f_title);}
void qg_screen_set_colors(qg_screen_t *s, qg_color_t fg, qg_color_t bg){ if(fg<=254)s->fg_color=fg; if(bg<=254)s->bg_color=bg; }
void qg_screen_flush_all(qg_screen_t *s){ qg_int_dirty_all(s); s->backend->flush(s); }
static int fails; static void check(int ok,const char*m){ printf("%s  %s\n",ok?"PASS":"FAIL",m); if(!ok)fails++; }
static uint32_t rng; static int roll(void){ rng=rng*1103515245u+12345u; return (int)((rng>>16)%20)+1; }
static void runlog(qg_screen_t*s){ char b[48]; qg_cls(s,QG_BLACK); qg_locate(s,6,4); rng=7; qg_println(s,"{f:1}Roll log");
  for(int i=1;i<=40;i++){ int r=roll(); qg_screen_set_colors(s,QG_LIGHTGRAY,QG_DEFAULT); snprintf(b,sizeof b,"#%02d  d20 = ",i); qg_print(s,b);
    qg_screen_set_colors(s,QG_WHITE,QG_DEFAULT); snprintf(b,sizeof b,"{c:%s}%d",r==20?"LIGHTGREEN":r==1?"LIGHTRED":"WHITE",r); qg_println(s,b);} }
int main(void){
  qg_screen_t s, t;
  int dims[2][2]={{240,320},{320,480}};
  for(int k=0;k<2;k++){ int w=dims[k][0],h=dims[k][1];
    mk(&s,w,h,NULL,1); runlog(&s); qg_screen_flush_all(&s);
    mk(&t,w,TH,&tback,0); runlog(&t); int bottom=t.cursor_y; int diff=0;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++) if(panel[y][x]!=tall[bottom-h+y][x]) diff++;
    char m[120]; snprintf(m,sizeof m,"D: %dx%d BUF8 scroll (memmove) after 40 lines == unscrolled reference (%d differing pixels)",w,h,diff); check(diff==0,m); }
  /* E: images */
  qg_image_t im[4]; const uint8_t*dat[4]={img_d20,img_potion,img_banner,img_landscape}; const uint32_t*sz[4]={&img_d20_size,&img_potion_size,&img_banner_size,&img_landscape_size}; int tr[4]={1,1,0,0};
  int cov_bad=0; double sumerr=0; long npx=0; int maxerr=0;
  int sizes[3][4]={{7,9,0,0},{-20,30,2,1},{100,-10,1,3}};
  for(int i=0;i<4;i++){ qg_image_open(&im[i],dat[i],*sz[i],tr[i]?QG_IMAGE_TRANSPARENT:0);
    for(int c=0;c<3;c++){ int x=sizes[c][0],y=sizes[c][1]; int dw=im[i].width*(sizes[c][2]?sizes[c][2]:1), dh=im[i].height*(sizes[c][3]?sizes[c][3]:1); if(!sizes[c][2]) {dw=im[i].width; dh=im[i].height;}
      qg_screen_t d; mk(&d,PW,PH,&dback,0); memset(dp,0xAB,sizeof dp); qg_image_draw_scaled(&d,&im[i],(int16_t)x,(int16_t)y,(int16_t)dw,(int16_t)dh);
      static uint8_t cov1[PW*PH], cov2[PW*PH];
      mk(&s,PW,PH,NULL,1); memset(fb,3,sizeof fb); qg_image_draw_scaled(&s,&im[i],(int16_t)x,(int16_t)y,(int16_t)dw,(int16_t)dh); memcpy(cov1,fb,sizeof fb);
      memset(fb,5,sizeof fb); qg_image_draw_scaled(&s,&im[i],(int16_t)x,(int16_t)y,(int16_t)dw,(int16_t)dh); memcpy(cov2,fb,sizeof fb);
      for(int p=0;p<PW*PH;p++){ int bufcov = !(cov1[p]==3 && cov2[p]==5); int dircov = dp[p/PW][p%PW]!=0xABAB; if(bufcov!=dircov) cov_bad++;
        if(dircov){ uint16_t a=dp[p/PW][p%PW], b=s.palette[cov1[p]];
          int er=abs(((a>>11)&31)*8-((b>>11)&31)*8), eg=abs(((a>>5)&63)*4-((b>>5)&63)*4), eb=abs((a&31)*8-(b&31)*8);
          int e=er>eg?(er>eb?er:eb):(eg>eb?eg:eb); if(e>maxerr)maxerr=e; sumerr+=e; npx++; } } } }
  char m[160]; snprintf(m,sizeof m,"E: 12 image draws: BUF8 covers exactly the pixels DIRECT does (%d mismatches)",cov_bad); check(cov_bad==0,m);
  snprintf(m,sizeof m,"E: standard palette: colours within one palette step (51); average error %.1f, worst %d",sumerr/npx,maxerr); check(maxerr<=51,m);
  /* exact colours after loading the image's palette */
  { mk(&s,PW,PH,NULL,1); int n=qg_palette_load_image(&s,&im[3],16); qg_screen_t d; mk(&d,PW,PH,&dback,0);
    qg_image_draw(&d,&im[3],0,0); qg_image_draw(&s,&im[3],0,0); int bad=0;
    for(int y=0;y<im[3].height;y++)for(int x=0;x<im[3].width;x++) if(s.palette[fb[y*PW+x]]!=dp[y][x]) bad++;
    snprintf(m,sizeof m,"E: after qg_palette_load_image (%d colours at 16+), the landscape matches DIRECT exactly (%d differing)",n,bad); check(bad==0,m);
    check(s.palette[QG_WHITE]==QG_RGB565(255,255,255)&&s.palette[QG_RED]==QG_RGB565(0xAA,0,0),"E:   ...and the 16 named colours are untouched"); }
  /* cache invalidation */
  mk(&s,PW,PH,NULL,1); qg_image_draw(&s,&im[3],0,0); uint8_t before=fb[50*PW+50];
  qg_palette_set(&s,before,255,0,0);  /* change the used entry to red: nearest match for that pixel should now move */
  qg_image_draw(&s,&im[3],0,0); uint8_t after=fb[50*PW+50];
  snprintf(m,sizeof m,"E: after qg_palette_set, the colour-match cache is rebuilt (index %u -> %u)",before,after); check(before!=after,m);
  printf(fails?"\n%d FAILED\n":"\nALL PASS (D-E)\n",fails); return fails; }
