#!/usr/bin/env python3
"""Thin display/input frontend. Guest RV32I instructions draw every pixel."""
import argparse
import ctypes as C
import pathlib
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--smoke", action="store_true", help="exercise real Tk key events and image presentation, then exit")
    args = parser.parse_args()
    try:
        import tkinter as tk
        root = tk.Tk()
    except (ImportError, RuntimeError) as error:
        print(f"Tk unavailable: {error}. Use ./run_square.sh --headless", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"Cannot open display: {error}. Use ./run_square.sh --headless", file=sys.stderr)
        return 1
    lib = C.CDLL(str(pathlib.Path("build/libplatform.so").resolve()))
    lib.pl_create.argtypes = [C.c_void_p, C.c_size_t]; lib.pl_create.restype = C.c_void_p
    lib.pl_destroy.argtypes = [C.c_void_p]; lib.pl_destroy.restype = None
    lib.pl_run.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32]; lib.pl_run.restype = C.c_int
    lib.pl_frame.argtypes = [C.c_void_p]; lib.pl_frame.restype = C.c_void_p
    lib.pl_error.argtypes = [C.c_void_p]; lib.pl_error.restype = C.c_char_p
    lib.pl_exit_code.argtypes = [C.c_void_p]; lib.pl_exit_code.restype = C.c_uint32
    raw = pathlib.Path("build/square.bin").read_bytes()
    image = C.create_string_buffer(raw)
    cpu = lib.pl_create(image, len(raw))
    if not cpu:
        root.destroy(); raise RuntimeError("guest image could not be loaded")
    root.title("Our RV32I computer — WASD / arrows; Esc quits")
    label = tk.Label(root)
    label.pack()
    tk.Label(root, text="Guest CPU draws this image | WASD / arrows | Escape quits").pack()
    pressed = set()
    frames = 0
    exit_status = 0
    last_frame = time.monotonic()
    started = last_frame
    picture = None
    destroyed = False

    def close():
        nonlocal destroyed
        if not destroyed:
            destroyed = True
            root.destroy()

    def key(event, down):
        name = event.keysym.lower()
        if down: pressed.add(name)
        else: pressed.discard(name)

    root.bind("<KeyPress>", lambda event: key(event, True))
    root.bind("<KeyRelease>", lambda event: key(event, False))
    root.bind("<FocusOut>", lambda event: pressed.clear())
    root.protocol("WM_DELETE_WINDOW", close)

    def step():
        nonlocal frames, picture, exit_status, last_frame
        try:
            keys = 0
            for names, bit in (({"a", "left"}, 1), ({"d", "right"}, 2),
                               ({"w", "up"}, 4), ({"s", "down"}, 8), ({"escape"}, 16)):
                if pressed & names: keys |= bit
            now = int((time.monotonic() - started) * 1000) & 0xffffffff
            event = lib.pl_run(cpu, keys, now, 20000)
            if event == 1:
                frame = C.string_at(lib.pl_frame(cpu), 64000)
                rgb = bytearray(48000)
                rgb[0::3] = frame[2::4]; rgb[1::3] = frame[1::4]; rgb[2::3] = frame[0::4]
                source = tk.PhotoImage(data=b"P6\n160 100\n255\n" + rgb, format="PPM")
                picture = source.zoom(4, 4)
                label.configure(image=picture)
                frames += 1
                last_frame = time.monotonic()
                if args.smoke:
                    if frames == 1:
                        assert source.get(76, 46) == (255, 204, 0)
                        root.event_generate("<KeyPress>", keysym="d")
                    if frames == 5:
                        assert source.get(80, 46) == (255, 204, 0)
                        assert source.get(76, 46) == (16, 32, 48)
                        root.event_generate("<KeyRelease>", keysym="d")
                    if frames == 6:
                        assert source.get(80, 46) == (255, 204, 0)
                        root.event_generate("<KeyPress>", keysym="Escape")
            elif event == 3:
                assert lib.pl_exit_code(cpu) == 0
                if args.smoke:
                    assert frames == 6
                    print("PASS Tk image presentation, actual key press/release dispatch, movement and Escape")
                close(); return
            elif event == 4:
                raise RuntimeError(lib.pl_error(cpu).decode())
            if time.monotonic() - last_frame > 10:
                raise RuntimeError("guest produced no frame for 10 seconds")
            root.after(1, step)
        except Exception as error:
            exit_status = 1
            print(f"Square demo failed: {error}", file=sys.stderr)
            close()

    try:
        root.update()
        root.focus_force()
        root.after(0, step)
        root.mainloop()
    finally:
        lib.pl_destroy(cpu)
        if not destroyed: close()
    return exit_status


if __name__ == "__main__":
    raise SystemExit(main())
