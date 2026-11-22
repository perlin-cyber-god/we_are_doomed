"""Desktop aliases to platform key bits; no GUI or guest execution here."""
KEY_GROUPS = (
    ({"a", "left"}, 1), ({"d", "right"}, 2), ({"w", "up"}, 4),
    ({"s", "down"}, 8), ({"control_l", "control_r", "f"}, 16),
    ({"space", "e"}, 32), ({"return"}, 64), ({"escape"}, 128),
    ({"tab"}, 256), ({"shift_l", "shift_r"}, 512),
    ({"alt_l", "alt_r"}, 1024),
) + tuple(({str(i)}, 1 << (10 + i)) for i in range(1, 8)) + (
    ({"y"}, 1 << 18), ({"n"}, 1 << 19), ({"backspace"}, 1 << 20),
)


def key_mask(held):
    result = 0
    for names, bit in KEY_GROUPS:
        if held & names:
            result |= bit
    return result
