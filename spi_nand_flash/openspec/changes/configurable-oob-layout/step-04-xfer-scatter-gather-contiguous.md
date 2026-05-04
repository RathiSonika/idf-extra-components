# Step 04 — Xfer context init + scatter/gather (contiguous fast path)

**PR identifier:** `oob-layout-04`  
**Depends on:** steps **01–03**  
**Estimate:** ~250–550 LOC

## Goal

Implement **init-time** caching for xfer context:

- `nand_oob_xfer_ctx_init(...)` — scans layout `free_region` callbacks once for a given `spi_nand_oob_class_t`, fills `regs[]`, computes `total_logical_len`.
- `nand_oob_scatter(...)` — copy **from** logical field buffer **into** `oob_raw[]` at mapped physical offsets (write path).
- `nand_oob_gather(...)` — copy **from** `oob_raw[]` **into** logical buffer (read path).

For **default layout** (single region length **2**, see step 03), scatter/gather must be **O(1)** memcpy slices — **no** loops over fragments beyond what **`SPI_NAND_OOB_MAX_REGIONS` (8)** allows (see step 02 / root [`README.md`](README.md)).

**Single-program-execute invariant (proposal §2.2 / §7.0):** All `program_load` calls produced by scatter on a single logical page program **must** precede exactly one `program_execute_and_wait`. The scatter API itself does not issue NAND I/O; the call site (`nand_prog`, step 07) is responsible for batching all loads before the single execute. Document this contract in the public-to-internal header for scatter so step 07 / 08 / 09 implementers cannot accidentally fan out.

## Non-goals

- Interleaved multi-region composition beyond iterating `regs[]` in order — full QA for weird geometries deferred.
- No NAND I/O in this module.

## API sketch (private header)

Add declarations to `priv_include/nand_oob_xfer.h` (new) or extend types header:

```text
esp_err_t nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                                 const spi_nand_oob_layout_t *layout,
                                 const void *chip_ctx,
                                 spi_nand_oob_class_t cls,
                                 uint8_t *oob_raw,
                                 uint16_t oob_size);
esp_err_t nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                          size_t logical_off,
                          void *dst,
                          size_t len);
esp_err_t nand_oob_scatter(spi_nand_oob_xfer_ctx_t *ctx,
                           size_t logical_off,
                           const void *src,
                           size_t len);
```

Use **`esp_err_t`**; validate bounds against `total_logical_len` and each region’s bounds.

## Files to touch

| File | Action |
|------|--------|
| `priv_include/nand_oob_xfer.h` | **Create** declarations |
| `src/nand_oob_xfer.c` | **Create** implementation |
| [`CMakeLists.txt`](../../../CMakeLists.txt) | Link when Kconfig on |

## Implementation checklist

1. `nand_oob_xfer_ctx_init`:
   - Clear `ctx`.
   - Loop `section = 0..` calling `layout->ops->free_region(chip_ctx, section, &desc)` until `ESP_ERR_NOT_FOUND`.
   - Filter regions matching requested `cls` (ECC vs NOECC) — default layout may use single region tagged ECC per step 03.
   - Copy filtered regions into `ctx->regs[]`, bump `reg_count`, sum lengths into `total_logical_len`.
   - Store `layout`, `cls`, `oob_raw`, `oob_size`.
2. **Scatter:** Map contiguous logical `[logical_off, logical_off+len)` into possibly multiple physical spans inside `oob_raw` — for one region, delegate to `memcpy` into `oob_raw[region.offset + local_off]`.
3. **Gather:** Inverse.
4. Return `ESP_ERR_INVALID_ARG` / `ESP_ERR_INVALID_SIZE` on out-of-bounds.
5. Add **internal** self-check build-only test or assert: default layout `total_logical_len == 2` (single free region {offset=2, length=2} per step 03).

## Testing

- Host-only tiny test harness optional (step 11); minimum is compile + manual reasoning.

## Acceptance criteria

- [ ] Default layout: scatter then gather round-trip preserves bytes for `logical_off=0, len=2` (the only valid range).
- [ ] PAGE_USED field at logical offset **0** length **2** inside the single free region (base **2**) maps to physical OOB bytes **2–3** — same on-flash bytes as today's hardcoded layout.
- [ ] `total_logical_len == 2` for default layout (BBM bytes 0–1 are not in any free region, per step 03).
- [ ] Out-of-bounds (e.g. `logical_off + len > total_logical_len`) returns `ESP_ERR_INVALID_SIZE` — does NOT spill into BBM bytes.
- [ ] No heap allocation in init/scatter/gather.
- [ ] Header contract documents the single-`program_execute`-per-page invariant for callers.

## Risks

- Off-by-one between **logical** offsets (field layer) and **physical** OOB indices — document clearly in header. The default layout deliberately keeps `region.offset = 2` so logical 0 ⇄ physical 2; an implementer who hard-codes `region.offset = 0` would silently overwrite BBM on every page program.
- Allowing scatter past `total_logical_len` would let the field layer write into BBM bytes — bounds check is load-bearing, not just a nice-to-have.

## Notes for implementers

- `chip_ctx` type is **opaque** (`const void *`) — passed through from device handle in later steps; for step 04 tests pass **NULL** if ops ignore chip for default layout.
- Per-call xfer ctx state is **stack-local** (proposal §2.2 / step 05). Do not stash `oob_raw` or in-flight bytes on the device handle — that would race against unsynchronized raw Flash BDL paths (baseline §9.1).
