#!/bin/sh
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# Convert the test images into BMPs and one C file for the M6 demo.
# Run from the project root:  sh tests/hardware/images/convert_test_images.sh
set -e
T=tools/img2bmp8.py
D=tests/hardware/images
python3 $T $D/d20.png       --no-dither --out $D/d20.bmp       --c-array img_d20
python3 $T $D/potion.png    --no-dither --out $D/potion.bmp    --c-array img_potion
python3 $T $D/banner.png                --out $D/banner.bmp    --c-array img_banner
python3 $T $D/landscape.png             --out $D/landscape.bmp --c-array img_landscape
{ printf '/* SPDX-License-Identifier: MIT-0 */\n/* SPDX-AI-Disclosure: ai-generated */\n/* SPDX-AI-Model: claude-opus-5-5 */\n/* SPDX-AI-Provider: Anthropic */\n'
  cat $D/img_d20.c $D/img_potion.c $D/img_banner.c $D/img_landscape.c; } > tests/hardware/demo_images.c
rm $D/img_*.c
echo "tests/hardware/demo_images.c written"
