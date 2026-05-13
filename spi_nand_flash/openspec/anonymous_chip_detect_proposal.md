# Proposed change: Anonymous chip detection (`spi_nand_flash`)

**Artifact type:** OpenSpec feature specification (SDD input — not baseline).  
**Normative product intent:** [`RFC_Anonymous_Chip_Detect.md`](RFC_Anonymous_Chip_Detect.md) — *Anonymous Chip Detection Strategy* (in-tree RFC mirror under `openspec/`).  
**Sources of truth for current behavior:** [`baseline.md`](baseline.md), [`feature-module-inventory.md`](feature-module-inventory.md).  
**Independence:** This epic **does not** depend on the configurable OOB layout work ([`configurable_oob_layout_proposal.md`](configurable_oob_layout_proposal.md)). Anonymous detection ships on the **existing** fixed marker model in `nand_impl.c` (baseline §5.3, Dhara callbacks). A later OOB-layout epic may add per-part spare maps **including** RFC-minimal anonymous profiles; that is explicitly **out of scope** here.  
**POC reference (non-mergeable):** branch `feat/nand_generic_fallback` in `idf-extra-components` — validates hardware flow; this spec **supersedes** POC details where they conflict with the RFC or with merge-quality bar below.

---

## 1. Problem statement

Today, chip bring-up requires a **known JEDEC manufacturer ID** and a **matching device table entry** in `src/devices/nand_*.c`. Unknown IDs fail init (`ESP_ERR_NOT_FOUND` / `ESP_ERR_INVALID_RESPONSE` path — baseline §7.2, §5.3).

Applications and integrators need a **controlled fallback** so that:

- Standard **ONFI parameter page** parts can be probed without adding every part number to the database first.
- **Manual** geometry can be supplied when ONFI is absent or unusable (lab / emergency only).

The fallback must be **safe by default**: conservative spare assumptions, explicit **detection provenance**, and clear **non-production** warnings — per RFC.

---

## 2. Goals

1. **Three-tier detection hierarchy** (RFC), applied during the same init entry points as today (`nand_init_device` / BDL factories that call it — baseline §5.3–§5.4):
   - **Tier 1 — Database:** unchanged semantics for known `(manufacturer_id, device_id)` and vendor init.
   - **Tier 2 — ONFI:** if Tier 1 does not match, attempt ONFI-based discovery of geometry, timing, and ECC hints from the **parameter page**, subject to validation below.
   - **Tier 3 — Manual:** if Tier 2 fails and **explicitly enabled** in Kconfig, initialize from **menuconfig-supplied** geometry and delays (RFC Tier 3).

2. **Provenance and introspection:**
   - Record **which tier** succeeded (`DATABASE` / `ONFI` / `MANUAL`).
   - Expose this to applications via a **small, stable** public API (RFC suggests `spi_nand_get_chip_source`; naming may be adjusted in implementation if it collides with existing symbols, but the **capability** is required).

3. **Safety guardrails (RFC §Implementation Notes):**
   - **Warning-level logs** when Tier 2 or Tier 3 is used; message must state that geometry/OOB assumptions must be verified before production.
   - Mark internally that the chip configuration is **not datasheet-backed** using **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** on **driver-private** state (e.g. `priv_include/nand.h` / device struct — **resolved:** not part of the stable public geometry bitmask; applications use **`spi_nand_get_chip_source`** only — §7).

4. **Documentation:** README / CHANGELOG entries describing tiers, Kconfig gates, limitations, and production guidance (RFC §Usage Recommendations).

---

## 3. Non-goals

- **Guaranteed filesystem or FTL correctness** for anonymous modes beyond what Dhara already provides (baseline §9.4). Anonymous init does not add new durability semantics.
- **Automatic vendor quirks** (OTP layout differences, non-ONFI parameter page placement, proprietary ECC regions) without a database row — out of scope except where explicitly detected and documented.
- **Parallel NAND / non-SPI NAND** — baseline scope unchanged.
- **Replacing** the device database for production: Tier 2/3 remain **bring-up / contingency** paths; production systems should still add validated parts to Tier 1 (RFC §Usage Recommendations).

---

## 4. Current behavior (baseline-aligned)

- **Init** reads manufacturer ID, switches on vendor, then device ID tables in per-vendor modules (`nand_impl.c` + `src/devices/*.c` — baseline §3.3, §5.3).
- **OOB / markers:** fixed **4-byte** pattern at column `page_size` for BBM + page-used (`nand_impl.c`; same model summarized in [`configurable_oob_layout_proposal.md`](configurable_oob_layout_proposal.md) §1.2 as documentation only). Dhara depends on these paths unchanged.
- **Linux target:** synthetic chip identity and geometry in emulator — baseline §4.7, §7.1. Anonymous ONFI over SPI **does not apply** on `linux`; behavior must be explicitly specified (see §8).

