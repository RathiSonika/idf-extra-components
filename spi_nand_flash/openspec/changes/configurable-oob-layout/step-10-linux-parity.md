# Step 10 — Linux `nand_impl_linux.c` parity

**PR identifier:** `oob-layout-10`  
**Depends on:** steps **01–09** (must include all marker paths touched on target)  
**Estimate:** ~250–600 LOC

## Goal

Mirror **every** `nand_impl.c` behavioral branch introduced for **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y`** into [`src/nand_impl_linux.c`](../../../src/nand_impl_linux.c):

- `nand_is_free`, `nand_prog`, `nand_is_bad`, `nand_mark_bad`, `nand_copy` (all five Dhara→OOB primitives per proposal §2.2).

With Kconfig **`n`**, Linux path remains **byte-identical** to current mmap emulator behavior.

## Mmap stride commitment (proposal §4 impacted-modules row — load-bearing)

The on-disk file layout is `pages_per_block × (page_size + chip.emulated_page_oob)` per block (today, see `src/nand_impl_linux.c:48–62` and baseline §4.7). With configurable OOB:

- The **formula stays the same**: `emulated_page_size = page_size + chip.emulated_page_oob`.
- `chip.emulated_page_oob` is driven by the active layout's `oob_bytes` (or, for default layout under step 03's Pattern A where `oob_bytes == 0`, the existing per-page-size table `16 / 64 / 128` for `512 / 2048 / 4096`).
- For the **default layout** this is byte-identical to today — no on-disk file format change. Existing host test fixtures and dump files keep working.
- For non-default layouts: `chip.emulated_page_oob` reflects whatever `oob_bytes` the layout declares; the file is reformatted on the next `nand_emul_init` (which already memsets to `0xFF` per baseline §4.7), so there is no migration concern within the host-test scope.
- Step 10 must **not** silently change the stride formula or the per-page-size defaults under Kconfig `n`.

## Known bug §11.4.1 — implement `nand_emul_get_stats` (**required in this step**)

[`known-bugs.md`](../../known-bugs.md) §11.4.1: `nand_emul_get_stats` is declared in [`include/nand_linux_mmap_emul.h`](../../../include/nand_linux_mmap_emul.h) when `CONFIG_NAND_ENABLE_STATS=y` but **never defined** → link errors for out-of-tree callers.

- **Implement** `nand_emul_get_stats` in [`src/nand_linux_mmap_emul.c`](../../../src/nand_linux_mmap_emul.c): read counters from `nand_mmap_emul_handle_t::stats` (same structure `nand_emul_clear_stats` uses) and write through the `size_t*` out-parameters.
- **Host test:** Add a case under `host_test/main/` that performs a deterministic number of emulated read/write/erase ops and asserts returned stats match (per known-bugs validation bullet).

## Non-goals

- Changing [`src/nand_linux_mmap_emul.c`](../../../src/nand_linux_mmap_emul.c) file layout under Kconfig `n`. Touch it only if a `y`-mode layout legitimately needs a different stride and the formula above produces it; comment the change clearly.

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl_linux.c`](../../../src/nand_impl_linux.c) | Conditional layout paths for all five primitives |
| [`priv_include/nand.h`](../../../priv_include/nand.h) | Already has device fields from step 05 — ensure Linux init calls the **same** layout-attach helper as the target path (may live in shared `src/nand_oob_device.c`) |
| [`src/nand_linux_mmap_emul.c`](../../../src/nand_linux_mmap_emul.c) | **`nand_emul_get_stats` implementation** (§11.4.1) + touch only if stride formula requires emulator changes |
| `host_test/main/*` | **New or extended test** for `nand_emul_get_stats` when stats Kconfig enabled |

## Implementation checklist

1. **Shared init:** Identify shared helper functions between target and Linux — **prefer** moving the layout-attach logic from step 05 into `src/nand_oob_device.c` (private header `priv_include/nand_oob_device.h`) so both `src/nand_impl.c::nand_init_device` and `src/nand_impl_linux.c::nand_init_device` call the same `nand_oob_attach_default_layout(handle)`. Avoids copy-paste drift.
2. **Stride wiring:** In `detect_chip` (Linux), once the layout pointer is attached, ensure `chip.emulated_page_oob` ends up consistent with `layout->oob_bytes` (or the per-page-size default if `oob_bytes == 0`). For default layout this means leaving the existing `16 / 64 / 128` mapping untouched.
3. **All five primitives:** Mirror the same `#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` branches added in steps 06 / 07 / 08 / 09. The Linux primitives use `nand_emul_read` / `nand_emul_write` (memcpy-backed, no DMA constraints), so the SPI-buffer-discipline rule from steps 06/07/08 does **not** apply here — a stack `uint8_t[4]` is fine for Linux. **Do not** copy that pattern back to the target path.
4. **`s_oob_used_page_markers` / `s_oob_mark_bad_markers` static arrays** (`src/nand_impl_linux.c:21–22`): under `y`, derive these from layout/field constants the same way step 07/08 do on target. Under `n`, leave them as-is.
5. Run **`host_test`** build with Kconfig `n` and `y`; sanity-check the on-disk file size matches `num_blocks × pages_per_block × (page_size + emulated_page_oob)` for both.

## Testing

- `idf.py --preview set-target linux && idf.py -C host_test build` (both `n` and `y` sdkconfigs).
- Run `pytest_nand_flash_linux.py` or documented host test command from baseline §8.2 for both Kconfig values (full suite — see step 11).

## Acceptance criteria

- [ ] **§11.4.1:** With `CONFIG_NAND_ENABLE_STATS=y`, **`nand_emul_get_stats` links** and host test passes (or document skip if CI never enables stats — prefer implementing test behind same Kconfig).
- [ ] Host tests pass **`n`** (CI default) — on-disk stride and marker bytes byte-identical to pre-change tree.
- [ ] Host tests pass **`y`** with default layout — on-disk stride matches the formula above; marker bytes byte-identical to `n`.
- [ ] All five primitives (`is_free`, `prog`, `is_bad`, `mark_bad`, `copy`) honor the layout under `y`.
- [ ] No divergent layout-attach code path between target and Linux (single shared helper).

## Risks

- Divergent init between Linux and target — extract **one** `nand_oob_device_attach_default(handle)` used by both. If two copies ever drift, the `y`-mode test matrix from step 11 catches it but only after the fact.
- Stride change under `y` going unnoticed because host tests reformat the file on every `nand_emul_init` — explicitly assert the byte size formula in PR description.

## Notes for implementers

- README convention: **do not merge** steps 06–09 to main without either **this step** in the same release train or an explicit "Linux broken with `y`" blocker issue — prefer completing **10** before announcing `y` experimental.
