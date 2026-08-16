# Hardware

## Board — Wemos D1 R1

ESP8266 with a CH340G USB-serial bridge, micro-USB, Uno form factor, 4 MB flash.
PlatformIO board id `d1`.

**The R1 is not the D1 mini.** Its pin map differs, and building with `board = d1_mini`
succeeds, uploads, and then misbehaves silently because every `Dn` constant points at a
different GPIO. The hello-world sketch prints a `pinmap` line specifically to catch this.

```
D0=GPIO3   D1=GPIO1   D2=GPIO16  D3=GPIO5   D4=GPIO4   D5=GPIO14
D6=GPIO12  D7=GPIO13  D8=GPIO0   D9=GPIO2   D10=GPIO15
D11=GPIO13 D12=GPIO12 D13=GPIO14
```

Aliases to be aware of: D7 = D11 = GPIO13, D5 = D13 = GPIO14, D6 = D12 = GPIO12.

| Function | GPIO | R1 pad |
|---|---|---|
| SPI SCK | 14 | D13 (also D5) |
| SPI MISO | 12 | D12 (also D6) |
| SPI MOSI | 13 | D11 (also D7) |
| Onboard LED (active low) | 2 | D9 |
| Second board LED (active high) — do not use, it is SCK | 14 | — |

Boot-strap pins: **GPIO15 must be low at reset** (held high, the board will not boot at
all — no serial, no blink, looks bricked) and GPIO0/GPIO2 must be high. Keep radio control
lines off all three.

## Radio — NRF24L01+ (PA+LNA)

Wire by **GPIO number**, never by silkscreen label:

| NRF24 | GPIO | R1 pad | Note |
|---|---|---|---|
| CE | 4 | D4 | Plain GPIO strobe, outside SPI. No register test can detect it miswired |
| CSN | 5 | D3 | Chosen over the common default of GPIO15, which is a strap pin |
| SCK | 14 | D13 | Hardware SPI, not reassignable |
| MOSI | 13 | D11 | Hardware SPI |
| MISO | 12 | D12 | Hardware SPI |
| VCC | — | 3V3 | **3.3 V only.** 5 V destroys the module |
| GND | — | GND | |
| IRQ | — | — | Unused |

Keep the dupont leads short, 10–20 cm. Long leads plus a PA+LNA module is a classic source
of flaky SPI.

### Power is the number one failure

The PA+LNA variant peaks around 115 mA on transmit and 45 mA on receive, which an ESP
board's onboard regulator handles badly. Solder a **10–100 µF electrolytic plus a 100 nF
ceramic directly across the module's VCC/GND**, as close to the module as possible. This
is prescribed by both the RF24 maintainers and the milight-hub author, not folklore.

A module that behaves better when you touch it is a power-stability fault, verbatim from
the RF24 docs.

**Confirmed here, and the symptom is deeply misleading.** Without those capacitors this
build received perfectly — thousands of packets, correct CRCs, clean decodes — while
transmitting nothing a receiver five metres away would act on. Receive draws a fraction of
the current, so it never provokes the fault. Everything that can be checked in software
looks healthy: the chip asserts TX_DS on every send, so the radio genuinely believes it
transmitted.

Adding the two capacitors was the entire fix. If transmit does not work and receive does,
check the decoupling before suspecting the protocol, the wiring or the code — all three
will pass their own tests while the light stays dark. Note also that `setPALevel()` does
**not** meaningfully reduce the current spike on a PA+LNA module: it controls the nRF24
die's output, while the external amplifier draws its own current regardless. Lowering
power is not a workaround for missing capacitance.

Screw the antenna on before powering up. Transmitting without one can damage the PA stage.

## Bring-up order

1. Blink and serial, with no radio attached at all.
2. Radio self-test as a standalone sketch: `begin()`, `isChipConnected()`, `isPVariant()`,
   write a channel and read it back. A dump of all `0x00` means MISO stuck low or no
   power; all `0xFF` means MISO floating. Both are wiring, never software.
3. Only then any protocol code.

If SPI is marginal, lower the clock before suspecting anything else — RF24 defaults to
10 MHz and dupont wire often cannot carry it. `RF24 radio(CE, CSN, 4000000)` is a
first-resort, not a last-resort.
