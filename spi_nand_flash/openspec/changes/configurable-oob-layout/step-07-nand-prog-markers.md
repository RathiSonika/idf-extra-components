# Step 07 — `nand_prog` marker program via layout path

**PR identifier:** `oob-layout-07`  
**Depends on:** steps **01–06** (06 may parallelize if careful — prefer sequential)  
**Estimate:** ~200–450 LOC

## Goal

In [`src/nand_impl.c`](../../../src/nand_impl.c) **`nand_prog`**:

- **`n`:** Keep legacy marker program `{0xFF, 0xFF, 0x00, 0x00}` at `column_addr + page_size`.
- **`y`:** Compose the same 4-byte pattern using xfer **scatter** built from layout descriptors:
  - BBM bytes 0–1: `0xFF, 0xFF` (good-block pattern from `layout->bbm.good_pattern`).
  - PAGE_USED bytes (logical offset **0**, len **2** → physical OOB bytes 2–3): `0x00, 0x00` (used).
  - **Source the bytes from layout/field constants**, not from a hardcoded `{0xFF,0xFF,0x00,0x00}` literal — that's the whole point of going through the layout path.

Sequence is unchanged from today: `program_load(data) → program_load(markers) → program_execute_and_wait`. Two `program_load`s, **one** `program_execute`.

## Single-program-execute invariant (proposal §2.2 / §7.0 — load-bearing)

`nand_prog` must produce **exactly one** `program_execute_and_wait` per logical page program. The two existing `program_load` calls (data + markers) commit atomically inside that single execute — power loss between the two loads is harmless. The scatter path **must not** introduce any additional `program_execute` for OOB regions on the same page.

If a future layout's regions ever required multiple `program_execute`s on the same page, that is a new partial-program failure mode and is out of scope for this proposal — that vendor module would need to opt in via a separate change.

## SPI-buffer-discipline rule (mirror of step 06)

The 4-byte marker buffer handed to `spi_nand_program_load` must be **DMA-capable / aligned** — use `handle->temp_buffer` (or `handle->read_buffer` if not in flight) as the actual scatter target, not a stack `uint8_t[4]`. See step 06's "SPI-buffer-discipline rule" for rationale (baseline §6 DMA history).

## Non-goals

- Changing program command count (still one program load for main + one for markers as today, then one execute).
- Adding new `program_execute` calls.

## Files to touch

| File | Action |
|------|--------|
| [`src/nand_impl.c`](../../../src/nand_impl.c) | `nand_prog` branch |

## Implementation checklist

1. Preserve order: main page data load, then marker load, then **one** `program_execute_and_wait`.
2. **`y` path:**
   - Stack-local `spi_nand_oob_xfer_ctx_t ctx;` pointing at a DMA-capable scratch buffer (e.g. `handle->temp_buffer` first 4 bytes).
   - Init ctx with `SPI_NAND_OOB_CLASS_FREE_ECC`.
   - **BBM bytes 0–1**: write directly from `layout->bbm.good_pattern` into the scratch buffer (BBM is **not** in any free region per step 03; it sits at physical offset 0, length `bbm.bbm_length`).
   - **PAGE_USED**: scatter `0x00, 0x00` at logical offset **0**, len **2** (resolves to physical OOB bytes 2–3).
   - Issue `spi_nand_program_load(handle, scratch, column_addr + page_size, 4)`.
3. **`CONFIG_NAND_FLASH_VERIFY_WRITE` parity (load-bearing):** the verify-read at `column_addr + page_size` (4 bytes) must still happen and must use the same buffer/length pattern as today (`s_verify_write` at `nand_impl.c:376`). The `y` path must not skip or shorten verify; the bytes being verified are the layout-composed 4 bytes, identical to legacy for default layout.
4. Failure paths unchanged.

## Testing

- Legacy sdkconfig full suite.
- `y` local parity: compare wire bytes on logic analyzer optional — minimum is regression tests.

## Acceptance criteria

- [ ] Marker bytes on medium identical to legacy for default layout (`{0xFF, 0xFF, 0x00, 0x00}` at `column_addr + page_size`).
- [ ] Exactly **two** `spi_nand_program_load` calls + exactly **one** `spi_nand_program_execute_and_wait` per `nand_prog` invocation (same as today).
- [ ] `CONFIG_NAND_FLASH_VERIFY_WRITE=y` still re-reads markers exactly as today (same call to `s_verify_write` for the 4-byte marker region).
- [ ] Marker buffer handed to `spi_nand_program_load` is DMA-capable (`handle->temp_buffer` / `handle->read_buffer`), not stack.
- [ ] BBM bytes are sourced from `layout->bbm.good_pattern`, not a hardcoded literal.
- [ ] PAGE_USED bytes are written via `nand_oob_scatter` at logical offset **0** length **2** — physically lands at OOB bytes 2–3.

## Risks

- Scatter builds wrong pattern — add unit round-trip test (step 11).
- Forgetting to update the `s_verify_write` path symmetrically — silent verify bypass on `y` builds.
- Using a stack buffer triggers SPI driver memcpy fallback or, worse, alignment failure on some targets.

## Notes for implementers

- Field logical offsets recap (matches step 03/04/05): BBM at **physical** offset 0 length 2 (separate descriptor, not a "free" region); PAGE_USED at **logical** offset 0 length 2 inside the single free region (physical OOB offset 2 length 2). Final 4 bytes on flash: BBM[0..1] + PAGE_USED[0..1].
- Scatter order is independent when writing the full buffer, but the **buffer composition** must be deterministic so `s_verify_write` compares against the same bytes.
