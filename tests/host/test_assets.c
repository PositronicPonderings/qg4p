/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qg_asset.h"
#include "qg_image.h"
#include "demo_images.h"
char __flash_binary_end;
static uint8_t *load(const char *p, long *n){ FILE*f=fopen(p,"rb"); fseek(f,0,SEEK_END); *n=ftell(f); fseek(f,0,SEEK_SET); uint8_t*b=malloc(*n); if(fread(b,1,*n,f)!=(size_t)*n) exit(1); fclose(f); return b; }
static int fails=0;
#define CHECK(c,msg) do{ if(c) printf("PASS  %s\n",msg); else {printf("FAIL  %s\n",msg); fails++;} }while(0)
int main(void){
  long n; uint8_t *pk = load("pack/assets.bin",&n);
  qg_asset_t a;
  CHECK(qg_asset_find("dice/d20.bmp",&a)==QG_ASSET_ERR_NOT_READY, "lookup before init is refused");
  CHECK(qg_asset_init_at(pk)==QG_ASSET_OK, "pack opens");
  CHECK(qg_asset_count()==6 && qg_asset_pack_size()==(uint32_t)n, "6 files, size matches the file");
  for(uint16_t i=0;i<qg_asset_count();i++){ qg_asset_get(i,&a); printf("      %-22s %6u bytes  type %u  flags %u\n",a.name,a.size,a.type,a.flags); }
  CHECK(qg_asset_verify()==QG_ASSET_OK, "CRC-32 matches");
  struct { const char*name; const uint8_t*ref; uint32_t size; int transp; } imgs[]={
    {"dice/d20.bmp",img_d20,img_d20_size,1},{"items/potion.bmp",img_potion,img_potion_size,1},
    {"art/banner.bmp",img_banner,img_banner_size,0},{"art/landscape.bmp",img_landscape,img_landscape_size,0},
    {"ui/d20_prebuilt.bmp",img_d20,img_d20_size,1}};
  for(int i=0;i<5;i++){ char msg[96]; int ok = qg_asset_find(imgs[i].name,&a)==QG_ASSET_OK && a.size==imgs[i].size && memcmp(a.data,imgs[i].ref,a.size)==0
       && a.type==QG_ASSET_TYPE_IMAGE && ((a.flags&QG_ASSET_FLAG_TRANSPARENT)!=0)==imgs[i].transp;
    qg_image_t im; ok = ok && qg_image_open(&im,a.data,a.size,(a.flags&QG_ASSET_FLAG_TRANSPARENT)?QG_IMAGE_TRANSPARENT:0)==QG_OK;
    snprintf(msg,sizeof msg,"%s: identical to the M6 array, right flag, opens as an image",imgs[i].name); CHECK(ok,msg);
    CHECK(((uintptr_t)a.data - (uintptr_t)pk) % 4 == 0, "  ...and starts on a 4-byte boundary"); }
  CHECK(qg_asset_find("text/welcome.txt",&a)==QG_ASSET_OK && a.type==QG_ASSET_TYPE_TEXT && memcmp(a.data,"Welcome",7)==0, "text file found, type text");
  CHECK(qg_asset_find("dice/d21.bmp",&a)==QG_ASSET_ERR_NOT_FOUND, "missing name -> not found");
  CHECK(qg_asset_find("Dice/d20.bmp",&a)==QG_ASSET_ERR_NOT_FOUND, "names are case-sensitive");
  CHECK(qg_asset_find("",&a)==QG_ASSET_ERR_NOT_FOUND && qg_asset_find(NULL,&a)==QG_ASSET_ERR_NOT_FOUND, "empty and NULL names -> not found");
  /* damage */
  uint8_t *bad = malloc(n); memcpy(bad,pk,n); bad[n-100]^=0x01; qg_asset_init_at(bad);
  CHECK(qg_asset_verify()==QG_ASSET_ERR_CORRUPT, "one flipped bit -> CRC mismatch detected");
  memcpy(bad,pk,n); bad[32+36+2]=0x7F; CHECK(qg_asset_init_at(bad)==QG_ASSET_ERR_CORRUPT, "entry pointing outside the pack -> refused at init");
  CHECK(qg_asset_find("dice/d20.bmp",&a)==QG_ASSET_ERR_NOT_READY, "  ...and lookups then refused");
  memset(bad,0xFF,n); CHECK(qg_asset_init_at(bad)==QG_ASSET_ERR_NO_PACK, "erased flash (all 0xFF) -> no pack");
  memcpy(bad,pk,n); bad[4]=2; CHECK(qg_asset_init_at(bad)==QG_ASSET_ERR_VERSION, "newer version -> version error");
  printf(fails? "\n%d FAILED\n":"\nALL PASS\n", fails); return fails; }
