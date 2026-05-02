# (WIP)RFC: Configurable OOB Layout for `spi_nand_flash` Component

## Motivation

SPI NAND flash devices provide an **Out-Of-Band (OOB) / spare area** per page.
This area is commonly used for:

- Bad Block Markers (BBM)
- ECC parity or ECC-related metadata
- Small metadata (FTL, or driver-level)

Vendor datasheets show that OOB is **not uniform**:
it is often divided into multiple sections with different properties:

- user-writable vs non-writable
- ECC-protected vs not ECC-protected
- contiguous vs interleaved

Hardcoding offsets or assuming a single free region is fragile and incorrect.

This RFC proposes a **layout-driven, Linux-MTD-inspired design** that:
- supports interleaved OOB layouts
- hides physical offsets from upper layers
- allows safe storage of driver-level metadata

---

## Problem Statement

- BBM location and size are not universal.
- ECC (especially internal ECC) may reserve part of OOB.
- We want to store small metadata (e.g., `page_used_marker`) safely in OOB.
- Vendor-specific OOB details may be unavailable initially.

---

## Design Goals

- Avoid hardcoded OOB offsets.
- Allow per-chip (and per-ECC-mode) OOB layout configuration.
- Work safely even without vendor-specific datasheets.
- Enable safe storage of small driver metadata in OOB.
- Allow future refinement without API breakage.

---

## Key Principle

> **By default, treat the OOB area as free except for bytes explicitly reserved.**

This allows safe initial operation and incremental refinement once more
information becomes available.

---

## Terminology

- **OOB (Out-Of-Band)**: Spare bytes associated with each NAND page.
- **BBM (Bad Block Marker)**: Bytes indicating factory-bad blocks.
- **Reserved region**: OOB bytes that must not be written by user logic.
- **Field**: A named, fixed-size metadata slot carved from free OOB.
---

## Important Notes for Reviewing the RFC:

* Layout structs describe reality
* Field specs describe intent
* Transfer context bridges the two
* Scatter/gather moves bytes safely

---

## Key Concepts Overview

* WL operate on *fields* and *logical OOB*.
* The driver maps these to *physical OOB regions*.
* WL layers only interact with **fields** and never see physical offsets or region boundaries.

The design is intentionally layered:

1. **Physical OOB Layout (hardware-facing)**
   - Defined per chip via `spi_nand_ooblayout_ops_t`
   - Exposes OOB as a set of *regions* via callbacks
   - Each region is described by `spi_nand_oob_region_desc_t`

2. **Logical Packed OOB Space (driver-internal)**
   - Free OOB bytes are exposed as *packed logical streams*
   - Separate logical streams exist for:
     - ECC-protected free bytes
     - Non–ECC-protected free bytes

3. **Fields (policy / semantics)**
   - Fields are named slices of the logical packed space
   - Identified by Field ID, not physical offsets

---

## Core Data Structures

### OOB Region Descriptor
A region descriptor *describes* one contiguous physical OOB region and its behavior:

- Physical offset and length
- Whether the region is user-programmable
- Whether data in the region is ECC-protected

Region descriptors are **metadata**, not stored in flash.

They are returned by layout callbacks to allow the driver to reason about
fragmented and interleaved OOB layouts.

```c
typedef struct {
    uint16_t offset;        // Physical byte offset within the OOB area
                            // This is chip-specific and never exposed to upper layers

    uint16_t length;        // Length of this contiguous region in bytes

    bool programmable;     // Indicates whether software is allowed to write user data
                            // into this region. ECC parity and vendor-reserved regions
                            // must set this to false.

    bool ecc_protected;    // Indicates whether bytes written into this region are
                            // covered by the NAND device’s internal ECC engine.
                            // This property is used to separate logical OOB streams.
} spi_nand_oob_region_desc_t;
```

---

### OOB Layout Operations

The layout ops provide an *enumeration interface* over OOB regions.

Typical callbacks:

- `free(chip, section, &desc)` – enumerate user-writable regions
- `ecc(chip, section, &desc)` – enumerate ECC parity regions (usually not writable)