---

## 5. Desired behavior — functional requirements

### 5.1 Tier ordering and failure modes

| Order | Tier | When it runs | On success | On failure |
|------|------|----------------|------------|------------|
| 1 | Database | Always first (current vendor match) | `chip_source = DATABASE`, no anonymous flag | Continue to Tier 2 |
| 2 | ONFI | Tier 1 did not produce a ready device | `chip_source = ONFI`, anonymous flag set | Continue to Tier 3 if enabled, else **`ESP_ERR_NOT_FOUND`** |
| 3 | Manual | Kconfig manual fallback **enabled** and Tier 2 failed | `chip_source = MANUAL`, anonymous flag set | **`ESP_ERR_NOT_FOUND`** |

**Final error after all tiers exhausted (resolved):** When anonymous detection is **enabled** at build time and **all** applicable tiers have been tried without success, `nand_init_device` (and callers) return **`ESP_ERR_NOT_FOUND`**. With anonymous detection **disabled**, failure codes remain **unchanged** from baseline (e.g. vendor / `detect_chip` paths as today).

**Tier 1 failure → Tier 2 (resolved — broad fallback):** Run Tier 2 when Tier 1 does **not** yield a fully initialized database-backed device, **including**:
- **Unknown manufacturer ID** (no vendor dispatch), or  
- **Known manufacturer** but **vendor init fails**, including **unknown / unlisted device ID** or any other `esp_err_t` from the vendor path before the device is ready.

Rationale: an ONFI-capable part may be absent from the device table or use a new ID variant; ONFI remains a valid second probe. Implementers should still avoid unnecessary register writes in vendor init on the “unknown device” branch where practical, but the policy does **not** skip ONFI solely because the manufacturer byte matched a known vendor.

