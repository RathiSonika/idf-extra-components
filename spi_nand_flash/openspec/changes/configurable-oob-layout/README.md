# Implementation plan: Configurable OOB layout

**Parent proposal:** [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md)

**Repo context:** `spi_nand_flash` is an **ESP-IDF-managed component** living under the **idf-extra-components** project (not inside the ESP-IDF tree). Build/test via [`test_app/`](../../../test_app/) and [`host_test/`](../../../host_test/) in this component directory.

**Handoff for agents:** copy-paste prompt in [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md).

This folder splits that proposal into **ordered, review-sized PRs** (target **≤500–700 lines of meaningful diff** per PR; docs-only or mechanical moves may exceed slightly if semantically trivial).

## Conventions for every PR

1. **Branch naming:** e.g. `feat/oob-layout-step-03-default-layout` (team convention may vary).
2. **Do not modify Dhara** (vendor component); only `spi_nand_flash` tree + **both** test apps ([`test_app/`](../../../test_app/) on-target, [`host_test/`](../../../host_test/) Linux).
3. **Kconfig off = baseline behavior:** When `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` is **not** set, behavior must match **pre-change** `nand_impl.c` / `nand_impl_linux.c` (see proposal §1.2).
4. **Default layout when Kconfig on:** For all chips supported **before** this work, default layout must reproduce **byte-identical** OOB marker patterns and column addressing vs §1.2 (proposal Q1).
5. **Types & APIs:** Keep new layout/field types in **`priv_include/`** until an explicit follow-up promotes symbols to `include/` (proposal §7 Q4).
6. **Linux parity:** Any PR that changes target `nand_impl.c` marker/OOB behavior under the Kconfig must either include the **Linux mirror** in the **same** PR or explicitly defer with a **blocking follow-up step** (avoid silent drift).
7. **Verification:** Each step lists minimal commands; **step 11** requires **`test_app` full on-target pytest pass** (legacy + BDL OOB presets) and **`host_test` full pass** — see step 11 doc.

## Ordered steps (reference IDs)

| Step | Document | Short description |
|------|----------|-------------------|
| **01** | [step-01-kconfig-and-build-guard.md](step-01-kconfig-and-build-guard.md) | Kconfig + CMake; no runtime behavior change |
| **02** | [step-02-priv-types-header.md](step-02-priv-types-header.md) | Private headers: region, layout ops, BBM descriptor, field IDs |
| **03** | [step-03-default-layout-table.md](step-03-default-layout-table.md) | Default `spi_nand_oob_layout_t` matching §1.2 |
| **04** | [step-04-xfer-scatter-gather-contiguous.md](step-04-xfer-scatter-gather-contiguous.md) | Xfer ctx init + scatter/gather (single-region fast path) |
| **05** | [step-05-device-state-and-init-hook.md](step-05-device-state-and-init-hook.md) | Device struct fields + init assigns default layout |
| **06** | [step-06-nand-is-free.md](step-06-nand-is-free.md) | Route `nand_is_free` through layout path when Kconfig on |
| **07** | [step-07-nand-prog-markers.md](step-07-nand-prog-markers.md) | Route `nand_prog` marker program through layout path |
| **08** | [step-08-nand-bbm-paths.md](step-08-nand-bbm-paths.md) | `nand_is_bad` / `nand_mark_bad` use layout BBM descriptor |
| **09** | [step-09-nand-copy.md](step-09-nand-copy.md) | `nand_copy` marker / OOB parity (incl. plane-equal branch) |
| **10** | [step-10-linux-parity.md](step-10-linux-parity.md) | `nand_impl_linux.c` (and emulator if needed): match target |
| **11** | [step-11-tests-and-ci-matrix.md](step-11-tests-and-ci-matrix.md) | **`test_app` + `host_test`** sdkconfig presets, pytest matrix; Kconfig on/off |
| **12** | [step-12-docs-and-follow-ups.md](step-12-docs-and-follow-ups.md) | CHANGELOG, proposal pointer, deferred work (MAX_REG, public API) |

## Dependency graph

```text
01 ─► 02 ─► 03 ─► 04 ─► 05 ─► 06 ─► 07 ─► 08 ─► 09 ─► 10 ─► 11 ─► 12
                              └───────────────┴──────────────┘
                                   (06–09 order may swap
                                    slightly if coordinated;
                                    see individual steps)
```

**Rule:** Do not merge a later step if an earlier step’s acceptance criteria fail.

## Path conventions in step documents

Step `.md` files live under `openspec/changes/configurable-oob-layout/`. Links use:

- **`../../../…`** — repository root (`spi_nand_flash/`: `src/`, `Kconfig`, `README.md`, …).
- **`../../…`** — `openspec/` siblings (`configurable_oob_layout_proposal.md`, `known-bugs.md`).

## Line-count discipline

- Prefer **one logical concern** per PR.
- If a step risks exceeding ~700 lines, split into **step N** and **step N-bis** (new file) rather than mixing refactors with behavior changes.
- **Golden rule:** another agent should implement the step **only** from the step `.md` + the parent proposal, without guessing intent.
