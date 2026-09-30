# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""Turn the host renders into reference pictures: the hardware test pages
(tests/host/build/*.ppm) into tests/hardware/expected/, and the examples
(tests/host/build/ex/*.ppm) into examples/expected/.
Run tests/host/run_tests.sh first."""
import os
from PIL import Image
here = os.path.dirname(os.path.abspath(__file__))
build, out = os.path.join(here, "build"), os.path.join(here, "..", "hardware", "expected")
m = {}
for i in range(1, 7): m[f"dm_{i}"] = f"m2_page{i}_screenA"; m[f"pl_{i}"] = f"m2_page{i}_screenB"
m.update({"l0_dm": "m3_page1_upright_screenA", "l0_pl": "m3_page1_upright_screenB",
          "l1_dm": "m3_page1_sideways_screenA", "l1_pl": "m3_page1_sideways_screenB",
          "dial_dm": "m3_page3_dial_screenA", "dial_pl": "m3_page3_dial_screenB"})
for i in range(1, 4): m[f"p{i}_dm"] = f"m4_page{i}_screenA"; m[f"p{i}_pl"] = f"m4_page{i}_screenB"
for i in range(1, 5): m[f"q{i}_dm"] = f"m5_page{i}_screenA"; m[f"q{i}_pl"] = f"m5_page{i}_screenB"
for i in range(1, 5): m[f"r{i}_dm"] = f"m6_page{i}_screenA"; m[f"r{i}_pl"] = f"m6_page{i}_screenB"
m.update({"s0_dm": "m7_nopack_screenA", "s0_pl": "m7_nopack_screenB", "s1_dm": "m7_page1_screenA",
          "s1_pl": "m7_page1_screenB", "s2_dm": "m7_page2_screenA", "s2_pl": "m7_page2_screenB",
          "s3_dm": "m7_page3_screenA"})
for i in range(1, 6): m[f"t{i}_dm"] = f"m8_page{i}_screenA"; m[f"t{i}_pl"] = f"m8_page{i}_screenB"
m.update({"n1_a": "new_commands_page1_view_screenA", "n2_a": "new_commands_page2_styles_screenA",
          "n3_b": "new_commands_page3_getput_screenB", "n4_a": "new_commands_page4_preset_screenA"})
os.makedirs(out, exist_ok=True)
n = 0
for src, dst in m.items():
    p = os.path.join(build, src + ".ppm")
    if os.path.exists(p):
        Image.open(p).save(os.path.join(out, dst + ".png"), optimize=True)
        n += 1
print("%d reference images written to tests/hardware/expected/" % n)

# The examples: tests/host/build/ex/*.ppm -> examples/expected/*.png
ex_build = os.path.join(build, "ex")
ex_out = os.path.join(here, "..", "..", "examples", "expected")
os.makedirs(ex_out, exist_ok=True)
k = 0
for f in sorted(os.listdir(ex_build)) if os.path.isdir(ex_build) else []:
    if f.endswith(".ppm"):
        Image.open(os.path.join(ex_build, f)).save(os.path.join(ex_out, f[:-4] + ".png"), optimize=True)
        k += 1
print("%d example images written to examples/expected/" % k)
