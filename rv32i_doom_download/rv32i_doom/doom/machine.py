"""Python ownership wrapper for the native C interpreter, not the game engine."""
import ctypes as C
import pathlib


class Machine:
    def __init__(self, image="build/doom/doom.bin", wad="doom/assets/freedoom1.wad"):
        self.lib = C.CDLL(str(pathlib.Path("build/libdoomhost.so").resolve()))
        self.lib.dh_create.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t]
        self.lib.dh_create.restype = C.c_void_p
        self.lib.dh_destroy.argtypes = [C.c_void_p]; self.lib.dh_destroy.restype = None
        self.lib.dh_run.argtypes = [C.c_void_p, C.c_uint32, C.c_uint32, C.c_uint32]
        self.lib.dh_run.restype = C.c_int
        self.lib.dh_report.argtypes = [C.c_void_p, C.c_uint]; self.lib.dh_report.restype = C.c_uint32
        self.lib.dh_boot_word.argtypes = [C.c_void_p]; self.lib.dh_boot_word.restype = C.c_uint32
        for name, result in (("dh_frame", C.c_void_p), ("dh_steps", C.c_uint64),
                             ("dh_frames", C.c_uint32), ("dh_exit_code", C.c_uint32),
                             ("dh_error", C.c_char_p), ("dh_log", C.c_char_p)):
            fn = getattr(self.lib, name); fn.argtypes = [C.c_void_p]; fn.restype = result
        image_path = pathlib.Path(image); wad_path = pathlib.Path(wad)
        if not 0 < image_path.stat().st_size <= 0x180000 or not 12 <= wad_path.stat().st_size <= 0x1f00000:
            raise RuntimeError("guest image or IWAD exceeds the reserved loading region")
        code = image_path.read_bytes(); data = wad_path.read_bytes()
        self.original_boot_word = int.from_bytes(code[:4], "little")
        a = C.create_string_buffer(code); b = C.create_string_buffer(data)
        self.pointer = self.lib.dh_create(a, len(code), b, len(data))
        if not self.pointer:
            raise RuntimeError("invalid/oversized guest image or IWAD, or host allocation failed")

    def run(self, keys, milliseconds, budget=200000):
        event = self.lib.dh_run(self.pointer, keys, milliseconds & 0xffffffff, budget)
        if event == 4:
            raise RuntimeError(self.lib.dh_error(self.pointer).decode())
        if event == 3 and self.lib.dh_exit_code(self.pointer) != 0:
            raise RuntimeError(f"guest exited with code {self.lib.dh_exit_code(self.pointer)}")
        return event

    @property
    def pose(self):
        values = [self.lib.dh_report(self.pointer, i) for i in range(4)]
        if values[0] != 0x504c4159:
            return None
        return tuple(v - (1 << 32) if v >= (1 << 31) else v for v in values[1:3]) + (values[3],)

    def check_boot_code(self):
        assert self.lib.dh_boot_word(self.pointer) == self.original_boot_word, "guest startup code was overwritten"

    @property
    def steps(self):
        return self.lib.dh_steps(self.pointer)

    @property
    def log(self):
        return self.lib.dh_log(self.pointer).decode(errors="replace")

    def frame(self):
        raw = C.string_at(self.lib.dh_frame(self.pointer), 320 * 200 * 4)
        rgb = bytearray(320 * 200 * 3)
        rgb[0::3] = raw[2::4]; rgb[1::3] = raw[1::4]; rgb[2::3] = raw[0::4]
        return b"P6\n320 200\n255\n" + rgb

    def close(self):
        if self.pointer:
            self.lib.dh_destroy(self.pointer); self.pointer = None
