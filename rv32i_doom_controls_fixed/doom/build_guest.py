#!/usr/bin/env python3
"""Cross-compile the pinned engine; never run it natively."""
import os
import pathlib
import re
import hashlib
import subprocess

root = pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)
cc = os.environ.get("RISCV_CC", "riscv64-unknown-elf-gcc")
pico = pathlib.Path(os.environ["PICOLIBC_ROOT"]).resolve()
headers = pico / "include"
library = pico / "lib/rv32i/ilp32"
build = root / "build/doom"
build.mkdir(parents=True, exist_ok=True)
common = ["-march=rv32i", "-mabi=ilp32", "-mno-relax", "-msmall-data-limit=0",
          "-ffreestanding", "-fno-builtin", "-fno-stack-protector", "-fno-pic",
          "-fno-pie", "-fno-strict-aliasing", "-fwrapv", "-ffunction-sections",
          "-fdata-sections", "-O2", "-g", "-isystem", str(headers), "-Idoom/engine",
          "-DDOOMGENERIC_RESX=320", "-DDOOMGENERIC_RESY=200", "-D_DEFAULT_SOURCE"]
makefile = pathlib.Path("doom/engine/Makefile").read_text()
names = re.search(r"^SRC_DOOM = (.*)$", makefile, re.M)[1].split()
sources = ["doom/engine/" + n.removesuffix(".o") + ".c" for n in names if n != "doomgeneric_xlib.o"]
sources += ["doom/port/guest.c", "doom/port/libc_glue.c"]
objects = []
header_bytes = b"".join(p.read_bytes() for p in sorted(pathlib.Path("doom/engine").glob("*.h")))
with (build / "compile.log").open("w") as log:
    for source in sources:
        obj = build / (pathlib.Path(source).stem + ".o")
        flags = ["-Wall", "-Wextra"]
        if source.startswith("doom/port/"):
            flags += ["-std=c11", "-Wpedantic", "-Werror", "-Wconversion", "-Wshadow"]
        digest = hashlib.sha256((cc + str(common + flags)).encode() + pathlib.Path(source).read_bytes() + header_bytes).hexdigest()
        stamp = obj.with_suffix(".stamp")
        diagnostics = obj.with_suffix(".diagnostics")
        if obj.exists() and stamp.exists() and diagnostics.exists() and stamp.read_text() == digest:
            log.write(diagnostics.read_text())
            objects.append(str(obj))
            continue
        result = subprocess.run([cc] + common + flags + ["-c", source, "-o", str(obj)],
                                text=True, capture_output=True)
        diagnostics.write_text(result.stdout + result.stderr)
        log.write(result.stdout + result.stderr)
        if result.returncode:
            print((result.stdout + result.stderr)[-5000:])
            raise SystemExit(result.returncode)
        stamp.write_text(digest)
        objects.append(str(obj))
start = build / "start.o"
subprocess.run([cc] + common + ["-c", "doom/port/start.S", "-o", str(start)], check=True)
elf = build / "doom.elf"
command = [cc, "-march=rv32i", "-mabi=ilp32", "-nostdlib", "-nostartfiles", "-static",
           "-Wl,-T,doom/port/linker.ld", "-Wl,--no-relax", "-Wl,--gc-sections",
           "-Wl,--build-id=none", "-Wl,-Map," + str(build / "doom.map"), str(start)]
command += objects + ["-L" + str(library), "-Wl,--start-group", "-lc", "-lm", "-lgcc", "-Wl,--end-group", "-o", str(elf)]
linked = subprocess.run(command, text=True, capture_output=True)
(build / "link.log").write_text(linked.stdout + linked.stderr)
if linked.returncode:
    print(linked.stdout + linked.stderr)
    raise SystemExit(linked.returncode)
copier = os.environ.get("RISCV_OBJCOPY", cc.removesuffix("-gcc") + "-objcopy")
subprocess.run([copier, "-O", "binary", str(elf), str(build / "doom.bin")], check=True)
print("Built guest", build / "doom.bin", (build / "doom.bin").stat().st_size, "bytes")
print("Upstream engine warnings:", (build / "compile.log").read_text().count("warning:"),
      "(retained in build/doom/compile.log; port code uses -Werror)")