The `section` parameter is a simple index:
- The caller starts with `section = 0`
- The driver returns one region per call
- `-ERANGE` indicates no more regions

Sections are **not stored in NAND** and **not visible to applications**.

```c
typedef struct {
    int (*free)(const void *chip, int section,
                spi_nand_oob_region_desc_t *out);
    // Enumerates user-writable OOB regions.
    // 'section' is an index starting at 0.
    // Returns 0 on success, -ERANGE when no more regions exist.
    // Must NOT return BBM or ECC parity regions.

    int (*ecc)(const void *chip, int section,
               spi_nand_oob_region_desc_t *out);
    // Enumerates ECC parity regions (Internal ECC).
    // These regions are typically not programmable by software.
    // Mainly used for validation, debugging, or layout inspection.
} spi_nand_ooblayout_ops_t;
```

---

### OOB Layout Descriptor

```c
typedef enum {
    SPI_NAND_BBM_CHECK_FIRST_PAGE = 1 << 0,
    SPI_NAND_BBM_CHECK_LAST_PAGE  = 1 << 1,
} spi_nand_bbm_check_t;

typedef struct {
    struct {
        uint8_t offset;     // Byte offset of the Bad Block Marker within OOB
        uint8_t length;     // Length of the BBM field in bytes

        uint8_t good_value; // Value indicating a good block (typically 0xFF)
                            // Any other value indicates a bad block

        uint8_t check_pages_mask; 
        // Bitmask of pages to check for BBM (e.g. first page, last page, or both)
        // Uses spi_nand_bbm_check_t values
        // This specification is mentioned in ONFI Specification
    } bbm;

    uint8_t oob_size;      
    // Total OOB size per page in bytes.
    // uint8_t is sufficient (e.g., 256 bytes for 8KB page NAND).

    const spi_nand_ooblayout_ops_t *ops;
    // Pointer to layout callbacks describing this chip’s OOB region structure
} spi_nand_oob_layout_t;

typedef struct {
    //...
    spi_nand_oob_layout_t oob_layout;
    //...
} spi_nand_chip_t;
```

---

## Bad Block Marker (BBM)

- Defined in `spi_nand_oob_layout_t`
- Never exposed as free space
- Written only when explicitly marking a block bad

---

## Logical Packed OOB Space

Free bytes are split into two *logical packed streams* based on ECC protection:

- **FREE_NOECC**: user-programmable bytes *not* ECC protected
- **FREE_ECC**: user-programmable bytes that *are* ECC protected

```c
typedef enum {
    SPI_NAND_OOB_CLASS_FREE_NOECC, // User-writable bytes not protected by ECC
    SPI_NAND_OOB_CLASS_FREE_ECC,   // User-writable bytes protected by ECC
} spi_nand_oob_class_t;
```

---

## Field Abstraction

Fields represent *logical metadata*, such as:

- page-used marker
- sequence number
- checksum

A field specifies:
- Field ID
- Length
- Whether ECC protection is required

Fields **do not specify physical offsets**.

```c
typedef enum {
    SPI_NAND_OOB_FIELD_PAGE_USED = 0, 
    // Indicates whether a page is used/valid (typically written by WL)

    // future: SPI_NAND_OOB_FIELD_SEQNO,
    // future: SPI_NAND_OOB_FIELD_CRC,

    SPI_NAND_OOB_FIELD_MAX
} spi_nand_oob_field_id_t;

typedef struct {
    spi_nand_oob_field_id_t id; 
    // Unique identifier for the field

    uint8_t length;        
    // Size of the field in bytes

    spi_nand_oob_class_t cls; 
    // Specifies whether this field must be placed in ECC-protected
    // or non-ECC-protected logical OOB space

    uint8_t logical_offset; 
    // Logical offset within the packed OOB stream.
    // Assigned by the driver during initialization.

    bool assigned;         
    // Indicates whether the driver has successfully assigned
    // a logical offset for this field
} spi_nand_oob_field_spec_t;

//e.g
spi_nand_oob_field_spec_t field_page_used = {
    .id = SPI_NAND_OOB_FIELD_PAGE_USED,
    .length = 2,
    .cls = SPI_NAND_OOB_CLASS_FREE_ECC
};
//This Field table will be maintained by WL layer
spi_nand_oob_field_spec_t fields[SPI_NAND_OOB_FIELD_MAX] = {
    field_page_used, //...
};
```

