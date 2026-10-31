# MK3S firmware 3.14.1 with 12-filament MMU support

This tree is Prusa printer firmware **v3.14.1** (`74a577bc0`) with a small MK3S/MK3S+ patch for a 12-filament MMU. The printer still has one extruder. `EXTRUDERS` stays 1. Slot count is the variant setting `MMU_FILAMENT_COUNT`.

## Versions that belong together

| Piece | Version |
| --- | --- |
| Printer | Original Prusa i3 MK3S or MK3S+ |
| Printer firmware base | [Prusa-Firmware v3.14.1](https://github.com/prusa3d/Prusa-Firmware/releases/tag/v3.14.1) |
| This branch | `feature/mmu12x` |
| MMU protocol | **3.0.3**, the protocol used by printer firmware 3.14.1 |
| MMU controller firmware | [cjbaar/Prusa-Firmware-MMU-12x](https://github.com/cjbaar/Prusa-Firmware-MMU-12x), built for 12 filaments and still speaking protocol 3.0.3 |
| MMU hardware | [cjbaar/prusa-mmu-12x](https://github.com/cjbaar/prusa-mmu-12x) |

Do not move this printer to MMU firmware 3.0.4. Firmware 3.14.1 does not speak that protocol.

The MMU controller firmware in this repository was not modified. The printer already sends the slot as one `uint8_t` in hexadecimal (`Protocol::UInt8ToHex`). Tools 0–9 go out as `0`–`9`. Tools 10 and 11 go out as `A` and `B`, so g-code `T10` becomes a `TA` request and `T11` becomes `TB`. The transmit buffer (`MaxRequestSize()`, 13 bytes) already fits that. A stock 5-slot MMU 3.0.3 build will answer `INVALID_TOOL` for `T5` and above. The 12-filament controller firmware has to accept slots 0–11.

## Configuration

`Firmware/variants/MK3S.h`:

```cpp
#define MMU_FILAMENT_COUNT 12
```

That is the only slot-count change. MK3, MK2.5, and the Revo variants are still 5. Internal tool indices are 0–11. The LCD shows filaments 1–12.

Setting `MMU_FILAMENT_COUNT` back to 5 and rebuilding restores the stock limits. Menus, T-codes, load-all, and Spool Join all read that constant. With a count of 5, `T0`–`T4` are accepted and `T5`–`T11` are rejected.

## What changed

- **T-codes.** With the MMU enabled, `T0`–`T11` are forwarded as tool indices 0–11. `T12` and above, `T100`, leading zeros (`T01`), and trailing junk (`T10x`, `T1.1`) are rejected. `Tx`, `T?`, and `Tc` keep their old meaning, including case. With the MMU disabled, the old first-character gate remains: `0`–`4`, `x`, `c`, and `?` are recognized, and `T5`–`T9` are still "Invalid T code."
- **LCD filament menus.** Preload, load-to-nozzle, eject, cut, and loading-test list every configured slot. Labels use a shared formatter, so filament 10 is `10` rather than the character after `'9'`.
- **Filament picker.** `choose_menu_P()` sizes itself from `MMU_FILAMENT_COUNT` when the MMU is on, and keeps 4 items (5 with Cancel) when it is off. The three-row window scrolls through all 12 entries, including the last one. First-layer calibration, `Tx`/`T?`, M600, and the IR self-test use this picker.
- **Status line.** The filament field stays inside its 5 columns. `F1` and `1>2` look as before. `F10` and `9>10` fit. `10>11` fills the field and drops the leading space so it does not cover the clock. On the MMU error row the same 5 columns end before the thermometer.
- **Long messages.** `lcd_print()` already prints two-digit numbers. "Unloading filament" is 18 characters, so a two-digit slot is drawn on the next row instead of wrapping off the 20-column display. Shorter messages such as "Loading filament 12" stay on one line.
- **Spool Join.** After slot 11 the next slot is 0. A count of 5 still wraps after slot 4.
- **Load all / test all.** Both loops use `MMU_FILAMENT_COUNT`.
- **M701 / M704 / M705 / M706.** They already refused a slot `>= MMU_FILAMENT_COUNT`, so `P10` and `P11` work and `P12` does not.

Left alone on purpose: `EXTRUDERS`, thermal protection, filament-sensor checks, PINDA calibration tables, steel-sheet names, and every other constant that happens to be 4 or 5.

## Build

The firmware is the MK3S variant. English-only is the build that was compiled and sized here. It is a flashable `.hex`. It does not carry the secondary language pack from the multi-language image.

From the repository root, after `python utils/bootstrap.py` (CMake, Ninja, avr-gcc 7.3.0, and the Prusa board package):

```text
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=cmake/AvrGcc.cmake -DFW_VARIANTS=MK3S
ninja -C build MK3S_ENGLISH
```

On Windows the bootstrapped tools live under `.dependencies/`. Point `CMAKE_MAKE_PROGRAM` at `ninja.exe` if CMake does not find it.

The English hex is written to:

```text
build/MK3S_MK3S+_FW_3.14.1_ENGLISH.hex
```

Host tests (no printer required):

```text
cmake -S . -B build-host -G Ninja
ninja -C build-host tests
build-host/tests/tests.exe
```

`build/`, `build-host/`, and `.dependencies/` are local outputs. Do not commit them.

## Size

Both figures are the MK3S English image for an ATmega2560. The baseline is unmodified v3.14.1. SRAM did not change. Program flash grew by 564 bytes and is still under the 256 KiB device limit.

| | Program | SRAM (data + bss + noinit) |
| --- | ---: | ---: |
| v3.14.1 MK3S English | 230742 bytes (88.0%) | 5665 bytes (69.2%) |
| This patch, MK3S English | 231306 bytes (88.2%) | 5665 bytes (69.2%) |

`avr-size` on the patched image: `text 231008`, `data 298`, `bss 5367`. Rebuilding from commit `335dd4a` produced the same figures.

## Tests

Host tests in `tests/Mmu12x_test.cpp` passed (364 assertions in the MMU cases, 395 in the full suite):

- `T0`–`T11` parse to tools 0–11, including `t11`, `T 10`, and a trailing space
- `T12`, `T13`, `T99`, `T100`, `T01`, `T00`, `T10x`, `T1.1`, `T-1` are rejected
- `Tx`, `T?`, and `Tc` keep their special meanings; `T?1` does not
- A count of 5 still accepts `T4` and rejects `T5` and `T11`
- The disabled-MMU gate still allows `0`–`4`, `x`, `c`, `?` and rejects `5`–`9`
- Labels are `1`…`12`, and unknown is `?`
- Every current/target pair among slots 0–11 and unknown fits in 5 status columns, including `10>11`
- Spool Join walks 0→11→0, and a 5-slot count still wraps after 4
- The filament menu can reach index 11 of 12, and index 12 when Cancel is present
- An 18-column message keeps a one-digit slot on the same row and moves a two-digit slot to the next row

The MK3S English firmware compiled with no reported warnings and linked.

## Not tested on a printer

Nothing was flashed. These still need a real MK3S and the 12-filament MMU:

- A tool change for every slot, including `T10` and `T11`, and a rejected `T12`
- Encoder scrolling and selection of filament 12 on the 20×4 LCD
- Spool Join wrapping during a print, including a failed load
- Load, unload, eject, cut, and the loading test on slots 6–12
- First-layer calibration filament selection and M600
- MMU error screens while a two-digit slot is active
- Behavior against a stock 5-slot MMU (this image will offer all 12 slots)
- The multi-language hex. One MMU error sentence now says `T0-T11` instead of `T0-T4`; existing translations of that sentence will not match until the language catalogs are regenerated

Thermal runaway, the filament sensor, and the rest of the safety checks were not removed. They were not exercised on hardware either.

## Rollback

1. In `Firmware/variants/MK3S.h`, set `MMU_FILAMENT_COUNT` back to `5`.
2. Rebuild `MK3S_ENGLISH` (or the multi-language target).
3. Flash that image, or flash the official [v3.14.1 MK3S hex](https://github.com/prusa3d/Prusa-Firmware/releases/tag/v3.14.1) if you want upstream bytes exactly.

The MMU board keeps whatever controller firmware you put on it. Rolling the printer back does not rewrite the MMU.
