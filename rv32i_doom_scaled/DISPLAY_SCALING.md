# Larger display, same guest framebuffer

Extract this complete release beside previous folders. All controls fixes are
included. The guest still renders 320x200 pixels; the default host zoom is now
3x, producing a 960x600 image. Use:

```bash
cd ~/Documents/DOOMed/rv32i_doom_scaled
./run_controls_fixed.sh --scale 3
# Or, if your desktop has room for a 1280x800 image:
./run_controls_fixed.sh --scale 4
```

Scale 2 restores 640x400. Scale 1 shows 320x200. Accepted values are 1 through
6. The status line and desktop window decorations add to these image sizes.
The regular ./run_doom.sh launcher also accepts --scale.

Integer zoom repeats each guest pixel in a square of host pixels. It makes
the picture larger, not more detailed. It does not change guest RAM layout,
pixel count, CPU ISA, engine image or input mapping. Larger host images do
take more host memory/display work, so identical wall-clock FPS is not promised.

True higher-resolution guest rendering is a separate integration task. The
engine build dimensions, framebuffer validation, host byte counts and Python
conversion sizes must all agree. A 640x400 four-byte framebuffer would occupy
1,024,000 bytes versus 256,000 for 320x200, and contain four times as many
pixels. No such guest resolution change is made in this release.

Verification: Python syntax checks; display --help; invalid scale 0 and 7
rejected before Tk is imported; shell syntax validation. CPU/guest/assets
compared byte-for-byte against the controls-fixed release. No GUI/Tk window,
engine startup, long tests or game regressions were executed for this change.
Existing verification logs are historical. Manually inspect the enlarged
window on your laptop when you launch normal play.
