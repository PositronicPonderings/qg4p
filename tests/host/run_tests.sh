#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QG4P host tests: run the library on a PC, no Pico needed.
#
#  Needs: gcc, python3, Pillow and numpy  (pip install pillow numpy)
#  Run:   sh tests/host/run_tests.sh      (from anywhere)
#
#  The drawing code runs against fake screens that record pixels instead of
#  sending them over SPI. Tests compare the results with independent
#  references, with exact expectations, and with "golden" fingerprints of
#  every page of the hardware test programs.
# ---------------------------------------------------------------------------
cd "$(dirname "$0")" || exit 1
L=../../qg4p; H=../hardware; B=build
mkdir -p $B
CF="-std=c11 -O1 -w -Istubs -I$L -I$L/assets -I$H -DQG_ASSET_HOST_TEST"
BASE="$L/qg_draw.c $L/qg_draw_pct.c $L/qg_block.c $L/qg_palette.c $L/qg_text.c $L/qg_image.c $L/fonts/qg_font_mono_12.c $L/fonts/qg_font_sans_16.c $L/fonts/qg_font_sans_bold_24.c"
BUF8="$L/backend/qg_backend_buf8.c"
pass=0; fail=0

build() {   # build <binary> <sources...>
    out=$1; shift
    gcc $CF -o $B/$out "$@" -lm 2> $B/$out.build.log || { echo "BUILD FAIL  $out (see $B/$out.build.log)"; fail=$((fail+1)); return 1; }
}
check() {   # check <name> <command...>   (run inside build/)
    name=$1; shift
    if (cd $B && "$@") > $B/$name.log 2>&1; then echo "PASS  $name"; pass=$((pass+1))
    else echo "FAIL  $name (see tests/host/$B/$name.log)"; fail=$((fail+1)); fi
}

echo "Building..."
build test_text_units   test_text_units.c $L/qg_draw.c $L/qg_draw_pct.c $L/qg_palette.c
build test_line_widths  test_line_widths.c $L/qg_draw.c
build test_scroll       test_scroll.c $BASE
build imgtest           imgtest.c $L/qg_image.c $L/qg_draw.c $L/qg_palette.c
build test_assets       test_assets.c $L/assets/qg_asset.c $L/qg_image.c $L/qg_draw.c $L/qg_palette.c $H/demo_images.c
build test_buf8_a       test_buf8_a.c $BASE $BUF8
build test_buf8_b       test_buf8_b.c $BASE $BUF8 $H/demo_images.c
build test_buf8_overlap test_buf8_overlap.c $BASE $BUF8 $H/demo_images.c
build test_new_commands test_new_commands.c $BASE $BUF8 $H/demo_images.c
build test_flash        test_flash.c $L/qg_flash.c $L/qg_palette.c
build render_m2         render_m2.c screenstub.c $BASE
build render_m3         render_m3.c $BASE
build render_m4         render_m4.c $BASE
build render_m5         render_m5.c $BASE
build render_m6         render_m6.c $BASE $H/demo_images.c
build render_m7         render_m7.c $BASE $L/assets/qg_asset.c
build render_m8         render_m8.c $BASE $BUF8 $H/demo_images.c
build render_new        render_new.c $BASE $BUF8 $H/demo_images.c

# The examples run unchanged against stand-in screens (render_example.c),
# each stopped at a representative moment. STOP = calls to sleep_ms().
EX="hello:1 shapes:1 text:25 layout:1 images:1 two_screens:40 asset_pack:1 animation_direct:70 framebuffer:70 palette_effects:40 paint:14 sprites:60 dashboard:160 dice_roller:150 colour_check:1 calibrate:1"
E=../../examples
EXCF="-std=c11 -O1 -w -Istubs_examples -Istubs -I$L -I$L/assets -I$E -DQG_ASSET_HOST_TEST"
EXLIB="$BASE $BUF8 $L/assets/qg_asset.c $E/example_art.c"
mkdir -p $B/ex
for ex in $EX; do
    n=${ex%%:*}; extra=""
    [ $n = asset_pack ] && extra="-Dqg_asset_init=host_asset_init"
    gcc $EXCF -Dmain=example_main $extra -c $E/$n.c -o $B/ex/$n.o 2> $B/ex/$n.build.log &&
    gcc $EXCF -o $B/ex/r_$n render_example.c $B/ex/$n.o $EXLIB -lm 2>> $B/ex/$n.build.log ||
    { echo "BUILD FAIL  example $n (see $B/ex/$n.build.log)"; fail=$((fail+1)); }
done

echo "Preparing test data..."
(cd $B/ex && python3 ../../../../tools/mkpack.py ../../../../examples/pack --out example_pack/assets > pack.log 2>&1) || echo "  (example pack failed: see $B/ex/pack.log)"
(cd $B && python3 ../../../tools/mkpack.py ../../hardware/pack --out pack/assets > pack.log 2>&1) || echo "  (mkpack failed: see $B/pack.log)"
(cd $B && python3 ../make_image_cases.py > cases.log 2>&1) || echo "  (image cases failed: see $B/cases.log)"
cp test_images.py test_images_delta.py $B/

echo "Running..."
check text_units        ./test_text_units
check line_widths       ./test_line_widths
check scroll_exact      ./test_scroll
check images_vs_pillow  sh -c 'python3 test_images.py < cases.txt'
check images_rle_delta  python3 test_images_delta.py
check asset_pack        ./test_assets
check buf8_equivalence  ./test_buf8_a
check buf8_scroll_image ./test_buf8_b
check buf8_overlap      ./test_buf8_overlap
check new_commands      ./test_new_commands
check flash             ./test_flash
check render_pages      sh -c 'rm -f *.ppm; for r in render_m2 render_m3 render_m4 render_m5 render_m6 render_m7 render_m8 render_new; do ./$r || exit 1; done'
check render_examples   sh -c 'cd ex && rm -f *.ppm && for ex in '"$EX"'; do n=${ex%%:*}; ./r_$n ${ex##*:} $n || exit 1; done && NOPACK=1 ./r_asset_pack 1 asset_pack_nopack && ./r_layout 2 layout_sideways && ./r_dice_roller 75 dice_roller_midroll && ./r_colour_check 2 colour_check_diagnostics && QG_KEYS="b++++++++[[[[" ./r_calibrate 1 calibrate_adjusted'
check golden_images     sh -c 'sha256sum -c ../golden.sha256 --quiet'
check manual_examples   python3 ../doc_examples.py --check
check manual_links      python3 ../doc_links.py
check ai_disclosure     python3 ../check_disclosure.py
check quick_reference   python3 ../check_quickref.py

echo "----"
echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
