# NodeMCU Lightning Detector

A Wemos D1 mini (ESP8266) with an AS3935 lightning sensor. It logs every
strike with its distance, and serves a dashboard on your network with:

- a status banner ("All clear", "Lightning 14 km away", "Within 10 km") and
  storm trend (approaching / moving away)
- a **map**: drop a pin where the sensor is, and each strike distance is drawn
  as a ring around it, coloured by age (the AS3935 measures distance, not
  direction)
- a distance-over-time chart, the strike log with CSV download
- sensor settings, antenna auto-tuning, and a "Simulate strike" button for
  testing without a storm

First-time setup works like a smart plug: the detector raises its own Wi-Fi
network with a setup page where you pick your home Wi-Fi.

![Wiring diagram](LightningDetector/wiring_diagram.svg)

---

## Hardware

### Parts

| Part | Notes |
|---|---|
| **Wemos / LOLIN D1 mini** (ESP8266, 4 MB flash) | Clones work. The ESP8266 is a 3.3 V part: its pins are not 5 V tolerant. |
| **AS3935 breakout** | One of the boards in [Supported breakouts](#supported-breakouts). |
| **Wires** | Jumper wires long enough to put **15–20 cm** between the sensor and the D1 mini (see [Placement and noise](#placement-and-noise)). |
| **USB power supply**, 5 V, 500 mA or more | Quality matters: cheap chargers are the most common noise source. A computer's USB port or a power bank is a good test supply. |
| *Optional:* 10 µF and 100 nF capacitors | Power filtering at the sensor. |
| *Optional:* clip-on ferrite for the USB cable | Cuts noise carried along the cable. |
| *Optional:* two 4.7 kΩ resistors | I2C pull-ups for long or flaky wiring. |
| *Optional:* plastic enclosure | **Not metal**: metal blocks the lightning signal too. |

### Supported breakouts

| Board | Interface | How to wire |
|---|---|---|
| **CJMCU-3935** (purple, ~11 pins) | I2C | [I2C wiring](#wiring-i2c-default) with the configuration pins strapped as listed. Some batches swap the MOSI/MISO labels. |
| **DFRobot Gravity SEN0290** | I2C | Only five wires: VCC→3V3, GND→G, SCL→D1, SDA→D2, IRQ→D5. Power it from **3V3, not 5 V**: its I2C signals run at its supply voltage. Leave the address switch at 0x03 (0x01 and 0x02 are found too). |
| **SparkFun SEN-15441** (v2) | SPI | [SPI wiring](#wiring-spi). Set up for SPI from the factory. |
| Other AS3935 boards | either | I2C if the board breaks out SI, A0, A1; otherwise SPI. |

### D1 mini pins used

| D1 mini pin | GPIO | I2C build (default) | SPI build |
|---|---|---|---|
| D1 | 5 | SCL | IRQ |
| D2 | 4 | SDA (the AS3935's MOSI pad) | CS |
| D5 | 14 | IRQ | SCK |
| D6 | 12 | — | MISO |
| D7 | 13 | — | MOSI |
| D4 | 2 | on-board blue LED | on-board blue LED |
| 3V3 | — | sensor power and straps | sensor power |
| G | — | ground | ground |

**Do not move IRQ to D3, D4 or D8.** Those pins decide how the D1 mini boots
(D3 and D4 must be high at power-up, D8 low). The AS3935 holds IRQ low while
idle, so on D3 or D4 the board would not start. D0 cannot take interrupts.
TX and RX carry the serial monitor.

### Wiring: I2C (default)

| AS3935 pin | Connect to | What it does |
|---|---|---|
| VCC | 3V3 | Power (3.3 V). |
| GND | G | Ground. |
| SCL | D1 | I2C clock. |
| **MOSI** | D2 | I2C data. On the AS3935 the I2C data line is the MOSI pad; it may be labelled SDA. |
| IRQ | D5 | Interrupt: the sensor raises it on each event. It also carries the antenna signal during tuning, so tuning fails without it. |
| SI | 3V3 | Selects I2C (GND would select SPI). |
| A0 | 3V3 | Address bit 0. |
| A1 | 3V3 | Address bit 1. |
| EN_V *(if present)* | 3V3 | Turns on the chip's internal voltage regulator. Some boards hard-wire it and have no pad. |
| CS | G | Unused in I2C mode, but it must not float. |
| MISO | not connected | |

**I2C address.** A0 and A1 set the address. The firmware tries 0x03 first,
then 0x01 and 0x02, so any combination except both on GND works:

| A1 | A0 | Address |
|---|---|---|
| 3V3 | 3V3 | 0x03 (default) |
| 3V3 | G | 0x02 |
| G | 3V3 | 0x01 |
| G | G | 0x00, not usable (reserved on every I2C bus) |

**Pull-ups.** The bus runs at 80 kHz. A 100 kHz clock's 5th harmonic would
land exactly on the 500 kHz the antenna listens on. Up to about 30 cm of
jumper wire works without extra parts. If the sensor is not found or drops
out, add 4.7 kΩ from SCL to 3V3 and from the data line to 3V3.

### Wiring: SPI

For SPI-only boards such as the SparkFun v2. In
[`config.h`](LightningDetector/config.h), uncomment
`#define AS3935_USE_SPI`, then flash.

| AS3935 pin | Connect to |
|---|---|
| VCC / 3V3 | 3V3 |
| GND | G |
| SCK | D5 |
| MISO | D6 |
| MOSI | D7 |
| CS | D2 |
| IRQ (INT on SparkFun) | D1 |
| SI *(if broken out)* | G |

The SPI clock is 2 MHz, well away from 500 kHz.

### Power

- The D1 mini runs from its USB port, and its regulator makes the 3.3 V
  supply. Power the sensor from **3V3**, never 5V.
- The AS3935 draws well under 1 mA. The D1 mini draws about 70–80 mA
  steadily, because the firmware keeps the Wi-Fi radio awake. It peaks near
  200 mA while transmitting. Any 500 mA USB supply has plenty of headroom.
- A 10 µF capacitor plus a 100 nF capacitor across VCC and GND, right at the
  sensor's pins, cleans up its supply. This also guards against the brown-out
  resets the firmware has to recover from (see
  [Troubleshooting](#hardware-troubleshooting)).

### Placement and noise

The AS3935 listens for lightning's radio signal at around 500 kHz.
Switching power supplies, displays and the D1 mini itself make noise in the
same range. Noise shows up two ways: as **disturbers** (false events that also
mute the sensor for 1.5 s each) and as a higher **noise floor** (less range).
Placement fixes more than any setting.

1. **Distance first.** Put the sensor 15–20 cm from the D1 mini and its USB
   supply. At 5 cm it still works, but with less range and more disturbers.
2. **Keep it away from** chargers, power bricks, monitors and TVs, LED
   drivers and dimmers, solar inverters, motors, and phones.
3. **Use a clean supply.** Try a computer's USB port or a power bank. If
   disturbers drop, your charger was the problem. A clip-on ferrite on the USB
   cable, right at the D1 mini, also helps.
4. **Route the wires sensibly.** Keep the sensor wires away from the USB
   cable. For runs over about 30 cm, twist SCL and the data line each with a
   ground wire.
5. **Filter the sensor's power** with the capacitors above. For more, put a
   ferrite bead in series with the VCC wire at the sensor end.
6. **Rotate the sensor board.** How much noise the small coil antenna picks up
   depends on its angle to the source. Turn the board and watch the disturber
   count.
7. **Plastic enclosure, never metal.** Indoors is fine; lightning's signal
   passes through ordinary walls. Near a window is best. Stay clear of metal
   siding, foil-backed insulation and large metal objects.
8. **Indoor vs outdoor.** The **Placement** setting on the dashboard switches
   the chip's amplifier gain. Use Indoor unless the sensor is outside: Outdoor
   is much less sensitive.

**Check how noisy it is.** Leave it running for an hour on a calm day, then
look at the dashboard:

- **Disturbers:** a few an hour is fine; dozens means fix something. "near
  Wi-Fi" counts the ones that came within half a second of the D1 mini
  answering a web request. With the dashboard open, about 1 in 10 land there
  by chance. If most do, the D1 mini's own Wi-Fi is the source: add distance.
- **Noise floor:** with Auto noise on, the floor climbs while the chip
  reports noise. Indoors, 0–2 is quiet, 3–4 is so-so, and 5–7 is noisy.

The firmware keeps its own radio as quiet as it can: radio always awake,
transmit power trimmed to what your signal needs, and little traffic when
nobody is watching. The **Wi-Fi power** and **Radio** settings are in the
dashboard's Device panel.

The dashboard's sensor settings can filter what is left: **Watchdog**,
**Spike rejection** and **Min. strikes**. These trade away some range, so fix
placement first.

### Status LED

The D1 mini's blue LED (D4):

| Pattern | Meaning |
|---|---|
| Short blink once a second | Setup mode: join **LightningDetector-Setup** |
| Off | Running normally |
| Two short blinks every 2 seconds | Can't reach your Wi-Fi; it keeps retrying |
| Quarter-second flash | Strike detected (simulated strikes too) |

### Antenna tuning

The sensor's antenna has to resonate at 500 kHz, within ±3.5%. The chip has
16 built-in capacitor steps (0–120 pF) to trim it. On first boot the firmware
tries every step, picks the closest and saves it (about 9 seconds). The
**Antenna** panel shows the result and can run it again. You might re-run it
after moving the sensor next to large metal.

If no step comes within 3.5%, the board's own antenna capacitors are off.
Some CJMCU-3935 batches ship with the wrong values; the two antenna
capacitors should be 680 pF and 270 pF. The detector still works, with less
range.
Swapping the capacitors fixes it if you are comfortable with small
surface-mount parts.

### Testing the hardware

- **Serial monitor at 115200 baud.** Boot messages show whether the sensor
  answered, for example `[AS3935] I2C 0x03 regs ...`,
  `[AS3935] tuned: cap 6 -> 501234 Hz`, or
  `[AS3935] no answer on I2C 0x01-0x03 - check wiring`.
- **Piezo BBQ or gas lighter:** click it a few centimetres from the sensor. It
  should register disturbers, and sometimes a strike. This tests the whole
  chain without a storm.
- **Simulate strike** on the dashboard tests only the software (log, map,
  chart, LED). It does not touch the sensor.

### Hardware troubleshooting

| Symptom | Likely cause and fix |
|---|---|
| Board won't boot or keeps restarting after wiring | IRQ on D3, D4 or D8, or a short between 3V3 and G. Use D5 for IRQ. |
| "Sensor not found" banner / `no answer on I2C` | SI not on 3V3. SCL and data swapped. Label swap on some CJMCU boards: try the **MISO** pad on D2. CS floating. A0 and A1 both on G. Long wires without pull-ups. |
| "Tuning failed: no signal on IRQ" | IRQ wire missing or on the wrong pin. |
| Tuning finishes but is outside ±3.5% | Board's antenna capacitors are off (see [Antenna tuning](#antenna-tuning)). It still works, with less range. |
| Dozens of disturbers an hour, or "noise too high" | Placement or power supply: see [Placement and noise](#placement-and-noise). |
| Serial shows `registers lost ... re-init` now and then | The sensor's supply dips and the chip resets. The firmware recovers by itself; add the 10 µF + 100 nF capacitors at the sensor or try a better supply. |
| Never detects anything | Normal without a storm in range (about 40 km). Try the lighter test. Check that Placement is Indoor (unless the sensor is outside) and that the noise floor is not stuck high. |

---

## Flash

1. Arduino IDE with the ESP8266 core (Boards Manager: "esp8266", 3.1.2 tested).
2. Tools → Board → **LOLIN(WEMOS) D1 R2 & mini**.
3. Tools → Flash Size → **4MB (FS:2MB OTA:~1019KB)** (needed for the log).
4. Open `LightningDetector/LightningDetector.ino` and upload. No extra
   libraries are needed.

Later updates go over Wi-Fi: Sketch → Export Compiled Binary, then on the
dashboard's Device panel click **Update firmware** and pick the `.bin`. It
asks for the device password; the page reloads itself when the detector is
back.

## Set up

1. Power it up. The blue LED blinks once a second.
2. On your phone, join the Wi-Fi network **LightningDetector-Setup**. The setup
   page opens (or browse to `http://192.168.4.1`). On iPhone, turn off
   Cellular Data if it doesn't load.
3. Pick your network, enter its password, choose a **device password**
   (8+ characters), and tap **Connect**. If the detector can't join, the
   setup network comes back within a minute with an error.
4. Back on your home Wi-Fi, open **http://lightning.local** (or the IP your
   router shows for "lightning").
5. Click **Place sensor**, then click the map where the detector is, or search
   your address. Drag the pin to the exact spot and click **Save location**.

On first boot the detector tunes its antenna by itself (about 9 seconds).

## Security

- **Viewing is open** to anyone on your network. **Changing anything**
  (settings, location, tuning, clearing the log, Reset Wi-Fi, firmware
  updates) needs the device password. Click the lock in the header to
  unlock a browser or lock it again.
- The device only answers to its own address (`lightning.local`, its IP,
  or `lightning.<your router's local domain>`), so other websites can't
  read it through your browser. Set `HOST_CHECK 0` in `config.h` if you
  reach it under another name.
- The setup network only comes up on first boot, after **Reset Wi-Fi**,
  or when you **press the reset button twice within 5 seconds**. A power
  cut never brings it up. It is open by default: set `AP_PASS` in
  `config.h` before flashing to encrypt it.
- **Forgot the device password, or changed your router?** Press reset
  twice, join **LightningDetector-Setup**, and set a new one.
- Arduino IDE network upload is off unless you set `OTA_PASSWORD` in
  `config.h`.
- Don't forward router ports to the detector: it uses plain HTTP.

## Notes

- This is a hobby sensor, not a safety device.
- The map and address search need internet access; the log, chart and
  settings work without it.
- Distances are the chip's estimate to the leading edge of the storm, in
  steps from overhead to 40 km. There is no direction.
- Firmware internals, the HTTP API and design notes are in
  [`HANDOFF.md`](LightningDetector/HANDOFF.md).
