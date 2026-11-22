#!/usr/bin/env python3
import argparse
import hashlib
import pathlib
import time
from machine import Machine


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frames", type=int, default=48)
    parser.add_argument("--budget", type=int, default=3000000000)
    parser.add_argument("--move", action="store_true", help="hold forward after frame 48")
    args = parser.parse_args()
    if args.frames < 1 or args.budget < 1:
        parser.error("positive frame count and budget required")
    m = Machine()
    now = 0; frames = 0; started = time.monotonic(); announced = 0
    try:
        while frames < args.frames:
            if m.steps >= args.budget:
                raise RuntimeError("host instruction budget exhausted before target frame")
            event = m.run(4 if args.move and frames >= 48 else 0, now,
                          min(200000, args.budget - m.steps))
            if event == 2:
                now += 1
            elif event == 1:
                m.check_boot_code()
                frames += 1; now += 29
                frame = m.frame()
                pathlib.Path("build/doom/frame.ppm").write_bytes(frame)
                print(f"frame={frames} steps={m.steps} pose={m.pose} sha256={hashlib.sha256(frame).hexdigest()}", flush=True)
            elif event == 3:
                raise RuntimeError("guest exited before target frame")
            if m.steps - announced > 20000000:
                announced = m.steps
                print(f"progress steps={m.steps} elapsed={time.monotonic()-started:.1f}s", flush=True)
        pathlib.Path("build/doom/engine.log").write_text(m.log)
        print(f"PASS guest produced {frames} frames through RV32I interpreter", flush=True)
    finally:
        pathlib.Path("build/doom/engine.log").write_text(m.log)
        m.close()


if __name__ == "__main__":
    main()
