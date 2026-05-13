# Handoff: OpenSpec **implementation steps** for anonymous chip detection (`spi_nand_flash`)

Copy this file (or the sections below) into a new agent/session to author **ordered, PR-sized step documents** under this folder.

---

## Goal

Author **ordered, PR-sized step documents** under `spi_nand_flash/openspec/changes/anonymous-chip-detect/` (same style as `openspec/changes/configurable-oob-layout/`: `README.md` step table + `step-01-….md`, `step-02-….md`, …).

Do **not** implement C code in this task unless explicitly asked; deliver **step specs** another implementer can execute without guessing.

---

## Source documents (read in this order)

1. **Feature spec (normative for *what* to build):** [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md)
2. **Product intent / tiers:** [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) (in-tree RFC mirror; note: RFC OOB snippets may still describe future configurable-OOB behavior — **v1 follows the proposal**, not conflicting RFC OOB sections.)
3. **Change folder stub:** [`README.md`](README.md) — extend with locked decisions + step index.
4. **Baseline / API stability:** [`../../baseline.md`](../../baseline.md) (especially init path, `include/` stability, Linux target, concurrency).
5. **Step style reference:** [`../configurable-oob-layout/README.md`](../configurable-oob-layout/README.md) + one or two `step-*.md` examples for tone (acceptance criteria, verification commands, “do not merge if…”).

---

## Reference implementation (logic only, **non-mergeable**)

- Git branch: **`feat/nand_generic_fallback`** (under `idf-extra-components` monorepo layout: paths like `spi_nand_flash/src/…`). **Port ideas, not the branch as-is.** The proposal intentionally diverges from POC (QE, public types, naming, spare handling, etc.).

---

## Locked decisions the steps **must** respect (summary)

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

1. Update **[`README.md`](README.md)** with:
   - Locked decision table (can mirror proposal §5–§7, §10 briefly).
   - **Ordered step list** with one-line description each.
   - **Dependency rule:** e.g. do not merge step N+1 if step N acceptance fails.
   - **Conventions:** branch naming, max diff size target (~500–700 LOC meaningful diff per PR where possible), touch only `spi_nand_flash/` + `test_app/` + `host_test/` as appropriate; **do not** modify vendored Dhara unless a step explicitly requires it (prefer avoid).

2. Create **`step-01-….md` through `step-N-….md`** — each step must include:
   - **Scope** (files/modules).
   - **Out of scope** (explicit).
   - **Acceptance criteria** (checklist).
   - **Verification** (`idf.py` build targets, pytest names, host vs target).
   - **Risks / notes** (hardware, WDT, SPI, etc. if relevant).

---

## Suggested step split (adjust after reading tree; keep PR-sized)

Indicative sequence — **rename/split** as needed after inspecting current `master` vs POC:

1. Kconfig + CMake gates (default **off**; no runtime behavior change).
2. Private types: `nand_parameter_page_t`, CRC helper, constants — `priv_include/` only.
3. SPI layer: parameter page read primitive(s) + bounded heap (§5.7); unit-testable CRC (§10.A).
4. ONFI init module: parse fields per proposal; `num_luns` check; **no** spare→geometry; SIO-only (§5.6); internal anonymous flag + `chip_source`.
5. `nand_init_device` integration: tier ordering + `ESP_ERR_NOT_FOUND` + logging.
6. Manual tier: second Kconfig + validation + init path.
7. Public API: `spi_nand_chip_source_t`, `spi_nand_get_chip_source` in `include/spi_nand_flash.h` + `nand.c` implementation.
8. **test_app:** `sdkconfig.ci.anonymous`, pytest matrix, on-target cases as feasible (§10.B/C).
9. **host_test:** CRC tests + Linux “no anonymous SPI” assertion (§10.A/D).
10. Docs: README/CHANGELOG; pointer to RFC + proposal.

Merge **§10.C** (hardware evidence) into the step that first claims “feature complete” or add a dedicated **“hardware sign-off”** step if your process requires it.

---

## Output quality bar

Another agent should implement **only** from these step files + the proposal + RFC, **without** re-deriving policy. Any ambiguity left in steps should be resolved by **citing** `anonymous_chip_detect_proposal.md` section numbers.

Once `step-01-…` through `step-10-…` exist, use **[`IMPLEMENTATION_PROMPT.md`](IMPLEMENTATION_PROMPT.md)** as the copy-paste handoff for **coding** those steps (not this file).
