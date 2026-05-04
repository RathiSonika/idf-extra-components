# Step 01 — Kconfig and CMake build guard

**PR identifier:** `oob-layout-01`  
**Depends on:** none  
**Estimate:** small (~50–120 LOC)

## Goal

Introduce a single **experimental** Kconfig symbol that gates **all** configurable OOB layout code paths added in later steps. **Zero** change to runtime NAND behavior: default **`n`**, no new sources compiled beyond stubs if needed.

## Non-goals

- No changes to `nand_impl.c`, `nand_impl_linux.c`, Dhara, or public headers.
- No new tests required beyond “still builds” (optional: CI already passes).

## Background

Parent proposal §2.1 requires Kconfig-gated rollout. This step establishes the **switch** so subsequent PRs can use `#ifdef CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` / `if (CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT)` without touching behavior until step 06+.

## Files to touch

| File | Action |
|------|--------|
| [`Kconfig`](../../../Kconfig) (component root) | Add `config NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` under existing menu |
| [`CMakeLists.txt`](../../../CMakeLists.txt) | Optionally prepare `if(CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT)` source list (may be empty until step 03); **or** defer CMake changes until first `.c` file lands — see checklist |

## Implementation checklist

1. Add Kconfig entry:
   - **Type:** `bool`
   - **Default:** `n`
   - **Prompt:** e.g. “Experimental: configurable OOB layout”
   - **Help:** State clearly: experimental; when disabled, behavior matches legacy fixed OOB markers (proposal §1.2); when enabled, subsequent releases wire layout — **currently no functional change** after this PR alone.
2. Ensure symbol is visible on **MCU targets** and **`linux`** target (no `depends on` that accidentally disables Linux host tests later).
3. CMake:
   - **Minimal approach (recommended for step 01):** only Kconfig in this PR; CMake unchanged.
   - **Alternative:** add commented placeholder `list(APPEND SRCS ...)` guarded by config for clarity — acceptable if it stays zero compiled objects until step 03.
4. Run `idf.py set-target esp32 && idf.py build` from **`test_app`** (or component example project) with default sdkconfig — must succeed.
5. Run `idf.py --preview set-target linux && idf.py build` from **`host_test`** — must succeed.

## Acceptance criteria

- [ ] With **`CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n`** (default): tree builds unchanged from NAND behavior perspective (no new code paths executed).
- [ ] Flipping to **`y`** in `menuconfig` still builds with **no additional `.c` files** linked unless you deliberately added empty stubs (prefer no stubs in step 01).
- [ ] Kconfig help documents experimental status and points maintainers to [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) or this folder.

## Risks

- None material if CMake stays untouched.

## Notes for implementers

- Final name of the option may be shortened but keep **`NAND_FLASH_`** prefix consistent with `NAND_FLASH_ENABLE_BDL`.
- Do **not** flip default to `y` until parent proposal rollout policy says so (later release; tracked in step 12).
- Locked project decisions (MAX regions = 8, layout selection, known-bug partition, **no** `config` layout override) live in the folder [`README.md`](README.md) — read when implementing any later step.
