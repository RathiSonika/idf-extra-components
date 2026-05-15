# Handoff: OpenSpec **implementation milestones** for anonymous chip detection (`spi_nand_flash`)

Copy this file (or the sections below) into a new agent/session to author or refine **ordered, PR-sized milestone documents** under this folder.

---

## Goal

Maintain **ordered milestone documents** under `spi_nand_flash/openspec/changes/anonymous-chip-detect/` (same tone as `openspec/changes/configurable-oob-layout/`: [`README.md`](README.md) milestone table + `step-tier*.md`, `step-tests-*.md`, `step-docs-*.md`).

Do **not** implement C code in this task unless explicitly asked; deliver **milestone specs** another implementer can execute without guessing.

---

## Source documents (read in this order)

1. **Feature spec (normative for *what* to build):** [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md)
2. **Product intent / tiers:** [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) (in-tree RFC mirror; note: RFC OOB snippets may still describe future configurable-OOB behavior — **v1 follows the proposal**, not conflicting RFC OOB sections.)
3. **Change folder index:** [`README.md`](README.md) — locked decisions + milestone index + dependency rule.
4. **Baseline / API stability:** [`../../baseline.md`](../../baseline.md) (especially init path, `include/` stability, Linux target, concurrency).
5. **Milestone style reference:** [`../configurable-oob-layout/README.md`](../configurable-oob-layout/README.md) + one or two `step-*.md` examples for tone (acceptance criteria, verification commands, “do not merge if…”).

---

## Reference implementation (logic only, **non-mergeable**)

- Git branch: **`feat/nand_generic_fallback`** (under `idf-extra-components` monorepo layout: paths like `spi_nand_flash/src/…`). **Port ideas, not the branch as-is.** The proposal intentionally diverges from POC (QE, public types, naming, spare handling, etc.).

---

## Locked decisions the milestones **must** respect (summary)

- **Tiers:** Tier 1 fail (unknown mfr **or** known mfr + vendor init fail) → Tier 2 ONFI → Tier 3 manual if enabled; master Kconfig off ⇒ baseline only.
- **ONFI:** Read parameter page; bytes **0–3 == `ONFI`**; CRC validate; **no** `READ_ID @ 0x20` requirement.
- **Multi-LUN:** ONFI tier fails if **`num_luns != 1`** (v1).
- **`spare_bytes_per_page`:** **Ignore for v1 init/geometry** (target `nand_flash_geometry_t` has no `oob_size`; markers = **4 bytes** at `page_size` per baseline `nand_impl.c`).
- **Quad:** Tier 2/3 success ⇒ **SIO only**, no QE; warn if config asked QOUT/QIO.
- **Flags/API:** **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** internal only; public **`spi_nand_get_chip_source`**; **`nand_parameter_page_t`** stays **`priv_include/`**.
- **Kconfig (two-level):** `CONFIG_NAND_FLASH_ANONYMOUS_DETECT` (master, default `n`), `CONFIG_NAND_FLASH_ANONYMOUS_MANUAL` (depends on master, default `n`); manual needs user-set geometry, no “works on random NAND” via defaults.
- **Final failure (anonymous on):** **`ESP_ERR_NOT_FOUND`** after all tiers exhausted; with anonymous off, baseline error codes unchanged.
- **Buffering:** proposal §**5.7** — bounded **heap**, DMA where required, **free all paths**.
- **Testing (merge bar §10):** **A** CRC host tests, **B** `sdkconfig.ci.anonymous` + pytest matrix, **C** hardware validation + documented evidence (CI optional per policy), **D** Linux host_test asserts anonymous does not change emulator path.

---

## Deliverables

1. **[`README.md`](README.md)** — locked decision table, **Tier 1 → Tier 2 → Tier 3 → Tests → Docs** index, dependency rule, conventions (branch naming, ~500–700 LOC per PR where possible; **Tier 2** may split into two PRs per milestone doc).

2. **One milestone file per row** — each must include:
   - **Scope** (files/modules).
   - **Out of scope** (explicit).
   - **Acceptance criteria** (checklist).
   - **Verification** (`idf.py` build targets, pytest names, host vs target).
   - **Risks / notes** (hardware, WDT, SPI, etc. if relevant).

---

## Milestone split (normative layout)

| Milestone | Typical contents |
|-----------|------------------|
| **Tier 1** | Kconfig + CMake; internal `chip_source`; `nand_init_device` Tier 1 only; transitional `ESP_ERR_NOT_FOUND` after Tier 1 fail when anonymous on until Tier 2 exists |
| **Tier 2** | `priv_include` ONFI types + CRC; SPI param page read; parse/init; `nand_init_device` ONFI branch + logging |
| **Tier 3** | Manual Kconfig + init; `spi_nand_chip_source_t` + `spi_nand_get_chip_source` |
| **Tests** | `test_app` CI preset + pytest + §10.C procedure; `host_test` CRC + Linux §10.D |
| **Docs** | Component README / CHANGELOG |

Adjust file names or split **Tier 2** across two PRs only if review load requires it; keep **one** normative **Tier 2** milestone doc as the contract.

---

## Output quality bar

Another agent should implement **only** from these milestone files + the proposal + RFC, **without** re-deriving policy. Any ambiguity left in milestones should be resolved by **citing** `anonymous_chip_detect_proposal.md` section numbers.

Use **[`IMPLEMENTATION_PROMPT.md`](IMPLEMENTATION_PROMPT.md)** as the copy-paste handoff for **coding** these milestones (not this file).
