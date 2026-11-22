#!/usr/bin/env python3
"""Deterministic headless game test: compare idle and forward-input runs."""
import hashlib
import json
import pathlib
from machine import Machine


def play(moving):
    m = Machine()
    milliseconds = 0; frames = 0; checkpoints = {}; last_frame = None
    label = "forward" if moving else "idle"
    try:
        while frames < 56:
            if m.steps >= 3000000000:
                raise RuntimeError("game verification instruction budget exhausted")
            event = m.run(4 if moving and frames >= 48 else 0, milliseconds)
            if event == 2:
                milliseconds += 1
            elif event == 1:
                m.check_boot_code()
                frames += 1; milliseconds += 29
                last_frame = m.frame()
                if frames in (1, 48, 56):
                    checkpoints[frames] = {"steps": m.steps, "pose": m.pose,
                                           "sha256": hashlib.sha256(last_frame).hexdigest()}
                    print(label, frames, checkpoints[frames], flush=True)
            elif event == 3:
                raise RuntimeError("game exited before verification completed")
        assert m.pose is not None
        assert len(set(last_frame[15:])) > 32, "frame is unexpectedly blank"
        pathlib.Path(f"build/doom/{label}.ppm").write_bytes(last_frame)
        pathlib.Path(f"build/doom/{label}-engine.log").write_text(m.log)
        return checkpoints
    finally:
        m.close()


def main():
    idle = play(False)
    forward = play(True)
    assert idle[1] == forward[1] and idle[48] == forward[48], "deterministic prefix differed"
    assert idle[56]["pose"][:2] == idle[48]["pose"][:2], "idle player moved unexpectedly"
    assert forward[56]["pose"][:2] != idle[56]["pose"][:2], "forward input did not move player"
    assert forward[56]["sha256"] != idle[56]["sha256"], "view did not respond to movement"
    pathlib.Path("build/doom/game-verification.json").write_text(json.dumps({"idle": idle, "forward": forward}, indent=2))
    print("PASS actual Doom gameplay: stable idle pose, guest-computed movement and changed rendered view", flush=True)


if __name__ == "__main__":
    main()
