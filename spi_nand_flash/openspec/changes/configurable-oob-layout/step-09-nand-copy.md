# Step 09 — `nand_copy`: fix double execute + layout / full-spare parity

**PR identifier:** `oob-layout-09`  
**Depends on:** steps **01–08**  
**Estimate:** ~300–650 LOC (bugfix + optional buffer growth for full spare)

## Goal

### A. Bugfix: duplicate `program_execute` (all Kconfig values — **required**)

[`src/nand_impl.c`](../../../src/nand_impl.c) **`nand_copy`** currently calls **`program_execute_and_wait(handle, dst, …)` twice** on the **cross-plane** path: once inside the `if (src_column_addr != dst_column_addr)` block and again unconditionally at the bottom of the function. That is **incorrect** for NAND (second execute without a valid reload sequence).

- **Restructure** so each branch runs **exactly one** `program_execute_and_wait` for `dst`:
  - **Cross-plane:** one execute after all `program_load` operations for that copy.
  - **Same-plane:** one execute only (unchanged count vs intended legacy behavior — today the same-plane path only hits the bottom execute; verify after refactor that same-plane still has **one** execute total).

### B. Same-plane fast path (proposal §7.0 — unchanged behavior)

When `src_column_addr == dst_column_addr`:

- Preserve **no** extra OOB `program_load` — chip-internal page program carries full spare (proposal invariant).
- Keep / add the **comment** from the previous step-09 text citing §7.0 so future edits do not “fix” OOB on this branch.

### C. Cross-plane + **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=y`** (**required** for acceptance when `y`)

- **Markers:** Compose the marker region via the same layout/scatter approach as step **07** (BBM good pattern + PAGE_USED used) into DMA-capable scratch (`handle->temp_buffer`), not a stack SPI buffer.
- **Full spare image (proposal §2.2 *Cross-plane vs same-plane equivalence*):** After `read_page_and_wait(src)`, read **`handle->chip.page_size`** bytes of main **and** **`oob_size`** bytes of spare from the chip cache (using cached `oob_size` from layout / step 05 — **not** only 4 marker bytes), then `program_load` main and `program_load` full spare at `dst_column_addr` / `dst_column_addr + page_size`, then **one** `program_execute_and_wait`. This makes cross-plane `dst` match what same-plane would have written for the same `src` for the programmable spare region.

  **Buffering:** Extend the cross-plane DMA buffer allocation from `page_size` to **`page_size + oob_size`** (cap `oob_size` using layout + sanity bound), or use `read_buffer`/`temp_buffer` slices documented in the PR — **no stack** for SPI.

### D. Cross-plane + **`n`** (legacy)

- After **(A)**, keep legacy **4-byte** marker program at spare base if full-spare copy is deferred for `n` — wire-level marker bytes must remain **byte-identical** to pre-fix tree for default geometry; only the **duplicate execute** is removed.

**`n`:** Both branches after (A) behave as today except the **removed erroneous second execute** on cross-plane.

## Single-program-execute invariant

Each `nand_copy` invocation must end with **at most one** successful `program_execute_and_wait` toward `dst` per program sequence (mirror step 07).

## SPI-buffer-discipline rule (mirror of step 06/07/08)

- Cross-plane main buffer: **`heap_caps_malloc(..., MALLOC_CAP_DMA | MALLOC_CAP_8BIT)`** — if grown for spare, keep DMA caps.
- Marker / spare composition: **`handle->temp_buffer`** or the same DMA heap buffer — **no** stack `uint8_t[]` handed to `spi_nand_program_load` / `spi_nand_read`.

## Non-goals

- Redesigning copy to a single atomic multi-plane vendor command — out of scope.
- **`nand_diag_api.c`** — no change (root [`README.md`](README.md)).

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl.c`](../../../src/nand_impl.c) | `nand_copy` refactor + `y` paths |

## Implementation checklist

0. **Map current fall-through** — Identify both `program_execute_and_wait` call sites on the cross-plane path; sketch control flow so the unconditional tail execute does **not** run after cross-plane already executed.
1. **Refactor structure (pseudocode):**

   ```text
   read_page_and_wait(src)
   if (cross-plane) {
       allocate DMA buffer >= page_size + (y ? oob_size : 0 or legacy marker-only)
       read main (+ spare per branch above)
       write_enable; program_load main; program_load markers or full spare; program_execute_and_wait(dst) ONCE
       free
   } else {
       write_enable; program_execute_and_wait(dst) ONCE   // cache already loaded from src
   }
   // verify-write block (#if CONFIG_NAND_FLASH_VERIFY_WRITE) updated so it does not assume an extra execute happened
   ```

2. **`y`:** Implement **(C)** full spare read/program using `nand_oob_*` helpers where applicable for the marker **subset**; raw `spi_nand_read`/`program_load` for bulk main+spare is acceptable if scatter is marker-only.
3. **`CONFIG_NAND_FLASH_VERIFY_WRITE`:** Adjust branches so verification still matches the new execute count (re-read `dst` after the **single** execute).
4. **Regression:** Logic analyzer or test assert: **one** program execute per `nand_copy` on cross-plane golden case.

## Testing

- Host/target copy tests; add a **unit-style** assertion in host test if available: cross-plane path invokes **one** execute (mock/spy if infrastructure exists — optional).
- Full suites in step **11**.

## Acceptance criteria

- [ ] **Bugfix:** Cross-plane path never calls `program_execute_and_wait` **twice** for the same logical copy of `dst`.
- [ ] Same-plane path: still **one** execute; **no** OOB `program_load`; §7.0 comment present.
- [ ] **`y` + default layout:** Cross-plane `dst` spare matches **src** for full **`oob_size`** (proposal equivalence), not only 4 marker bytes — **or** document in PR why a follow-up PR completes full spare if step size must split.
- [ ] **`n`:** Marker bytes on `dst` match **legacy** golden behavior; only duplicate-execute bug is fixed.
- [ ] DMA / alignment rules respected (steps 06–08 mirror).
- [ ] `CONFIG_NAND_FLASH_VERIFY_WRITE=y` paths updated and still run where applicable.

## Risks

- **Buffer size** — `page_size + oob_size` heap use on cross-plane; watch fragmentation; prefer single alloc per copy, free before return.
- **Verify-write** assumptions after control-flow change — easy to miss a branch.

## Notes for implementers

- If this step risks **>700 LOC**, split **09a** = duplicate-execute fix + `n` parity only; **09b** = `y` full-spare cross-plane + tests.
