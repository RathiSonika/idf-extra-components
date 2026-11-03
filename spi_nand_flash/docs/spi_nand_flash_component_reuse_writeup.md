# Reusing `spi_nand_flash` via the ESP Component Registry

## Summary

The existing [`spi_nand_flash`](https://components.espressif.com/) driver (published on the ESP Component Registry) can be reused as the common NAND stack for both **primary on-chip NAND** and **external SPI NAND**, instead of duplicating NAND protocol and chip support inside ESP-IDF. With a small set of extensions—mainly a pluggable SPI transport and clear Raw vs WL BDL entry points—the same component can serve bootloader/partition access, storage, and dual-flash products.

## Motivation

Today the driver is GPSPI-oriented and used for external NAND. Products that want NAND as primary memory (app image on NAND, copy to PSRAM for execution; NOR still used for early boot) should not require a second, forked NAND implementation in IDF. Reusing the registry component keeps chip databases, ECC handling, and page/block operations in one place.

## Required changes (high level)

### 1. Transport layer (GPSPI / MSPI)

Add a thin **transport seam** under the NAND command layer so the same core can talk to:

- **GPSPI** — external NAND via `spi_master` / `spi_device_transmit` (current path)
- **MSPI** — primary NAND via IDF MSPI / flash-host style access (`common_command` and related APIs)

That allows one driver to integrate NAND as **primary memory** and as **external storage**.

### 2. Everything above the bus goes through BDL

- **Raw NAND BDL** — physical page/block access for bootloader, partition table, and app image read/write (no wear-leveling)
- **WL BDL** — logical storage for filesystem use cases

BDL keeps consumers (`esp_blockdev_t`) uniform and makes it straightforward to plug Raw into early boot and WL into storage later.

### 3. Dual SPI: primary + external at once

For boards with **on-chip primary NAND (MSPI)** and **external NAND (GPSPI)**, both transports can be enabled and used **independently**: same component code, **separate** device / Raw BDL handles (and WL only where storage needs it).

### 4. Dhara optional; other FTLs (e.g. UBI) for mixed raw + storage on one die

**Dhara** treats the flash as a **single** wear-leveled unit. That does **not** fit a layout where:

- some regions must stay **raw** (app / bootloader partitions), and  
- other regions need **WL + FS** on the **same** NAND.

For **two different NAND devices** (e.g. MSPI primary + GPSPI external), the current driver + Dhara remains fine: Raw (or no WL) on one device, Dhara WL on the other.

Plan:

- Keep **Dhara support as-is** (optional via Kconfig) for whole-device / second-flash storage.
- Add a **separate UBI (or other FTL) BDL layer** that integrates with the existing Raw BDL path for same-die “raw partitions + managed storage” layouts.
- Users who bring another FTL only depend on Raw BDL.

### 5. Splitting WL into another component — revisit later

WL and the NAND driver are planned to be **logically independent** (Dhara optional via Kconfig). Physically splitting WL into a separate registry component is **optional** and can wait until Raw-only consumers or packaging needs make it necessary. **No strong need to split right now**; revisit when optional-WL and alternate-FTL paths are stable.

### 6. Alignment with `spi_flash`

The `spi_nand_flash` structure is similar in spirit to the IDF `spi_flash` component (host/chip/OS layering). Some headers will need extension (e.g. **shared or NAND-specific error codes**). Exact error-code mapping needs a focused follow-up analysis.

### 7. `esp_mspi` / MSPI in IDF

IDF’s MSPI-related support provides shared low-level handling for flash and PSRAM on the MSPI peripheral. How `spi_nand_flash`’s MSPI transport should reuse that (vs flash-host / `common_command` paths) is **not fully decided yet** and must be checked against current IDF APIs. Reuse is expected to be possible; details are TBD.

## Expected outputs from the component

After the work above, `spi_nand_flash` should be able to provide:

1. **Raw NAND BDL handle(s)** that can operate over **MSPI and/or GPSPI**
2. **WL BDL handle** (Dhara), when enabled
3. **Bring-up / init APIs** appropriate to each transport (system/primary MSPI path vs application GPSPI path)
4. A clean base for **alternate FTL BDLs** (e.g. UBI) on top of Raw BDL

## Deferred / later topics

Flash encryption, OTA, and related product features are **out of scope for the first integration**. Once stable **BDL handles** exist for NAND, those can be designed on top (partition agents, encryption hooks, OTA flows) without forking the NAND protocol stack again.

## Bottom line

Publish and extend **`spi_nand_flash` from the Component Registry** as the shared NAND driver: add GPSPI/MSPI transport, expose Raw + optional WL (and later UBI) via BDL, support primary and external NAND independently, and keep IDF focused on boot/partition/OTA policy rather than duplicating NAND chip support.
