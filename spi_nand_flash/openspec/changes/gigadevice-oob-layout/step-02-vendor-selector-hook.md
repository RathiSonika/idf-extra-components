# Step 02 — Wire the per-vendor selector hook

**PR identifier:** `gd-oob-02`
**Depends on:** step **01** merged (layout table available); all `configurable-oob-layout/` steps 01–12 merged.
**Estimate:** ~120–250 LOC.

## Goal

Add the **per-vendor `get_oob_layout` hook** described in parent proposal §2.2 and use it to select the GD5F1GQ4xA layout for DIs `0x31, 0x32, 0x25, 0x35`. Every other DI continues to fall back to `nand_oob_layout_get_default()` — so existing chips keep their byte-for-byte default behavior.

## Datasheet verification gate (mandatory before merging)

This step **cannot be merged** until each of the four DIs is confirmed against the chip datasheet as a **GD5F1GQ4xA-family** part:
- 1Gb / 2Gb / 4Gb single-plane density
- 2 KiB main + 64 B spare per page
- Internal ECC always enabled (no user-toggleable bypass)
- Spare layout matches `gd5fxgq4xa_ooblayout` in Linux MTD

If any DI **cannot** be verified to that exact spec, **remove it from the resolver** in this PR and document the deferral in [`step-04-docs-and-followups.md`](step-04-docs-and-followups.md). Do **not** merge a "best guess" mapping. A wrong mapping silently corrupts the `ecc_region` enumeration even though the driver's hot path stays safe (parent proposal §5 first risk).

## Non-goals

- No tests in this PR. Tests land in step 03 — they reference the resolver added here.
- No layout for variant2 / 128 B-spare GD parts. Out of scope (parent proposal §7.1 Q3).
- No promotion of the hook to other vendors. Out of scope.
- No removal of the existing default fallback path — it stays intact.

## Files to touch

| File | Action |
|------|--------|
| [`priv_include/nand.h`](../../../priv_include/nand.h) | Add **one** new field on `spi_nand_flash_device_t`: a function pointer for the vendor OOB-layout resolver. Guarded the same way the rest of the OOB-related handle fields are (i.e. only when the framework is compiled in). |
| [`priv_include/nand_oob_layout_gigadevice.h`](../../../priv_include/nand_oob_layout_gigadevice.h) | Add a second declaration: the resolver function `spi_nand_gigadevice_get_oob_layout(const spi_nand_flash_device_t *dev)`. |
| [`src/nand_oob_layout_gigadevice.c`](../../../src/nand_oob_layout_gigadevice.c) | Implement the resolver as a small `switch` on `dev->device_info.device_id`, returning the Q4xA layout for the four allowed DIs and `NULL` otherwise. **Asserts** `dev != NULL`. |
| [`src/devices/nand_gigadevice.c`](../../../src/devices/nand_gigadevice.c) | At the **end** of `spi_nand_gigadevice_init`, set `dev->vendor_get_oob_layout = spi_nand_gigadevice_get_oob_layout`. **Do not** change any existing geometry / timing / flag setup. |
| [`src/nand_oob_device.c`](../../../src/nand_oob_device.c) | In `nand_oob_device_layout_init`, **before** assigning `handle->oob_layout = nand_oob_layout_get_default()`, consult `handle->vendor_get_oob_layout` if it is non-NULL. Cache the resulting pointer the same way the default is cached today. |

No other files change in this PR.

## Implementation notes

### Field name on the handle

Pick the name to match the convention already used by other vendor-set callbacks/fields in `priv_include/nand.h`. If unsure, use `vendor_get_oob_layout` and call it out in the PR description so a reviewer can request a rename — that's a one-line follow-up. **Do not** invent a new naming convention.

If the existing handle already groups vendor-set behavior into a substruct, place the field there. Otherwise it sits alongside the other OOB-related fields added by `configurable-oob-layout/step-05-device-state-and-init-hook.md`.

### Resolver shape

The resolver must be **side-effect-free**: pure function from `dev->device_info` to a layout pointer. No allocations, no logging, no register reads. The vendor init has already populated `device_info` and applied the chip's ECC configuration before this is called — this resolver is a lookup, nothing more.

