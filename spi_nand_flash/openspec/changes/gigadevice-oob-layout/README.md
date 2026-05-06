# Implementation plan: GigaDevice GD5F1GQ4xA-family OOB layout

**Parent proposal (this change):** [`../../gigadevice_oob_layout_proposal.md`](../../gigadevice_oob_layout_proposal.md)
**Framework parent (must be merged first):** [`../../configurable_oob_layout_proposal.md`](../../configurable_oob_layout_proposal.md) and **all** of [`../configurable-oob-layout/`](../configurable-oob-layout/) steps **01–12**.

**Repo context:** `spi_nand_flash` is an **ESP-IDF-managed component** under the **idf-extra-components** project (not inside the ESP-IDF tree). Build/test from [`test_app/`](../../../test_app/) and [`host_test/`](../../../host_test/).

**Handoff for agents:** copy-paste prompt in [`HANDOFF_PROMPT.md`](HANDOFF_PROMPT.md).

This folder splits the parent proposal into **ordered, review-sized PRs** (target **≤300–400 LOC** each; this is a small epic).

## Implementation decisions (locked for this plan)

Work proceeds **step 01 → step 04** in order; each step doc is the source of truth for that PR.

| Topic | Decision |
|--------|----------|
| **Selection mechanism** | Per-vendor `get_oob_layout(dev) -> const spi_nand_oob_layout_t *` hook. **No** `spi_nand_flash_config_t` runtime override. **No** Kconfig per-chip toggle. (Mirrors parent proposal's "table → generic only" decision.) |
| **DI scope** | **`0x31`, `0x32`, `0x25`, `0x35`** only — and only after each is **datasheet-verified** as a 64 B-spare Q4xA-family part. Any DI that turns out to be a 128 B-spare variant2 part **must be removed** from this set and deferred to a separate proposal. |
| **BBM length** | **2 bytes** (matches Espressif default), not Linux MTD's 1 — preserves byte-for-byte compatibility with default-layout-formatted volumes. |
| **`oob_bytes`** | **0** in the layout struct → defer to `nand_oob_device.c`'s `page_size → spare` mapping (= 64 for 2 KiB page). **No** Linux mmap stride change in this epic. |
| **API visibility** | New layout / vendor-hook symbols stay under **`priv_include/`** (parent proposal §7 Q4). |
| **Linux parity** | None of the four steps touches `nand_impl_linux.c` — the Q4xA layout uses the same 64-byte spare model the emulator already provides. |
| **Variant2 (128 B spare)** | **Out of scope.** Separate proposal needed (parent proposal Q3, deferred work in step 04 of this plan). |

## Conventions for every PR

1. **Branch naming:** e.g. `feat/gd-oob-step-01-q4xa-table`.
2. **Do not modify the vendored Dhara library.** Do not modify `nand_impl.c`, `nand_impl_linux.c`, or `dhara_glue.c` in this epic — the framework parent already routes them through the layout abstraction. If a step needs a change in those files, **stop and reopen the design**.
3. **Kconfig off ⇒ no behavior change.** When `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT=n`, this entire epic is compiled out; the new vendor hook field on the device handle is `#ifdef`-guarded the same way the rest of the OOB plumbing is in the parent epic.
4. **Default layout stays the fallback.** Any DI not listed in §2.2 of the parent proposal must continue to resolve to `nand_oob_layout_get_default()` — verified by host_test V3 in step 03.
5. **Default-preserves-bytes invariant** (parent proposal §7.0): a device formatted on the default layout, then re-mounted on a build that selects the Q4xA layout, must read identically — no migration. Verified by host_test V4 in step 03.
6. **Types & APIs:** new symbols stay under **`priv_include/`** until an explicit follow-up promotes them.
7. **PR size:** aim ≤300–400 meaningful LOC per step; one logical concern per PR.
8. **Verification gate:** step 02 cannot merge until each DI in §2.2 of the parent proposal is confirmed against its datasheet (Q4xA, 2 KiB page, 64 B spare, internal ECC always on).

## Ordered steps (reference IDs)

| Step | Document | Short description |
|------|----------|-------------------|
| **01** | [step-01-vendor-layout-table.md](step-01-vendor-layout-table.md) | Add `nand_oob_layout_get_gigadevice_q4xa()` (priv header + .c). No callers yet. |
| **02** | [step-02-vendor-selector-hook.md](step-02-vendor-selector-hook.md) | Add `vendor_get_oob_layout` field on the device handle; wire `spi_nand_gigadevice_init` to set the resolver; teach `nand_oob_device_layout_init` to consult it. **Datasheet verification gate**. |
| **03** | [step-03-tests.md](step-03-tests.md) | Host_test cases V1–V5 (layout enumeration, BBM contract, selector mapping, byte-for-byte compat, parity bound). Optional `test_app` preset `sdkconfig.ci.gigadevice_oob` (build-only minimum; full pytest if hardware available). |
| **04** | [step-04-docs-and-followups.md](step-04-docs-and-followups.md) | CHANGELOG entry; README short note; deferred work list (variant2 128 B spare, other vendors, ECC-mode-keyed layouts). |

## Dependency graph

```text
configurable-oob-layout step 01 .. step 12
                  ▼
              gigadevice 01 ─► 02 ─► 03 ─► 04
```

**Rule:** Do not merge a step in this epic if any step in `configurable-oob-layout` is unmerged.

## Path conventions in step documents

Step `.md` files live under `openspec/changes/gigadevice-oob-layout/`. Links use:

- **`../../../…`** — repository root (`spi_nand_flash/`: `src/`, `Kconfig`, `README.md`, …).
- **`../../…`** — `openspec/` siblings (`gigadevice_oob_layout_proposal.md`, `configurable_oob_layout_proposal.md`).
- **`../configurable-oob-layout/…`** — sibling implementation plan (the framework parent).

## Line-count discipline

- Prefer **one logical concern** per PR.
- This epic is **small** by design (one chip family, four DIs, advisory ECC enumeration). If a step's diff approaches 500+ LOC, that's a signal scope crept — split or push to a follow-up.
- **Golden rule:** another agent should implement the step **only** from the step `.md` + the parent proposal + `configurable_oob_layout_proposal.md`, without guessing intent.