**Invariant:** With **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`** (§6), runtime behavior matches **pre-change** baseline (no Tier 2/3 code paths). With master **`y`**, a chip that succeeds **Tier 1** must see **no extra SPI traffic** beyond today’s successful init (fallback runs only after Tier 1 failure).

### 5.2 ONFI tier — normative algorithm (RFC-aligned; POC may differ)

The RFC describes:

1. **Parameter page read** (implementation-defined sequence: e.g. `READ_PARAM_PAGE` / `0xEC`, OTP / config register bits, fixed row addresses per datasheet — as required by the part).

2. **ONFI signature:** Bytes **0–3** of the read **256-byte** page must equal ASCII **`ONFI`**. This is the only required signature gate; it is **not** specified via a particular `READ_ID` address (that was misleading for SPI NAND).

3. **Integrity:** CRC over the parameter page (e.g. bytes **0–253** vs **254–255** per ONFI — **verify** polynomial / init value against ONFI 1.0 / vendor errata in implementation).

4. **Field extraction (minimum):**

   | Logical quantity | ONFI / spec source | Use |
   |------------------|--------------------|-----|
   | `data_bytes_per_page` | Parameter page | `log2_page_size`, `page_size` |
   | `pages_per_block` | Parameter page | `log2_ppb`, block geometry |
   | `blocks_per_lun` | Parameter page | **`num_blocks`** when **`num_luns == 1`** (see multi-LUN rule below) |
   | `num_luns` | Parameter page | **v1:** must be **1**; otherwise ONFI tier **fails** (option 3 — §5.2a) |
   | `spare_bytes_per_page` | Parameter page | **v1 — ignore for init / geometry:** target **`nand_flash_geometry_t`** has **no `oob_size`**; `nand_impl.c` uses **fixed 4-byte** marker access at **`page_size` + 0..3** only. Do **not** require mapping ONFI spare into geometry for this epic. **Optional:** log `spare_bytes_per_page` for bring-up. **`spare_bytes_per_page`** becomes **authoritative for driver spare length** only under the **future configurable OOB layout** work (see [`configurable_oob_layout_proposal.md`](configurable_oob_layout_proposal.md)). |
   | `t_r`, `t_prog`, `t_bers` (max typical) | Parameter page | Delays (`read_page_delay_us`, etc.) |
   | `ecc_correctability` | Parameter page | Logging / future use; **v1** does not introduce new ECC layout logic beyond what baseline + vendor-less minimal setup already need (§5.5) |

5. **Device info string:** Populate human-readable fields where possible (manufacturer / model ASCII from parameter page), without claiming database `chip_name` stability for unknown parts.

**5.2a Multi-LUN (resolved — option 3):** If **`num_luns` != 1**, anonymous **ONFI (Tier 2) does not succeed**: reject this parameter page for v1 (no `num_blocks = blocks_per_lun * num_luns` linearization). Log that multi-LUN parts are **unsupported** for anonymous ONFI until explicitly designed. Init then attempts **Tier 3** (if enabled) or ends with the usual aggregate failure (**§5.1**). **Tier 1 (database)** parts with multi-LUN remain handled by existing vendor code — this rule applies only to the **anonymous ONFI** path.

### 5.3 Manual tier (RFC Tier 3)

When **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`** is **`y`** (requires **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`** — §6), the user supplies geometry and delays via Kconfig (exact symbol names TBD; POC used `SPI_NAND_GENERIC_*`):

- User supplies: page size, pages per block, block count, `t_r` / `t_prog` / `t_bers`.
- **Validation:** power-of-two constraints where required by existing geometry code; reject impossible combinations with **`ESP_ERR_INVALID_ARG`** and a clear log.
- **Defaults** must be conservative and documented; manual mode remains **opt-in**.

### 5.4 OOB and marker policy (baseline only — no configurable layout dependency)

**v1 rule:** Anonymous detection (Tier 2 / Tier 3) must keep **byte-identical** marker behavior to Tier 1 today: **four bytes** programmed/read at **column `page_size`**, bytes **0–1** BBM (`0xFF,0xFF` good), bytes **2–3** page-used (`0xFF,0xFF` free), same `nand_is_bad` / `nand_mark_bad` / `nand_prog` / `nand_is_free` / `nand_copy` semantics as current `nand_impl.c`. No `spi_nand_oob_layout_t`, no `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` requirement.

**ONFI `spare_bytes_per_page` (v1):** Not consumed for geometry or init decisions (see §5.2 table row). Same **four-byte marker** contract as Tier 1 at **`page_size`**; the die must physically allow programming those columns — **out of scope** for this epic to validate against ONFI spare beyond optional logging.

**RFC note:** The RFC’s “**2 bytes free OOB** only” profile is **deferred** to the separate configurable-OOB epic (or a follow-up) if product still wants that stricter exposure **without** changing Dhara’s on-flash marker contract. This epic intentionally matches **baseline** markers so Dhara and existing tests need no layout refactor.

**Manual tier:** same baseline marker model as Tier 1; **no** spare-size Kconfig in v1 (POC-style). User is responsible that the attached part matches **page / block** counts and supports marker I/O at **`page_size`** per datasheet.

### 5.5 ECC and feature registers

- Vendor init for anonymous parts may **not** exist. The implementation must define **minimal** register setup (e.g. block unlock, default internal ECC consistent with how the part ships) and document **assumptions**.
- **Geometry types:** On **target**, `nand_flash_geometry_t` carries **no `oob_size`** today — anonymous ONFI/manual init **must not** invent a public `oob_size` field for v1. Set **`nand_ecc_data_t`** / delays / page–block counts only, consistent with baseline `nand_impl.c`. Binding **`spare_bytes_per_page`** (and real spare length) into the driver is **deferred** to the configurable OOB layout epic.

### 5.6 Quad / I/O mode (resolved — option 3)

- For **`chip_source`** **`ONFI`** or **`MANUAL`** (Tier 2/3 success), the driver **does not** enable quad (`QE`) and **does not** assume `has_quad_enable_bit` / `quad_enable_bit_pos`. All SPI NAND traffic for that device uses **SIO-style** access (single I/O), regardless of whether **`spi_nand_flash_config_t::io_mode`** requests **`QOUT`** or **`QIO`**.
- **Behavior:** downgrade the effective I/O path to **SIO** for anonymous init; **`ESP_LOGW`** when the caller requested QOUT/QIO so the mismatch is visible. Init **succeeds** (subject to other checks); callers who need quad must use a **database**-known part with correct vendor QE setup.
- **Rationale:** avoids the POC’s unsafe `has_quad_enable_bit = 1` / `quad_enable_bit_pos = 0` guess. A future revision may re-enable quad for anonymous parts only with datasheet-backed register rules.

### 5.7 Parameter page probe buffer (resolved — option 1)

- Use **bounded heap** allocation for ONFI parameter page probing (e.g. read buffers sized to **at most** a fixed multiple of **256 bytes** × number of copies/rows probed — document the exact upper bound in code or README).
- **DMA-capable** where the SPI path requires it (`MALLOC_CAP_DMA` etc., matching `spi_nand_oper` patterns).
- **Free before return** on all paths (success, failure, early exit); **no** persistent probe buffer on `spi_nand_flash_device_t` for this path.

---

## 6. Build and configuration (Kconfig)

### 6.1 Two-level gates (resolved)

| Option | Role | Default | `depends on` |
|--------|------|---------|----------------|
| **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT`** (name may be adjusted for style) | **Master:** compiles in Tier 2 (ONFI) and shared anonymous plumbing; allows init to fall back from Tier 1 when this path can run. **Off** ⇒ baseline-only chip detect (smaller binary, no anonymous SPI/OTP sequences). | **`n`** | Non-Linux target for SPI probe paths unless host story is defined (§8) |
| **`CONFIG_NAND_FLASH_ANONYMOUS_MANUAL`** (name TBD) | **Manual tier only:** user explicitly opts in to Tier 3. All geometry and delay fields are **user-defined** in Kconfig; there is **no** “safe default” profile that silently runs on an arbitrary unknown NAND. If **master is `n`**, manual is unavailable (not shown or forced off). | **`n`** | Master anonymous **`y`** |

- **ONFI (Tier 2)** is enabled together with the **master** switch (no separate third bool required for v1 unless footprint audit later splits it).
- **Manual (Tier 3)** requires **both** switches **`y`**: user chose anonymous support **and** chose manual fallback **and** supplied parameters from the datasheet.

### 6.2 Production vs bring-up (intent)

- **Not “debug-only code” in the sense of temporary hacks:** ONFI + optional manual are **supported bring-up / lab / contingency** paths (RFC usage guidance).
- **Production firmware** often sets **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`** to reduce code size and to ensure init **never** succeeds via Tier 2/3 on the factory floor unless you explicitly want that policy.
- **Production deployments** should still prefer **Tier 1 (database)** after validation; anonymous tiers remain **second-class** from a product-quality perspective even when enabled at build time.

Manual parameters are **never** trusted unless the user enabled manual mode and filled values from hardware documentation — **unknown NAND + wrong defaults must not appear to work by accident.** Integer fields may still carry **menuconfig placeholder defaults** for build ergonomics; help text must state that every value **must** match the datasheet, and the implementation **may** reject unchanged placeholders or require an explicit confirmation option (TBD in implementation steps).

---

## 7. Public API and types

**Minimum shipping surface:**

- `typedef enum { SPI_NAND_CHIP_SOURCE_DATABASE, SPI_NAND_CHIP_SOURCE_ONFI, SPI_NAND_CHIP_SOURCE_MANUAL } spi_nand_chip_source_t;` (or equivalent namespaced enum).
- `esp_err_t spi_nand_get_chip_source(spi_nand_flash_device_t *handle, spi_nand_chip_source_t *out);`

**Types:**

- **`nand_parameter_page_t` and related ONFI constants** (packed layout, CRC coverage, copy count): **resolved — keep in `priv_include/`** for v1. Do not publish the raw parameter page struct from `include/`; applications use existing **`nand_device_info_t`** / **`nand_flash_geometry_t`** on the handle plus **`spi_nand_get_chip_source`**. Promotion to `include/` is a **follow-on** if a stable public need appears.

**Flags (resolved):**

- Use **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** on **internal** chip/device state only (same conceptual bit the RFC calls “anonymous”; not `…_GENERIC`). **Do not** publish this bit through **`nand_flash_geometry_t::flags`** or other **`include/`**-visible geometry fields in v1 — avoids expanding the stable public bitmask contract. Callers use **`spi_nand_get_chip_source`** for detection provenance.

**Stability:** follow baseline §4.0 — anything added under `include/` is **stable**; avoid leaking unstable ONFI offsets into public headers.

---

## 8. Linux / host target

- **Default:** anonymous SPI probe **not supported** on `IDF_TARGET_LINUX`; `nand_init_device` behavior remains emulator-driven (baseline §4.7).
- **Optional future:** inject parameter page bytes into emulator for tests — **out of scope** for v1 unless listed in a follow-up step.

---

## 9. Logging

- **Tier 1:** existing log level (typically `ESP_LOGD` / `ESP_LOGI` as today).
- **Tier 2 / 3:** **`ESP_LOGW`** with explicit text that detection was **ONFI** or **manual**, and that **parameters and OOB** must be verified before production (RFC §Safety Guardrails).

---

## 10. Testing and acceptance criteria (resolved — **A**, **B**, **C**, **D**)

**A — CRC unit tests (host-safe):** Golden-vector tests for ONFI parameter page **CRC** (bytes 0–253 vs 254–255) and any pure-parse helpers; runs in **`host_test`** (or equivalent host/native target) **without** SPI NAND hardware.

**B — `sdkconfig` preset + CI matrix:** Add a preset (e.g. **`sdkconfig.ci.anonymous`**) with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=y`** (and manual tier per test plan), wire **`test_app`** pytest (and **`host_test`** where applicable) so **anonymous-on** is built in CI the same way as legacy/BDL presets. Preset and pytest matrix entry are **merge blockers**.

**C — Hardware validation (ONFI path):** On **at least one** ONFI-capable part **not** represented in the device table (or with ID masked so Tier 1 fails), run **read/write/GC smoke** (existing marker-dependent coverage). **Evidence** (lab log, CI job on hardware bench, or signed checklist per release policy) is a **merge requirement**. Default **GitHub CI** may omit the on-target run **only** if project policy explicitly allows merge pending hardware; the **procedure and ownership** for C must still be documented in README or test doc so the gap is visible.

**D — Linux / anonymous interaction:** **`host_test`** (or shared init test) asserts that on **`IDF_TARGET_LINUX`**, anonymous SPI/ONFI paths do **not** alter baseline emulator init (§8) — e.g. synthetic chip path unchanged when anonymous Kconfig is present.

**Regression (always):** Full **`test_app`** + **`host_test`** matrix remains green with **`CONFIG_NAND_FLASH_ANONYMOUS_DETECT=n`** (baseline §8); no anonymous-only code path may break default presets.

---

## 11. Implementation plan placeholder

After this spec is accepted, split work into ordered steps under:

`openspec/changes/anonymous-chip-detect/README.md`

Suggested high-level PR sequence (indicative):

1. Types + Kconfig gates (no behavior change default off).
2. ONFI parameter page transport + **signature (bytes 0–3 == `ONFI`)** + CRC validation (probe buffer per §5.7).
3. `spi_nand_onfi_init` (or equivalent): geometry, delays, **`nand_ecc_data_t`** defaults consistent with **baseline** marker I/O (§5.4); **no** `oob_size` from ONFI in v1 (§5.5).
4. Manual fallback module + validation.
5. `spi_nand_get_chip_source` + integration tests + docs.

Each step should cite this file + RFC + baseline sections.

---

## 12. POC (`feat/nand_generic_fallback`) — gap list (merge blockers)

The following must be resolved before merge; this spec treats them as **known deltas**:

| Area | POC behavior | Required for merge |
|------|----------------|-------------------|
| ONFI gate | OTP + row scan + CRC only | **Parameter page** path must validate **`ONFI`** at bytes **0–3** and CRC (RFC-aligned) |
| `nand_parameter_page_t` | POC exposed in `include/` | **Keep in `priv_include/`** only (§7); no public packed parameter page type in v1 |
| QE defaults | POC assumed QE bit at pos 0 | **§5.6 (resolved):** anonymous Tier 2/3 ⇒ **SIO only**, no QE; warn if config asked QOUT/QIO |
| Geometry | ONFI path may omit full `ecc_data` alignment | **`nand_ecc_data_t`** / delays / page–block fields consistent with baseline (§5.5); **ONFI:** **`num_luns == 1` only** (§5.2a); **no `oob_size` from ONFI** in v1 |
| OOB | POC / spec drift on `oob_size` | **Baseline** fixed 4-byte markers at `page_size`; **ignore `spare_bytes_per_page` for init** in v1 (§5.2, §5.4); defer to configurable OOB |
| Naming | POC `SPI_NAND_CHIP_FLAG_GENERIC`, `SPI_NAND_GENERIC_*` Kconfig | **`SPI_NAND_CHIP_FLAG_ANONYMOUS`** (internal-only, §7); Kconfig **`CONFIG_NAND_FLASH_ANONYMOUS_*`** (§6); drop “generic” wording |
| Dynamic alloc | POC heap probe buffer | **§5.7 (resolved):** bounded **heap**, DMA-capable where required, **free** on all exit paths |

---

## 13. References

- RFC (in-tree): [`RFC_Anonymous_Chip_Detect.md`](RFC_Anonymous_Chip_Detect.md) — product intent and tiered strategy; keep in sync with this proposal when decisions diverge.
- Baseline: [`baseline.md`](baseline.md) — init, stability, Linux, failure modes.
- **Follow-on (optional):** [`configurable_oob_layout_proposal.md`](configurable_oob_layout_proposal.md) — may later add RFC-minimal spare maps for anonymous parts; **not** a prerequisite for this epic.
- POC branch: `feat/nand_generic_fallback` — `nand_onfi.c`, `spi_nand_read_parameter_page`, `nand_read_parameter_page`, `nand_impl.c::nand_init_device`, `Kconfig` manual menu.