At initialization time, the driver assigns each field (fields[]) a
**logical offset within the packed free OOB space**.

---

## Cached Transfer Context

```c
typedef struct {
    const spi_nand_oob_layout_t *layout;
    // Pointer to the active OOB layout describing this chip

    spi_nand_oob_class_t cls;
    // Logical OOB class (ECC or non-ECC) this context operates on

    uint8_t *oob_raw;
    // Pointer to the raw physical OOB buffer being read or composed

    uint16_t oob_size;
    // Size of the raw OOB buffer in bytes

    spi_nand_oob_region_desc_t regs[MAX_REG];    
    // Discuss with Martin to determine a generic MAX_REG value based on multiple datasheets
    // Cached list of physical OOB regions matching 'cls'

    uint8_t reg_count;
    // Number of valid entries in regs[]

    size_t total_len;
    // Total logical size (sum of all cached region lengths),
    // used for bounds checking during scatter/gather
} spi_nand_oob_xfer_ctx_t;
```

Purpose of defining spi_nand_oob_xfer_ctx_init():
Initializes a transfer context by:
- iterating layout .free() callbacks once
- filtering regions by ECC class and programmability
- caching the resulting region list
This function performs no NAND I/O.

Init once:
```c
int spi_nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                               const spi_nand_chip_t *chip,
                               spi_nand_oob_class_t cls,
                               uint8_t *oob_raw,
                               uint16_t oob_size);
```

Then scatter/gather use ctx->regs directly (faster, simpler internals) instead of iterating through free regions during each scatter/gather API call.

---

## Scatter / Gather APIs

### scatter (write path)

Purpose:
Copies data from a logical packed OOB offset into the correct
physical OOB locations inside ctx->oob_raw[].

- Input: logical offset + data
- Output: modified physical `oob_raw[]` buffer inside `spi_nand_oob_xfer_ctx_t`

```c
//spi_nand_oob_unpack
int spi_nand_oob_scatter(const spi_nand_oob_xfer_ctx_t *ctx,
                         size_t logical_off,
                         const void *src,
                         size_t len);
```

### Gather (read path)

Purpose:
Extracts data from physical OOB locations into a logical field buffer.

- Input: physical `oob_raw[]` buffer (`spi_nand_oob_xfer_ctx_t`)
- Output: logical field dst buffer

```c
// spi_nand_oob_pack
int spi_nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                        size_t logical_off,
                        void *dst,
                        size_t len);
```
Scatter/gather hides physical fragmentation and interleaving.

---

### Named Field Read/Write (built on packed free-OOB)

Wrappers built on top of the scatter/gather APIs to read and write OOB fields.

```c
esp_err_t spi_nand_oob_field_read(uint32_t page,
                                 spi_nand_oob_field_id_t id,
                                 void *out, size_t out_len);

esp_err_t spi_nand_oob_field_write(uint32_t page,
                                  spi_nand_oob_field_id_t id,
                                  const void *data, size_t data_len);
```
---

## Raw OOB Read/Write (debug)

Purpose:
Low-level, debug-oriented access to raw OOB bytes.
These APIs bypass field abstractions and should be used for debugging.

```c
esp_err_t spi_nand_oob_read_raw(uint32_t page, uint8_t *oob, size_t oob_len);
esp_err_t spi_nand_oob_write_raw(uint32_t page, const uint8_t *oob, size_t oob_len);
```
---

## Summary

- Layout describes OOB behavior
- Callbacks enumerate regions
- Scatter/gather hides fragmentation
- Fields provide semantic metadata
- Single program operation per page
