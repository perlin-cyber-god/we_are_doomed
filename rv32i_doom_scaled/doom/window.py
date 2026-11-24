#!/usr/bin/env python3
import argparse
import pathlib
import sys
import time
from machine import Machine
from keys import key_mask


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--smoke", action="store_true", help="automatic Tk input test and exit")
    parser.add_argument("--scale", type=int, choices=range(1, 7), default=3,
                        help="integer display zoom, 1–6 (default: 3, 960x600); guest stays 320x200")
    args = parser.parse_args()
    display_width, display_height = 320 * args.scale, 200 * args.scale
    try:
        import tkinter as tk
        root = tk.Tk()
    except Exception as error:
        print(f"Display unavailable: {error}. Try ./run_doom.sh --headless", file=sys.stderr)
        return 1
    root.title("Doom on our RV32I interpreter — Freedoom Phase 1")
    screen = tk.Canvas(root, width=display_width, height=display_height, highlightthickness=0)
    screen.create_text(display_width // 2, display_height // 2, text="Starting the guest engine…\nFirst frame may take 20–90 seconds.", tags="boot")
    screen.pack()
    status = tk.Label(root, text="WASD/arrows: move/turn | Ctrl/F: fire | Space/E: use | Esc: menu | close window: exit")
    status.pack()
    m = Machine(); held = set(); picture = None; frames = 0; closed = False; result = 0
    image_item = None
    start = time.monotonic(); last_update = start; last_frame = start
    def close():
        nonlocal closed
        if not closed:
            closed = True; root.destroy()
    def key(event, down):
        name = event.keysym.lower()
        if down: held.add(name)
        else: held.discard(name)
    root.bind("<KeyPress>", lambda event: key(event, True))
    root.bind("<KeyRelease>", lambda event: key(event, False))
    root.bind("<FocusOut>", lambda event: held.clear())
    root.protocol("WM_DELETE_WINDOW", close)
    # GUI smoke uses the deterministic clock, normal play uses wall time.
    virtual_ms = 0; smoke_pose = None
    def step():
        nonlocal picture, frames, result, last_update, last_frame, virtual_ms, smoke_pose, image_item
        try:
            keys = key_mask(held)
            now = virtual_ms if args.smoke else int((time.monotonic() - start) * 1000)
            event = m.run(keys, now, 1000000)
            if event == 1:
                m.check_boot_code()
                frames += 1; last_frame = time.monotonic()
                source = tk.PhotoImage(data=m.frame(), format="PPM")
                picture = source.zoom(args.scale, args.scale)
                screen.delete("boot")
                if image_item is None:
                    image_item = screen.create_image(0, 0, image=picture, anchor="nw")
                else:
                    screen.itemconfigure(image_item, image=picture)
                if args.smoke:
                    virtual_ms += 29
                    if frames == 48:
                        smoke_pose = m.pose
                        assert smoke_pose is not None
                        root.event_generate("<KeyPress>", keysym="w")
                    if frames == 56:
                        assert m.pose[:2] != smoke_pose[:2]
                        root.event_generate("<KeyRelease>", keysym="w")
                        pathlib.Path("build/doom/gui-frame.ppm").write_bytes(m.frame())
                        print("PASS Doom Tk presentation and key-event driven guest movement", flush=True)
                        close(); return
            elif event == 2 and args.smoke:
                virtual_ms += 1
            elif event == 3:
                close(); return
            current = time.monotonic()
            if current - last_update > 0.2:
                status.configure(text=f"Guest instructions: {m.steps:,} | frames: {frames} | WASD/arrows, Ctrl/F, Space, Esc")
                last_update = current
            if current - last_frame > (180 if frames == 0 else 30):
                raise RuntimeError("guest frame watchdog expired; see build/doom/window-engine.log")
            root.after(1, step)
        except Exception as error:
            result = 1; print(f"Doom guest failed: {error}", file=sys.stderr); close()
    try:
        root.update(); root.focus_force(); root.after(0, step); root.mainloop()
    finally:
        pathlib.Path("build/doom/window-engine.log").write_text(m.log)
        m.close()
        if not closed: close()
    return result


if __name__ == "__main__":
    raise SystemExit(main())
