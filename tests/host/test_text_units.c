/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* Unit tests for the text engine's internals: kerning (both forms), the four
 * character-map forms, and UTF-8 decoding. Includes qg_text.c directly so it
 * can reach its static functions. */
#include <stdio.h>
#include "../../qg4p/qg_text.c"
static int fails = 0;
#define EXPECT(got, want, what) do { long g_ = (long)(got), w_ = (long)(want); \
    if (g_ != w_) { printf("FAIL  %s: got %ld, expected %ld\n", what, g_, w_); fails++; } } while (0)
int main(void)
{
    lv_font_fmt_txt_dsc_t d = {0};
    static const uint8_t ids[] = { 3,7, 5,2, 5,9, 8,1 };
    static const int8_t vals[] = { -10, 4, -32, 7 };
    lv_font_fmt_txt_kern_pair_t kp = { .glyph_ids = ids, .values = vals, .pair_cnt = 4, .glyph_ids_size = 0 };
    d.kern_dsc = &kp; d.kern_classes = 0; d.kern_scale = 16;
    EXPECT(kerning(&d,5,9), -32, "kern pairs (5,9)");  EXPECT(kerning(&d,3,7), -10, "kern pairs (3,7)");
    EXPECT(kerning(&d,8,1),   7, "kern pairs (8,1)");  EXPECT(kerning(&d,5,3),   0, "kern pairs (5,3) absent");
    static const uint16_t ids16[] = { 300,7, 300,400, 1000,2 };
    static const int8_t v16[] = { 1, -5, 9 };
    lv_font_fmt_txt_kern_pair_t kp16 = { .glyph_ids = ids16, .values = v16, .pair_cnt = 3, .glyph_ids_size = 1 };
    d.kern_dsc = &kp16;
    EXPECT(kerning(&d,300,400), -5, "kern pairs16 (300,400)"); EXPECT(kerning(&d,1000,2), 9, "kern pairs16 (1000,2)");
    static const uint8_t lmap[] = {0,1,2,0}, rmap[] = {0,2,1,1};
    static const int8_t cv[] = { -8, 3, 5, -12 };
    lv_font_fmt_txt_kern_classes_t kc = { .class_pair_values=cv, .left_class_mapping=lmap, .right_class_mapping=rmap,
                                          .left_class_cnt=2, .right_class_cnt=2 };
    d.kern_dsc = &kc; d.kern_classes = 1; d.kern_scale = 32;
    EXPECT(kerning(&d,1,1), 6, "kern classes (1,1)");   EXPECT(kerning(&d,2,1), -24, "kern classes (2,1)");
    EXPECT(kerning(&d,1,2), -16, "kern classes (1,2)"); EXPECT(kerning(&d,3,1), 0, "kern classes (3,1) none");
    static const uint8_t ofs8[] = {0,2,1};
    static const uint16_t ul[] = {0,5,9}, ofs16[] = {4,0,7};
    lv_font_fmt_txt_cmap_t cm[2] = {
        {.range_start=65,.range_length=3,.glyph_id_start=10,.glyph_id_ofs_list=ofs8,.type=LV_FONT_FMT_TXT_CMAP_FORMAT0_FULL},
        {.range_start=200,.range_length=10,.glyph_id_start=50,.unicode_list=ul,.glyph_id_ofs_list=ofs16,.list_length=3,
         .type=LV_FONT_FMT_TXT_CMAP_SPARSE_FULL}};
    d.cmaps = cm; d.cmap_num = 2;
    EXPECT(glyph_id(&d,'B'), 12, "cmap FORMAT0_FULL 'B'"); EXPECT(glyph_id(&d,'C'), 11, "cmap FORMAT0_FULL 'C'");
    EXPECT(glyph_id(&d,205), 50, "cmap SPARSE_FULL 205");  EXPECT(glyph_id(&d,209), 57, "cmap SPARSE_FULL 209");
    EXPECT(glyph_id(&d,204), 0, "cmap SPARSE_FULL 204 absent");
    const char *s = "A\xC2\xB0\xE2\x82\xAC\xF0\x9F\x8E\xB2\xC3";
    const uint32_t want[] = { 0x41, 0xB0, 0x20AC, 0x1F3B2, 0xFFFD, 0 };
    for (int i = 0; i < 6; i++) EXPECT(utf8_next(&s), want[i], "utf8 sequence");
    printf(fails ? "%d FAILED\n" : "text units: all pass\n", fails);
    return fails != 0;
}
