/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* Thick lines must be exactly their width: vertical and horizontal lines
 * of width 1..8 are drawn and measured. */
#include <stdio.h>
#include <string.h>
#include "qg_draw.h"
#define W 200
#define H 60
static unsigned char g[H][W];
static void f(qg_screen_t*s,int16_t x,int16_t y,int16_t w,int16_t h,qg_color_t c){(void)s;(void)c;for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++)g[j][i]=1;}
static const qg_backend_t tb={.name="T",.fill_rect=f};
int main(void){ qg_screen_t s; memset(&s,0,sizeof s); s.width=W;s.height=H; qg_view_reset(&s); { static qg_text_line_t qg_hist_pool[4][QG_TEXT_HISTORY_LINES]; static const void *qg_hist_owner[4]; int k_ = 0; while (k_ < 3 && qg_hist_owner[k_] && qg_hist_owner[k_] != (const void *)(&s)) k_++; qg_hist_owner[k_] = (&s); (&s)->hist = qg_hist_pool[k_]; (&s)->hist_cap = QG_TEXT_HISTORY_LINES; }s.ready=true;s.backend=&tb;s.fg_color=QG_WHITE;
  int fails=0;
  for(int t=1;t<=8;t++){ memset(g,0,sizeof g); s.line_width=(uint8_t)t;
    qg_line(&s,50,5,50,40,QG_WHITE);                    /* vertical  */
    int wv=0; for(int x=0;x<W;x++) wv+=g[20][x];
    memset(g,0,sizeof g); qg_line(&s,10,30,150,30,QG_WHITE); /* horizontal */
    int wh=0; for(int y=0;y<H;y++) wh+=g[y][80];
    if(wv!=t||wh!=t){ printf("FAIL  width %d: vertical %d, horizontal %d\n",t,wv,wh); fails++; } }
  printf(fails?"%d FAILED\n":"line widths 1-8: all exact\n",fails); return fails!=0; }
