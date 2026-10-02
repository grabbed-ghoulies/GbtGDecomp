# Milestones

The queue does not cover all of `.text`. Stop the supervisor when you want to. These checks are the progress bar, in order.

1. **frame_tick callees.** Pad `0x000FA3C0` and `0x000FAD80`, audio `0x0015B780`, XACT `0x0003A040`, `0x000396F0`, `0x00038640`, `0x00038C20`, and present `0x00033480`, `0x00032AB0`, `0x00033A90`. Those tasks start in `queue/open/`.
2. **Script VM.** Scene order can leave the attract reel.
3. **One gameplay scene.** That scene's actor tick runs.

A milestone is done when its tasks are in `queue/done/` and `gbtg --headless --frames 1` still exits 0.
