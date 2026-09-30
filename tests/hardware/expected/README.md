# Expected screens

What each hardware test page should look like, drawn by the real library code on a PC (see `tests/host`). Flash a test, then compare the screen with its picture here.

Names say which program, which page and which screen: `m5_page2_screenA.png` is page 2 of `qg4p_m5`, on screen A.

**Before you compare:**
- **Colours will look a little different.** Cheap panels, viewing angles and your room's lighting all shift them; shapes, positions and text should match exactly.
- **Screen B pictures are for the 3.5" 320x480 board.** With the 2.8" 240x320 board as screen B, layouts drawn in percentages match, while fixed-position items sit differently.
- **Animated pages show their final frame.** (The M8 dice, the M3 dial and the XOR slide end where these pictures show.)
- **Some pages include random numbers** (dice rolls), which will differ from run to run.

Regenerate after changing a test page: `sh tests/host/run_tests.sh`, then `python3 tests/host/make_expected.py`.