```c
const spi_nand_oob_layout_t *spi_nand_gigadevice_get_oob_layout(
        const spi_nand_flash_device_t *dev)
{
    assert(dev != NULL);
    if (dev->device_info.manufacturer_id != SPI_NAND_FLASH_GIGADEVICE_MI) {
        return NULL;
    }
    switch (dev->device_info.device_id) {
    case GIGADEVICE_DI_31:
    case GIGADEVICE_DI_32:
    case GIGADEVICE_DI_25:
    case GIGADEVICE_DI_35:
        return nand_oob_layout_get_gigadevice_q4xa();
    default:
        return NULL;
    }
}
```

The `manufacturer_id` check is defensive — `spi_nand_gigadevice_init` is the only path that wires this resolver in this PR, so MI should already be `0xC8`. The check is cheap insurance against a future refactor that calls vendor resolvers from a generic site.

### Selector wiring in `nand_oob_device_layout_init`

The existing code (after framework step 05) ends roughly like:

```c
handle->oob_layout = nand_oob_layout_get_default();
```

This step **inserts** the vendor consultation *before* that line:

```c
const spi_nand_oob_layout_t *layout = NULL;
if (handle->vendor_get_oob_layout != NULL) {
    layout = handle->vendor_get_oob_layout(handle);
}
if (layout == NULL) {
    layout = nand_oob_layout_get_default();
}
handle->oob_layout = layout;
```

The rest of `nand_oob_device_layout_init` (region caching into `oob_cached_regs_free_ecc[]` etc., field assignment for `SPI_NAND_OOB_FIELD_PAGE_USED`) is **unchanged**. The `assert` block guarded by `NDEBUG` that pins the default-layout invariants must continue to hold when the default is selected — verify by running the existing host_test once the change lands.

### Linux mmap parity

Nothing to do. The Q4xA layout has `oob_bytes = 0` (defer to chip mapping), and `nand_impl_linux.c` already maps 2 KiB page → 64 B emulated_page_oob. The Q4xA layout fits in 64 B; no stride change.

## Acceptance criteria

- [ ] Datasheet verification documented in the PR description for each of `0x31, 0x32, 0x25, 0x35`. Any DI that cannot be verified is **removed** from the switch and called out in the PR description (and queued for [`step-04-docs-and-followups.md`](step-04-docs-and-followups.md)).
- [ ] `priv_include/nand.h` declares the new `vendor_get_oob_layout` field and the field is initialized to `NULL` somewhere in the device init path (zero-init via calloc or explicit assignment in `nand_init_device`).
- [ ] `spi_nand_gigadevice_init` sets `dev->vendor_get_oob_layout = spi_nand_gigadevice_get_oob_layout` only after the existing `device_id` switch has assigned `num_blocks` and other geometry — i.e. the resolver is set last.
- [ ] `nand_oob_device_layout_init` consults `vendor_get_oob_layout` (when non-NULL) before falling back to `nand_oob_layout_get_default()`. The fallback path is **unchanged** for any non-GigaDevice chip and for any GigaDevice DI not in the four-DI set.
- [ ] When the resolver returns `NULL` (any non-Q4xA DI), the existing `NDEBUG` invariant block in `nand_oob_device_layout_init` (default-layout asserts) still holds — verify with `idf.py build` (host_test) and a quick run.
- [ ] `idf.py build` passes for `test_app/sdkconfig.ci.oob_layout` and `test_app/sdkconfig.ci.bdl_oob_layout` (no behavior change for non-GD chips).
- [ ] `idf.py build --target linux` passes for `host_test` with the existing preset (no new test files in this step).
- [ ] When `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n`, the new field on the handle and the new resolver are compiled out — `git grep` shows no symbol leakage.

## Risks

- **Wrong DI mapping** (parent proposal §5 first risk). Mitigated by the verification gate above.
- **Initialization order.** If `vendor_get_oob_layout` is consulted before `device_info` is populated, the resolver returns `NULL` and we fall back to default — silent, but wrong-by-omission. Mitigation: assert in step 03 host_test V3 that the resolver matches the four DIs when called with a populated `device_info`.
- **Concurrency.** The hook is read once during init under the existing init-context locking discipline. It is **not** touched on hot paths. No new lock or atomic is introduced.
- **`NULL` return signaling default.** Documented contract in the resolver's header comment; tested by V3 in step 03.

## Notes for implementers

- Do **not** rename or move `nand_oob_layout_get_default()` in this PR. The selector simply consults the vendor hook, then falls through.
- Do **not** combine this with step 03 tests — keep the resolver wiring reviewable on its own. Tests are short and arrive cleanly in the next PR.
- If the PR diff approaches 350+ LOC, re-check whether you've accidentally touched `nand_impl.c` / `dhara_glue.c`. They must not change.
