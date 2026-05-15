# Tier 1 — Foundation and database path

**PR identifier:** `anonymous-chip-tier1`  
**Depends on:** none  
**Estimate:** ~120–400 LOC (Kconfig/CMake + minimal `nand_init_device` / private state)

## Goal

Establish **build gates** and **explicit Tier 1 semantics** per proposal §5.1, §6, §8:

1. **Kconfig + CMake:** Master `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` and dependent `CONFIG_NAND_FLASH_ANONYMOUS_MANUAL` (default **`n`** / **`n`**). With defaults unchanged, **zero** change to chip detection, init return codes, or SPI traffic.
2. **Tier 1 path:** When anonymous master is **off**, control flow matches **pre-change** `nand_init_device`. When master is **on**, run the **existing** database/vendor path first; on **Tier 1 success**, set internal **`chip_source = DATABASE`** and clear internal anonymous flag (proposal §7). **No** ONFI parameter page traffic on Tier 1 success (proposal §5.1 invariant).
3. **After Tier 1 failure** with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`:** return **`ESP_ERR_NOT_FOUND`** (no Tier 2 attempt yet — **Tier 2** milestone adds ONFI). With master **`n`**, preserve **baseline** `esp_err_t` from Tier 1 failure (proposal §5.1).

## Scope

| Area | Action |
|------|--------|
| [`../../../Kconfig`](../../../Kconfig) | `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` (master), `CONFIG_NAND_FLASH_ANONYMOUS_MANUAL` (`depends on` master); Linux: anonymous SPI probe **off** for `IDF_TARGET_LINUX` per §8 (`depends on !IDF_TARGET_LINUX` or equivalent). |
| [`../../../CMakeLists.txt`](../../../CMakeLists.txt) | Conditional `SRCS` for anonymous-only objects added in **Tier 2**; no dangling references when master is `n`. |
| [`../../../src/nand_impl.c`](../../../src/nand_impl.c) | Tier ordering scaffold: Tier 1 as today; `chip_source = DATABASE` on success; anonymous off ⇒ unchanged flow; anonymous on + Tier 1 fail ⇒ `ESP_ERR_NOT_FOUND` until Tier 2 lands. |
| [`../../../src/nand_flash_blockdev.c`](../../../src/nand_flash_blockdev.c) | **No change** if it only calls `nand_init_device` — verify BDL inherits behavior (baseline §5.4). |
| `priv_include/nand.h` (or device struct) | Default **`chip_source`** = DATABASE at handle creation; storage for provenance enum (private header). |

## Out of scope

- ONFI parameter page read, CRC helpers, `nand_onfi_*` modules (**Tier 2**).
- **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`** geometry prompts beyond what **Tier 1** already added for the manual symbol (full Tier 3 Kconfig fields land in **Tier 3**).
- Public **`spi_nand_get_chip_source`** / `include/` enum (**Tier 3**).
- **`sdkconfig.ci.*`** / pytest / host CRC tests (**Tests** milestone).

## Background

Proposal §5.1 (Tier 1 invariant, final errors when anonymous on); §6.1 (two-level gates); §8 (Linux).

## Implementation checklist

1. **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT`:** `bool`, default **`n`**; help cites master gate for Tier 2 + shared plumbing; when `n`, behavior matches pre-feature baseline.
2. **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`:** default **`n`**; `depends on` master; help: Tier 3 only (proposal §5.3, §6.1).
3. **CMake:** Guard future anonymous `.c` files with `if(CONFIG_NAND_FLASH_ANONYMOUS_DETECT)` (or repo-equivalent). Prefer **no** new `.c` in this PR unless linking requires a stub — document choice.
4. **`nand_init_device`:** When **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`**, compile path matches pre-change control flow.
5. Tier 1 success: internal **`chip_source = DATABASE`**, clear anonymous flag.
6. Build smoke: `test_app` (e.g. esp32) + `host_test` linux preview (baseline §4.7).

## Acceptance criteria

- [ ] With both Kconfig options **`n`**: no new SPI sequences; init **`esp_err_t`** semantics vs current `master` unchanged for same hardware/config (proposal §5.1).
- [ ] Manual option invisible or forced off when master is `n`.
- [ ] Linux default host build succeeds; anonymous master unavailable/off per §8.
- [ ] Tier 1 success (anonymous **`y`**, known part): **no** ONFI traffic (proposal §5.1).
- [ ] Anonymous **`y`** + Tier 1 fail ⇒ **`ESP_ERR_NOT_FOUND`** until Tier 2 merges (document transitional behavior in PR if needed).
- [ ] Anonymous **`n`** + Tier 1 fail ⇒ baseline errors (e.g. unknown ID path).

## Verification

```bash
cd test_app && idf.py set-target esp32 && idf.py build
cd ../host_test && idf.py --preview set-target linux && idf.py build
```

## Risks / notes

- **CMake:** Flipping master to **`y`** must build — either omit Tier 2 sources until **Tier 2** or add stubs; do not reference missing object files.
- **Mis-scoped `depends on`:** Verify against `sdkconfig.defaults` in test apps.

## References

- [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) §5.1, §6.1, §8.
- [`../../baseline.md`](../../baseline.md) §5.3–§5.4.
