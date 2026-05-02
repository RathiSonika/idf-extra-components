# Step 09 — `nand_copy` marker / OOB correctness (both plane branches)

**PR identifier:** `oob-layout-09`  
**Depends on:** steps **01–08**  
**Estimate:** ~250–500 LOC

## Goal

Align [`src/nand_impl.c`](../../../src/nand_impl.c) **`nand_copy`** with the **two locked invariants** from proposal §2.2 / §7.0:

1. **Same-plane fast path (`src_column_addr == dst_column_addr`):** Today's code does **not** reprogram OOB on dst — it relies on the chip's internal page-move command (page-read into chip cache → program-execute on dst) to carry the **entire** spare area along with the data.

   Per proposal §7.0 invariant *"Chip-internal page-copy preserves OOB byte-for-byte"*, this is treated as **load-bearing for all supported parts**. Configurable layouts ride along the same-plane fast path with **no extra OOB programming**. Step 09 therefore:

   - Preserves today's behavior on this branch byte-for-byte under both `n` and `y`.
   - Adds an in-code comment citing the §7.0 invariant so a future reader does not "fix" it by adding a redundant OOB program.
   - Optionally adds a build-time assertion or comment near the branch documenting the assumption.

2. **Cross-plane branch (`src_column_addr != dst_column_addr`):** Today's code does CPU-side `spi_nand_read(main)` → `spi_nand_program_load(main)` → `spi_nand_program_load(markers `{0xFF,0xFF,0x00,0x00}` at `dst_column_addr + page_size`)` → `program_execute_and_wait`. With **`y`**, route the marker `program_load` through the layout scatter path identical to step 07 (BBM `0xFF,0xFF` + PAGE_USED `0x00,0x00`).

3. **`n`:** Both branches behave bit-identical to pre-change tree.

## Single-program-execute invariant (mirror of step 07)

The cross-plane branch must produce **exactly one** `program_execute_and_wait` per `nand_copy` invocation (same as today: two `program_load`s — main + markers — then one execute). The scatter path **must not** add a second execute for OOB.

## SPI-buffer-discipline rule (mirror of step 06/07/08)

- Cross-plane branch already uses `heap_caps_malloc(... MALLOC_CAP_DMA | MALLOC_CAP_8BIT)` for `copy_buf` (`nand_impl.c:487`) — keep that.
- Marker scatter target must also be DMA-capable. `handle->temp_buffer`'s first 4 bytes are fine (it is allocated with DMA caps in `nand_init_device`, `nand_impl.c:117`). **No** stack array as the SPI transfer buffer.

## Non-goals

- Redesigning copy to a single atomic multi-plane command — out of scope.
- Forcing OOB reprogram on the same-plane fast path — explicitly **forbidden** by §7.0 invariant.

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl.c`](../../../src/nand_impl.c) | `nand_copy` |

## Implementation checklist

1. **Same-plane branch:** No code change to OOB behavior. **Add a comment** referencing the proposal §7.0 invariant and `RFC_SPI_NAND_OOB_LAYOUT.md` so the assumption is discoverable from the code site. Suggested wording:
   ```c
   /* Same-plane (fast) path: chip-internal page-move preserves the entire
    * spare area byte-for-byte (proposal §7.0 invariant). Do NOT add an
    * OOB reprogram here — it would double-write OOB and change wire-level
    * behavior for default and configurable layouts alike. */
   ```
2. **Cross-plane branch:** Reuse the marker-composition helper from step 07 (BBM `0xFF,0xFF` + PAGE_USED `0x00,0x00`) into `handle->temp_buffer` first 4 bytes; replace the legacy hardcoded `markers[4]` literal with the layout-driven build. SPI sequence stays the same: read main → program_load main → program_load markers → program_execute_and_wait.
3. **`CONFIG_NAND_FLASH_VERIFY_WRITE` parity (load-bearing):** the existing `s_verify_write` calls for both the main page and the markers (`nand_impl.c:540`, and any same-plane verify path) must remain. The `y` path must not skip or shorten verify; the bytes being verified are the layout-composed 4 bytes, identical to legacy for default layout. If verify is currently absent on the same-plane branch (no marker reprogram → no marker verify), keep it absent — do **not** add new verify traffic.
4. **`n`:** Branches bit-identical to pre-change tree.

## Testing

- Stress copy tests if present; add host/target case if a gap is identified — may be folded into step 11.

## Acceptance criteria

- [ ] Cross-plane branch: marker bytes on dst match legacy for default layout (`n` vs `y`), via layout scatter (BBM + PAGE_USED), not a hardcoded literal.
- [ ] Cross-plane branch: exactly **one** `spi_nand_program_execute_and_wait` per `nand_copy` (same as today).
- [ ] Same-plane branch: **no** OOB reprogram added under `y` (review checklist; proposal §7.0 invariant cited in code comment).
- [ ] `CONFIG_NAND_FLASH_VERIFY_WRITE=y`: verify calls preserved symmetrically vs legacy on both branches.
- [ ] Marker buffer for `program_load` is DMA-capable (`handle->temp_buffer`), not stack.

## Risks

- Hidden reliance on hardware copy leaving OOB unchanged (the same-plane invariant) — if a future part violates this, that vendor module must opt out of the fast path; this step explicitly does **not** try to be defensive about that case.
- Doubling programs by accident — extra `program_execute` on the same-plane branch would change wire behavior on every page copy.

## Notes for implementers

- If step exceeds **700 LOC**, split: **09a** comment + cross-plane scatter only; **09b** any small refactor. Same-plane branch is comment-only so it should stay small.
