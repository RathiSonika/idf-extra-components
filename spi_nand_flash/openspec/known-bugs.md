# Known bugs / follow-ups — `spi_nand_flash`

> **Artifact type:** OpenSpec discovery — confirmed defects and intended fix directions.  
> **Source of truth:** [`baseline.md`](baseline.md) §11.4. This file mirrors that section so the [feature inventory](feature-module-inventory.md) stays focused on behavior and operational constraints (e.g. §9) without duplicating defect tracking.  
> **Scope:** These issues are **not** part of the stable behavior that changes must preserve; they are explicitly queued to be fixed. When §11.4 changes in `baseline.md`, update this file to match.

---

## 11.4 Known bugs / follow-ups (queued for next release)

**OpenSpec note (configurable OOB layout epic):** The two items that previously lived here are **fixed in-tree**:

| ID | Fix |
|----|-----|
| **§11.4.1** `nand_emul_get_stats` missing | Implemented in `src/nand_linux_mmap_emul.c`; host test under `CONFIG_NAND_ENABLE_STATS` (step **10**). |
| **§11.4.2** Handle / Dhara alloc not pinned to internal RAM | `MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT` for handle and Dhara private struct (step **05**). |

No further entries are queued in this section at the time of the above sync. New defects should be added here and in `baseline.md` §11.4 together.

---

## Document history

| Version | Note |
| --- | --- |
| 1.0 | Split from feature inventory; content aligned with `baseline.md` §11.4. |
| 1.1 | §11.4.1 / §11.4.2 marked fixed after configurable OOB steps 10 / 05. |
