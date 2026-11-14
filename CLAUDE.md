# CLAUDE.md – arduino_i2c_orja

Danfoss/Thermia heat pump i2c interface for Arduino Nano / Pro mini (ATmega328P, 5V 16MHz). It acts as an **i2c slave (0x2e)** on the pump's bus and exposes the pump's registers over serial using ThermIQ-USB style **AT commands** (works with taloLogger, ThermIQ, ThermIQ2-Web, minicom). GPL-3.

Sibling project: `../esp32_i2c_orja` (same protocol core, adds WiFi/MQTT, OLED and ROOM2 temperature emulation). Keep the protocol logic in the two repos consistent when changing it.

## Layout
- `i2c_rtc_simplified_with_watchdog/i2c_rtc_simplified_with_watchdog.ino` – the whole firmware (~336 lines, single file).
- `libraries/` – vendored NeoSWSerial and AltSoftSerial (needed for the extra serial ports).
- `Dockerfile`, `linux_build.sh` – Docker based build, outputs `.hex` files into `hex/`.
- `hardware/` – gerber zip (PCB v1.3) and STL screw-mount. `images/` – README pictures.
- `README.md` – user documentation (hardware, software alternatives, build, BOM). Don't duplicate it here.

## Build / flash / test
- Build: `./linux_build.sh` (Docker) or Arduino IDE (needs Wire, NeoSWSerial, AltSoftSerial).
- Flash: `avrdude -v -p atmega328p -c arduino -P /dev/ttyUSB0 -b 57600 -D -U flash:w:<file>.hex:i` (unplug the 220uF reset-prevention capacitor while flashing).
- No automated tests. Verify on hardware / serial: `ATI` → `ThermIQ2 v.4.30 0011`, `ATR0010` reads register 0x10, `ATP` shows the A6 read-only pin value.

## Architecture
Everything real happens in `loop()`; the Wire callbacks only record state.

1. Pump (i2c master) writes `receiveEvent`: first byte = command, rest = args → stored in `requestedCmd` / `i2cArgs`.
   - `0xFD` PING_BOOT, `0xFE` PING_QUERY: "do you want anything?"
   - `0x00–0x7F`: pump pushes the value of that register (`i2cArgs[0]`=low byte, `[1]`=high byte; value is signed 16 bit).
2. On a ping `loop()` builds `i2cResponse` that `requestEvent` sends back:
   - `[reg]` – first register with `readQueue[reg] > 0` (reads have priority),
   - else `[writeAddress|0x80, low, high]` if an `ATW` is pending,
   - else `[0xFF]` = nothing needed; then "OK" is printed to whoever is waiting (`okPending`).
3. When the pump pushes the requested register, it is printed as `RR=HHLL` (hex) to every port whose bit is set in `readQueue[reg]`, then cleared.
4. `handle_serial()` is called for each port: line-buffered (max 25 chars), local echo, upper-cased, AT command parsing.

`readQueue[128]` bitmask: `1` = hw `Serial`, `2` = `serial2` (NeoSWSerial D3 RX / D4 TX), `4` = `serial3` (AltSoftSerial, D8/D9). `okPending` uses the same bits.

AT commands: `ATI`, `ATR<xx>[<yy>[<aabb>]]` (hex register / range / second range, max 0x7F else `NO`), `ATW<xx><hhhh>...` (`atwxxhhll`, write hhll to register xx), `ATB` (unsupported), `ATP` (protect pin state), `ATL?`, `ATTG`/`ATTS` (`NO`).

## Pins and settings
- i2c: A4/A5 (Wire), slave address 0x2e, 8 s watchdog (`wdt_enable(WDTO_8S)`).
- D5/D6/D7 (INPUT_PULLUP) select baud rates at boot: Serial 9600 (high) / 115200 (low); serial2 9600 / 38400; serial3 115200 (high) / 9600 (low). Note serial3's mapping is inverted compared to the others.
- A6: read-only ("RO") pin – `ATW` is accepted only when `analogRead(A6) > 100`.

## Gotchas
- Safety: this writes to heat pump hardware. Don't make writes easier/default-on; keep the RO pin check.
- `ATW` does no length/format validation beyond `pituus > 6`; `getVal()` assumes upper-case hex (input is upper-cased first).
- `okPending` accumulates (`|=`) who waits for "OK"; it is printed only when the read queue is empty.
- Only one write can be pending at a time; a new `ATW` overwrites an unsent one.
- Code style: single sketch, Arduino C++, Finnish variable names appear (`pituus` = length). Keep changes minimal; don't refactor unasked.
