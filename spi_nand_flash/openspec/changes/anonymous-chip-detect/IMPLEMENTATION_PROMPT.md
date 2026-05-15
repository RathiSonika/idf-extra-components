# Handoff: **implement** anonymous chip detection (OpenSpec tier milestones)

Copy everything from **“Agent instructions”** below into a new agent/session (or attach this file). Implement **firmware and tests**, not more planning docs.

---

## Agent instructions

### Goal

Implement **anonymous chip detection** for the `spi_nand_flash` ESP-IDF component by executing milestones **in order**:

**Tier 1 → Tier 2 → Tier 3 → Tests → Docs**

under:

`spi_nand_flash/openspec/changes/anonymous-chip-detect/`

Each **`step-tier*.md`**, **`step-tests-*.md`**, or **`step-docs-*.md`** file is the **PR-sized contract** for that slice: scope, out-of-scope, acceptance checklist, verification commands, risks. **Do not skip milestones** and **do not merge the next milestone** if the previous milestone’s acceptance criteria fail (see [`README.md`](README.md) dependency rule).

### Read order (per milestone / PR)

1. **Current milestone file** — implement exactly what it lists; stay within **Out of scope**.
2. **Normative feature spec:** [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md) — cite section numbers when behavior is ambiguous; this wins over the RFC where they conflict.
3. **Locked decisions table:** [`README.md`](README.md) — non-negotiable summary; do not re-litigate tiers, errors, ONFI gate, multi-LUN, SIO-only Tier 2/3, Kconfig defaults, `priv_include` for parameter page types, §10 test bar.
4. **Current behavior / stability:** [`../../baseline.md`](../../baseline.md) — init path (`nand_init_device`), `include/` API stability, Linux emulator (§4.7), mutex / concurrency (§9.1), existing test layout (`test_app/`, `host_test/`).
5. **Product intent only:** [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md) — tiers and guardrails; **ignore RFC OOB layout code** for v1 (proposal §5.4; README “Ordering vs configurable OOB”).

### Reference code (non-mergeable)

Branch **`feat/nand_generic_fallback`** in `idf-extra-components`: use for **flow ideas** only. **Do not** port POC verbatim — proposal [**§12**](../../anonymous_chip_detect_proposal.md) lists required deltas (ONFI bytes 0–3, no public `nand_parameter_page_t`, no QE guess, SIO for Tier 2/3, naming, buffers §5.7).

### Implementation rules

- **No spec references in implementation:** do not mention proposal/RFC/openspec filenames, milestone names, or `§…` markers in `spi_nand_flash/` production code, `Kconfig` help, or `priv_include/` / `src/` comments — keep those docs in the read-order above only; implementation text stays self-contained.
- **ESP-IDF / FreeRTOS:** match existing patterns in `spi_nand_flash` (`esp_err_t`, logging tags, `heap_caps_malloc` where DMA is required, task vs ISR context — anonymous init runs in **init / task** context like today’s `nand_init_device`).
- **Scope control:** touch **`spi_nand_flash/`**, **`test_app/`**, **`host_test/`** as each milestone specifies; **do not** modify vendored **Dhara** unless a milestone explicitly requires it (none do by default).
- **Kconfig off:** `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n` ⇒ **no** Tier 2/3 behavior and baseline failure codes when anonymous was absent (proposal §5.1).
- **Linux:** no real ONFI/SPI anonymous probe on `IDF_TARGET_LINUX`; keep emulator path per proposal §8 and the **Tests** milestone §10.D notes.
- **Commits:** follow project convention for this repo (e.g. `feat(spi_nand_flash): …` / `fix` / `test` as appropriate); keep PRs close to **~500–700 LOC** meaningful diff when possible ([`README.md`](README.md) conventions). **Tier 2** may be split into two stacked PRs per that milestone doc if the diff is too large.

### Workflow each milestone

1. Read the **milestone** doc and list files you will change.
2. Implement; resolve ambiguities using **proposal §…** only.
3. Run the **Verification** section commands from that doc (and regressions: default `test_app` / `host_test` builds).
4. Check every **Acceptance criteria** checkbox; fix gaps before claiming done.
5. Open or update PR; only then start the **next milestone**.

### Milestone index (open the matching file)

| Order | Document |
|-------|----------|
| Tier 1 | [`step-tier1-foundation-and-database-path.md`](step-tier1-foundation-and-database-path.md) |
| Tier 2 | [`step-tier2-onfi.md`](step-tier2-onfi.md) |
| Tier 3 | [`step-tier3-manual-and-public-api.md`](step-tier3-manual-and-public-api.md) |
| Tests | [`step-tests-ci-host-linux.md`](step-tests-ci-host-linux.md) |
| Docs | [`step-docs-and-changelog.md`](step-docs-and-changelog.md) |

### Definition of “epic done”

All five milestones’ acceptance criteria satisfied; proposal [**§10**](../../anonymous_chip_detect_proposal.md) merge bar addressed (**A–D**) as the **Tests** and **Docs** milestones assign; **§10.C** hardware evidence procedure exists and is satisfied per project policy.

---

## One-line pointer for maintainers

**Author step specs:** [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md) · **Execute implementation:** this file.
