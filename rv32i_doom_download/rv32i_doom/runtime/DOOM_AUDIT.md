# Focused source audit

Read from ozkl/doomgeneric commit
`e257a09527062bfb986b6e5e0780839b6cbd5b71` on 21 November 2026.
This is a focused audit of the next runtime requirements, not a complete
link-dependency inventory or proof that the engine can already link.

| Inspected source | What it actually does | Our milestone / remaining adapter |
|---|---|---|
| i_system.c, AutoAllocMemory/I_ZoneBase | Requests an arena through malloc; this revision's DEFAULT_RAM and MIN_RAM macros are both 6 MiB | Small malloc arena verified; final engine memory map must be much larger |
| z_zone.c | Manages the engine's tagged zone allocations and coalescing | Preserve this layer; our allocator is not a substitute for Z_* semantics |
| w_file_stdc.c | WAD backend uses fopen/fseek/fread/fclose | Memory-backed random read and validated WAD fixture implemented; engine backend not yet connected |
| w_wad.c, W_CheckNumForName | Searches entries backwards for overriding names | Our lookup tests later duplicate wins; exact-name contract is narrower |
| m_fixed.h/m_fixed.c | Q16.16 values; wide products/division; engine overflow policies | RV32I software integer helpers tested; checked fixed API requires future policy adapter |

Pinned primary-source links:

- [i_system.c](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/i_system.c)
- [z_zone.c](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/z_zone.c)
- [w_file_stdc.c](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/w_file_stdc.c)
- [w_wad.c](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/w_wad.c)
- [m_fixed.h](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/m_fixed.h)
- [m_fixed.c](https://github.com/ozkl/doomgeneric/blob/e257a09527062bfb986b6e5e0780839b6cbd5b71/doomgeneric/m_fixed.c)

The i_system.c explanatory comment mentions a different default; the actual
macros above determine this revision's compiled default. Six MiB is the zone
request, not total game RAM. It excludes executable, framebuffer, assets and
other allocations. Our 512 KiB test map is deliberately a runtime test map.

No Doom source files or game assets were imported into the runtime program.
The new code supplies educational primitives and its own synthetic fixture.
Missing libc functions, full file/formatting interfaces, engine-specific
allocation/arithmetic policies and the actual Doom integration remain explicit
work. Multiplayer, sound and performance optimization are also later work.
