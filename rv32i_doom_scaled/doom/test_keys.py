"""Short keyboard alias regression, without Tk, windows or game execution."""
from keys import key_mask

assert key_mask(set()) == 0
for name in ("control_l", "control_r", "f"):
    assert key_mask({name}) == 16
for name in ("space", "e"):
    assert key_mask({name}) == 32
for number in range(1, 8):
    assert key_mask({str(number)}) == 1 << (10 + number)
assert key_mask({"w", "f", "2"}) == 4 | 16 | (1 << 12)
assert key_mask({"control_l", "f"}) == 16
assert key_mask({"unknown"}) == 0
print("PASS desktop fire/use aliases, all weapon bits, combinations and releases")
