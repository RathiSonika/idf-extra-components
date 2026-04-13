# NAND logical-volume UART dumper

Firmware example that reads the **wear-leveled logical block device** (`spi_nand_flash_init_with_layers`) from byte `0` through `geometry.disk_size - 1` and streams it over UART. The saved image is a **contiguous view of the volume** (Dhara FTL applied on the device); it does **not** include raw NAND OOB or a separate Dhara metadata stream suitable only for physical parse tools.

**Requirements:** ESP-IDF **6.0+** with `esp_blockdev`, and `CONFIG_NAND_FLASH_ENABLE_BDL=y` (set in `sdkconfig.defaults` here).

## Wiring

- **SPI NAND:** Same default pins as `spi_nand_flash/test_app` (VSPI on ESP32, SPI2 on other targets). Adjust in source if your board differs (`nand_volume_uart_dumper.c`).
- **Dump UART:** Defaults to **UART1**, TX **GPIO17**, RX **GPIO18**, **921600** baud (`main/Kconfig.projbuild`; use **115200** if the link is unreliable).
- **Same cable as flash / `idf.py monitor`:** Set **EXAMPLE_DUMP_UART_PORT_NUM** to **0** and **TX/RX** to your board’s console UART pins (ESP32: **GPIO1 TX**, **GPIO3 RX**). If that matches `CONFIG_ESP_CONSOLE_UART_NUM`, the app **uninstalls the console UART driver** before opening the dump port — **close monitor** and use the **same COM port** for `host_dump_nand_volume.py` as for flashing. Prefer **USB Serial/JTAG console** in menuconfig if you want UART0 for dump **and** a separate debug log stream.

## Build and flash

```bash
cd examples/nand_volume_uart_dumper
idf.py set-target esp32   # or esp32s3, etc.
idf.py build flash
```

## Capture an image on the host

1. Connect a USB–serial adapter **RX** to firmware **dump TX** (Kconfig `EXAMPLE_DUMP_UART_TX_PIN`), **GND** common.
2. Start the Python script **before** or **right after** reset so it can catch **`WAIT\n`** (sent within ~1 s of boot).
3. Run (use the **dump** UART, not the console UART unless you moved the dump there):

```bash
pip install pyserial
python3 host_dump_nand_volume.py -p /dev/ttyUSB1 -o fat_volume.bin
```

(`-b` defaults to **921600** to match Kconfig; pass `-b 115200` if needed.)

The script **waits for `WAIT\n`** (UART driver up; default **30 s**, `--wait-timeout`), then **`READY\n`** (NAND + WL inited; default **180 s**, `--ready-timeout`), then sends `DMP\n` and parses **`NDLV`**. If you never see `WAIT`, the adapter RX is not on the firmware **dump TX** pin, **GND** is missing, or **baud** does not match `EXAMPLE_DUMP_UART_BAUD`.

If you see **only `0x00` / `0x80`-like garbage**, you are almost certainly on the **wrong UART**, **wrong baud**, or a **floating RX**. Re-flash after changing Kconfig pins/baud (`idf.py fullclean` if the old baud sticks in `sdkconfig`).

The firmware **drains RX after the handshake**, uses a **large TX ring**, and **`uart_wait_tx_done` after the header** so `NDLV` is not overrun by the first page write. On NAND init failure it sends **`FAIL nand_init\n`** on the dump UART.

## Parsing with ESP-IDF `fatfsparse.py`

If the volume was formatted as FAT (same layout FatFs would see), try:

```bash
python3 "$IDF_PATH/components/fatfs/fatfsparse.py" fat_volume.bin
```

- **Sector size:** `fatfsparse.py` uses the FAT BPB in the image (`BytesPerSec`), not NAND page size. Typical ESP-IDF FAT uses **512** bytes per logical sector unless you formatted otherwise.
- **`--wl-layer`:** This flag applies to **ESP-IDF’s FAT wear-levelling (`wl_fatfsgen`)** wrapper in the image, **not** to Dhara. A logical dump from this example is usually a **plain FAT image**; start with default **detect**. If parsing fails and you know the image still contains ESP FAT WL sectors, try `--wl-layer enabled` or `disabled` explicitly.

## Consistency / safety

- For a **mountable** FAT image, avoid concurrent writes: do **not** mount FatFs on the same volume while dumping, or use read-only + sync and no other storage tasks.
- The wear-leveled `read` path uses `spi_nand_flash_read_page` internally; corrected-bit ECC handling may still trigger maintenance behavior in the driver on marginal pages—prefer a **dedicated dumper firmware** for forensic captures.

## Protocol reference

1. ASCII **`WAIT\n`** — dump UART is configured (sent **before** NAND init so you can verify wiring quickly).
2. ASCII **`READY\n`** — NAND + wear-level layer ready for handshake.
3. Host sends ASCII **`DMP\n`**.
4. Binary header — **one of:**
   - **Raw** (`NDLV`): `disk_size` u64 LE, `page_size` u32 LE (16-byte header), then **`disk_size`** contiguous bytes.
   - **Sparse** (`NDS1`, default `CONFIG_EXAMPLE_DUMP_SPARSE_FF`): `version` u32 LE (=1), `disk_size` u64 LE, `page_size` u32 LE (20-byte header), then a sequence of records covering the volume in order:
     - **`SKIP`**: magic 4, `offset` u64 LE, `length` u32 LE — logical range reads as all **0xFF** (not sent).
     - **`DATA`**: magic 4, `offset` u64 LE, `length` u32 LE, then **`length`** bytes.

The host script still builds a **full `disk_size` file** (SKIP regions filled with 0xFF) so `fatfsparse.py` works unchanged.

**Note:** “Sparse” here means **skip erased-looking (all-0xFF) chunks** on the wire. It is **not** the same as “only FAT-allocated clusters” (that would require walking the FAT in firmware or mounting the FS).
