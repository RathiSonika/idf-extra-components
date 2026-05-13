# Change folder: Anonymous chip detection

**Specification (normative for *what* to build):** [`../../anonymous_chip_detect_proposal.md`](../../anonymous_chip_detect_proposal.md)

**Product intent / tiers (RFC mirror; defer to proposal where OOB differs):** [`../../RFC_Anonymous_Chip_Detect.md`](../../RFC_Anonymous_Chip_Detect.md)

**Baseline (init path, `include/` stability, Linux, concurrency):** [`../../baseline.md`](../../baseline.md)

**POC branch (ideas only, non-mergeable):** `feat/nand_generic_fallback` — gaps vs merge bar: proposal §12.

**Ordering vs configurable OOB:** Ship anonymous detection on the **baseline** fixed **4-byte** marker model at column `page_size` (`nand_impl.c`); RFC OOB snippets that assume configurable spare maps **do not** apply to v1 — see proposal §5.2 (spare row), §5.4, header independence note.

**Handoff for agents:** [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md) (author step docs) · [`IMPLEMENTATION_PROMPT.md`](IMPLEMENTATION_PROMPT.md) (execute steps 01–10 in code)

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

1. **Branch naming:** e.g. `feat/anonymous-chip-step-03-param-page-spi` (team convention may vary).
2. **Touch surface:** Prefer **`spi_nand_flash/`** (`Kconfig`, `CMakeLists.txt`, `src/`, `include/`, `priv_include/`) plus **`test_app/`** and **`host_test/`** as each step specifies. Do **not** modify vendored **Dhara** unless a step explicitly requires it (prefer avoid; proposal does not require Dhara edits for anonymous init).
3. **Kconfig off:** **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`** ⇒ no Tier 2/3 runtime behavior; binary/behavior aligned with pre-change baseline for init failure codes when anonymous was absent.
4. **Line-count target:** Aim for **~500–700 LOC** meaningful diff per PR; split steps if a change balloons.
5. **Path links in step docs:** From `openspec/changes/anonymous-chip-detect/`, **`../../../`** = component root (`spi_nand_flash/`), **`../../`** = `openspec/`.

---

## Dependency rule

**Do not merge step N+1 if step N’s acceptance criteria fail.** Implement steps **01 → 10** in order unless a step doc explicitly marks an optional parallel track (none by default).

```text
01 ─► 02 ─► 03 ─► 04 ─► 05 ─► 06 ─► 07 ─► 08 ─► 09 ─► 10
```

Steps **08** (CI matrix + on-target smoke where feasible) and **09** (host CRC + Linux §10.D) may be **one combined PR** only if the combined diff stays reviewable; otherwise keep separate.

---

## Ordered steps

| Step | Document | Short description |
|------|----------|-------------------|
| **01** | [step-01-kconfig-and-cmake-gates.md](step-01-kconfig-and-cmake-gates.md) | Master + manual Kconfig, CMake source gates; **no** runtime behavior change |
| **02** | [step-02-priv-types-onfi-crc.md](step-02-priv-types-onfi-crc.md) | `priv_include/`: packed parameter page layout, CRC API declaration, ONFI constants |
| **03** | [step-03-spi-parameter-page-read.md](step-03-spi-parameter-page-read.md) | Target SPI: read parameter page primitive(s); bounded heap §5.7; no init integration |
| **04** | [step-04-onfi-parse-and-init-module.md](step-04-onfi-parse-and-init-module.md) | Parse/validate ONFI fields; `num_luns==1`; populate geometry/delays; SIO-only; internal `chip_source` + anonymous flag |
| **05** | [step-05-nand-init-device-tier-integration.md](step-05-nand-init-device-tier-integration.md) | `nand_init_device`: tier ordering, `ESP_ERR_NOT_FOUND`, Tier 1 `chip_source` = DATABASE |
| **06** | [step-06-manual-tier-kconfig-and-init.md](step-06-manual-tier-kconfig-and-init.md) | Tier 3: Kconfig geometry/delays, validation, `ESP_ERR_INVALID_ARG` on bad combos |
| **07** | [step-07-public-api-chip-source.md](step-07-public-api-chip-source.md) | `spi_nand_chip_source_t`, `spi_nand_get_chip_source` in `include/spi_nand_flash.h` + `nand.c` |
| **08** | [step-08-test-app-ci-and-hardware-evidence.md](step-08-test-app-ci-and-hardware-evidence.md) | `sdkconfig.ci.anonymous`, pytest matrix, §10.B/C procedure |
| **09** | [step-09-host-test-crc-and-linux-parity.md](step-09-host-test-crc-and-linux-parity.md) | §10.A CRC vectors; §10.D Linux anonymous-off parity |
| **10** | [step-10-docs-and-changelog.md](step-10-docs-and-changelog.md) | README / CHANGELOG; pointers to proposal + RFC |

Ordered step documents (`step-01-…` through `step-10-…`) follow the tone of [`../configurable-oob-layout/`](../configurable-oob-layout/) (scope, non-goals, acceptance, verification). This epic does **not** depend on configurable OOB layout work.
