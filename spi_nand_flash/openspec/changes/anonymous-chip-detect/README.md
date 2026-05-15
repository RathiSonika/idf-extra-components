# Change folder: Anonymous chip detection

**Specification (normative for *what* to build):** [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md)

**Product intent / tiers (RFC mirror; defer to proposal where OOB differs):** [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md)

**Baseline (init path, `include/` stability, Linux, concurrency):** [`../../baseline.md`](../../baseline.md)

**POC branch (ideas only, non-mergeable):** `feat/nand_generic_fallback` — gaps vs merge bar: proposal §12.

**Ordering vs configurable OOB:** Ship anonymous detection on the **baseline** fixed **4-byte** marker model at column `page_size` (`nand_impl.c`); RFC OOB snippets that assume configurable spare maps **do not** apply to v1 — see proposal §5.2 (spare row), §5.4, header independence note.

**Handoff for agents:** [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md) (author step docs) · [`IMPLEMENTATION_PROMPT.md`](IMPLEMENTATION_PROMPT.md) (execute tier milestones in code)

---

## Locked decisions (implementers must not re-litigate)

Summarized from proposal §5–§7 and §10; normative detail stays in [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md).

| Topic | Decision |
|--------|----------|
| **Tier order** | **1** Database → **2** ONFI → **3** Manual (if enabled). Tier 1 fails → Tier 2 when **either** unknown manufacturer **or** known mfr but vendor init does not yield a ready device (proposal §5.1). |
| **Tier 1 success invariant** | With `CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`, runtime matches baseline; with `y`, Tier 1 success adds **no** extra SPI traffic vs today (proposal §5.1). |
| **Final failure (anonymous on)** | After all applicable tiers exhausted: **`ESP_ERR_NOT_FOUND`**. With master **`n`**, failure **`esp_err_t`** values stay **baseline** (proposal §5.1). |
| **ONFI gate** | Parameter page bytes **0–3 == `ONFI`**; **CRC** validate (e.g. 0–253 vs 254–255 per ONFI); **no** `READ_ID @ 0x20` requirement (proposal §5.2). |
| **Multi-LUN** | **`num_luns != 1`** ⇒ ONFI tier **fails** (v1); Tier 1 database multi-LUN unchanged (proposal §5.2a). |
| **`spare_bytes_per_page`** | **Ignore** for v1 init/geometry; optional log only; **`nand_flash_geometry_t`** has no public `oob_size` (proposal §5.2, §5.4–§5.5). |
| **Markers / OOB** | Same as baseline: **4 bytes** at **`page_size`** for BBM + page-used; no `spi_nand_oob_layout_t` / experimental OOB Kconfig for this epic (proposal §5.4). |
| **Quad / QE** | Tier 2/3 success ⇒ **SIO only**; no QE; **`ESP_LOGW`** if config requested QOUT/QIO (proposal §5.6). |
| **Flags / API** | **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** internal only; **`nand_parameter_page_t`** stays **`priv_include/`**; public provenance via **`spi_nand_get_chip_source`** + **`spi_nand_chip_source_t`** (proposal §7). |
| **Kconfig** | **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT`** master, default **`n`**; **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`** depends on master, default **`n`**; manual is explicit opt-in with user geometry — no “silent safe defaults” on arbitrary NAND (proposal §6). |
| **Linux** | Anonymous SPI/ONFI **not** supported on `IDF_TARGET_LINUX`; emulator path unchanged by default (proposal §8). |
| **Logging** | Tier 2/3: **`ESP_LOGW`** with production-verification warning (proposal §9, RFC guardrails). |
| **Probe buffers** | Bounded **heap**, DMA-capable where SPI requires, **free on all paths** (proposal §5.7). |
| **Testing merge bar (§10)** | **A** CRC host tests; **B** `sdkconfig.ci.anonymous` + pytest matrix; **C** hardware evidence on ONFI path (procedure documented); **D** Linux: anonymous Kconfig does not alter emulator init. |

---

## Conventions for every PR

1. **Branch naming:** e.g. `feat/anonymous-chip-tier1-foundation`, `feat/anonymous-chip-tier2-onfi` (team convention may vary).
2. **Touch surface:** Prefer **`spi_nand_flash/`** (`Kconfig`, `CMakeLists.txt`, `src/`, `include/`, `priv_include/`) plus **`test_app/`** and **`host_test/`** as each milestone specifies. Do **not** modify vendored **Dhara** unless a milestone explicitly requires it (prefer avoid; proposal does not require Dhara edits for anonymous init).
3. **Kconfig off:** **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`** ⇒ no Tier 2/3 runtime behavior; binary/behavior aligned with pre-change baseline for init failure codes when anonymous was absent.
4. **Line-count target:** Aim for **~500–700 LOC** meaningful diff per PR; if **Tier 2** is too large for one review, split into two stacked PRs (e.g. ONFI library without `nand_init_device` hook, then init integration) while keeping a single milestone doc as the contract.
5. **Path links in step docs:** From `openspec/changes/anonymous-chip-detect/`, **`../../../`** = component root (`spi_nand_flash/`), **`../../`** = `openspec/`.

---

## Dependency rule

**Do not merge the next milestone if the previous milestone’s acceptance criteria fail.** Implement in order:

```text
Tier1 ─► Tier2 ─► Tier3 ─► Tests ─► Docs
```

**Tests** may be **one combined PR** with **Tier 3** only if the diff stays reviewable; otherwise keep **Tier 3** then **Tests** as separate merges.

---

## Ordered milestones

| Milestone | Document | Short description |
|-----------|----------|-------------------|
| **Tier 1** | [step-tier1-foundation-and-database-path.md](step-tier1-foundation-and-database-path.md) | Kconfig + CMake gates; internal `chip_source`; `nand_init_device` Tier 1 only — **no** ONFI/manual/public API (proposal §5.1 Tier 1, §6, §8) |
| **Tier 2** | [step-tier2-onfi.md](step-tier2-onfi.md) | Private ONFI types + CRC; SPI parameter page read; parse/init module; `nand_init_device` calls ONFI after Tier 1 failure (proposal §5.2, §5.6–§5.7, §9) |
| **Tier 3** | [step-tier3-manual-and-public-api.md](step-tier3-manual-and-public-api.md) | Manual Kconfig + Tier 3 init; `spi_nand_chip_source_t` + `spi_nand_get_chip_source` (proposal §5.3, §7) |
| **Tests** | [step-tests-ci-host-linux.md](step-tests-ci-host-linux.md) | `test_app` CI preset + pytest + §10.C procedure; `host_test` CRC vectors + Linux §10.D (proposal §10) |
| **Docs** | [step-docs-and-changelog.md](step-docs-and-changelog.md) | Component README / CHANGELOG; pointers to proposal + RFC |

Milestone documents follow the tone of [`../configurable-oob-layout/`](../configurable-oob-layout/) (scope, non-goals, acceptance, verification). This epic does **not** depend on configurable OOB layout work.
