# Wiring guide: test boards to Pico 2

All communication is one-way, Pico to screen. **MISO is never connected.**

## 1. Standard signal names and jumper colours

Display boards label the same few signals in many different ways. The library, the code comments and this guide always use the **standard name** in the left column.

| Standard name | What it does | Other labels you'll see | Jumper colour |
|---|---|---|---|
| **SCK** | SPI clock, Pico to display | SCL, SCLK, CLK | **YELLOW** |
| **MOSI** | Data, Pico to display | SDA, SDI, DIN, SI | **ORANGE** |
| **MISO** | Data, display to Pico (not used) | SDO, SDA-O, DOUT, SO | **N/A** |
| **CS** | Chip select: LOW = "you're being talked to" | CS, SS, CE | **GREEN** |
| **DC** | Data/Command: LOW = command, HIGH = data | DC, D/C, RS, A0 | **BLUE** |
| **RST** | Hardware reset, active LOW | RST, RESET, RES | **WHITE** |
| **BL** | Backlight control | BL, LED, BLK | **PURPLE** |
| **VCC** | Power | VCC, VIN, 3V3 | **RED** |
| **GND** | Ground | GND | **BLACK** |

**SDA and SCL are not I2C here.** Many SPI display boards borrow the I2C names SDA and SCL for their SPI pins. On all three of these boards they are SPI: SCL means SCK and SDA means MOSI.


## 2. Board-by-board mapping

### 2.0" ST7789: blue board "GMT020-02-8P VER:1.21" (DM screen)

8-pin header, listed top to bottom as it appears in your photo:

| Board label | Standard | Connect to | Notes | Colour |
|---|---|---|---|---|
| BL | BL | GP16 | Drives the backlight transistor (Q2). HIGH = on. PWM brightness. | **PURPLE** |
| CS | CS | GP17 | | **GREEN** |
| DC | DC | GP20 | Shared | **BLUE** |
| RST | RST | GP21 | Shared | **WHITE** |
| SDA | MOSI | GP19 | Not I2C | **ORANGE** |
| SCL | SCK | GP18 | Not I2C | **YELLOW** |
| VCC | VCC | As in your working POC | | **RED** |
| GND | GND | GND | | **BLACK** |

This board has a small resistor-and-diode network on each signal input (R1 to R5, D1 to D5). That kind of circuit adapts 5 V logic to the chip, but it can also limit how fast the board accepts signals. See section 5.

### 2.8" ILI9341: red board "2.8" TFT 240xRGBx320 V1.1" (player screen)

14-pin left header, listed top to bottom:

| Board label | Standard | Connect to | Notes | Colour |
|---|---|---|---|---|
| T_IRQ | (touch) | Not connected | Touch interface, not used | N/A |
| T_DO | (touch) | Not connected | | N/A |
| T_DIN | (touch) | Not connected | | N/A |
| T_CS | (touch) | Not connected | | N/A |
| T_CLK | (touch) | Not connected | | N/A |
| SDO(MISO) | MISO | Not connected | | N/A |
| LED | BL | GP15 | Backlight. HIGH = on. PWM brightness. | **PURPLE** |
| SCK | SCK | GP18 | Shared | **YELLOW** |
| SDI(MOSI) | MOSI | GP19 | Shared | **ORANGE** |
| DC | DC | GP20 | Shared | **BLUE** |
| RESET | RST | GP21 | Shared | **WHITE** |
| CS | CS | GP22 | | **GREEN** |
| GND | GND | GND | | **BLACK** |
| VCC | VCC | As in your working POC | | **RED** |

- **Right-edge SD_SCK, SD_MISO, SD_MOSI, SD_CS:** These go to the microSD slot, a separate device from the display. Leave them unconnected.
- **J1:** Leave it exactly as it was in your working POC. On these boards it's commonly a power jumper related to the onboard regulator.

### 3.5" ST7796S: blue board, 9-pin "SPI" header (alternate player screen)

Listed pin 1 to pin 9:

| Pin | Board label | Standard | Connect to | Notes | Colour |
|---|---|---|---|---|---|
| 1 | GND | GND | GND | | **BLACK** |
| 2 | VCC | VCC | As in your earlier tests | | **RED** |
| 3 | SCL | SCK | GP18 | Not I2C | **YELLOW** |
| 4 | SDA | MOSI | GP19 | Not I2C | **ORANGE** |
| 5 | RST | RST | GP21 | Shared | **WHITE** |
| 6 | DC | DC | GP20 | Shared | **BLUE** |
| 7 | CS | CS | GP22 | Takes the ILI9341's place; set `PLAYER_BOARD` in the demo | **GREEN** |
| 8 | BL | BL | GP15 | Backlight | **PURPLE** |
| 9 | SDA-O | MISO | Not connected | | N/A |

- **The two 8080 connectors** (16-bit and 8-bit) are parallel interfaces. They aren't used.
- **Interface-mode jumpers IM0, IM1, IM2:** The table printed on the board shows that 4-wire SPI needs all three set to **1**. In your photo they appear to be on the "1" side, which is correct. (The "SPI3" column, IM0=1 IM1=0 IM2=1, is a 3-wire mode with no DC pin, which this library doesn't support.)

## 3. Pico 2 pin assignments (shared bus)

All pins can be changed in the demo's `#define` block. Everything except the player backlight is on the right-hand side of the Pico (physical pins 21 to 29). The player backlight, GP15 on pin 20, sits directly across from pin 21. The ADC pins (GP26 to GP28) are left free for battery monitoring later.

| Signal | Pico GPIO | Physical pin | Goes to | Colour |
|---|---|---|---|---|
| BL, player screen | GP15 | 20 | 2.8" or 3.5" board | **PURPLE** |
| BL, DM screen | GP16 | 21 | 2.0" ST7789 | **PURPLE** |
| CS, DM screen | GP17 | 22 | 2.0" ST7789 | **GREEN** |
| GND | GND | 23 or 28 | All screens | **BLACK** |
| SCK | GP18 | 24 | All screens (shared) | **YELLOW** |
| MOSI | GP19 | 25 | All screens (shared) | **ORANGE** |
| DC | GP20 | 26 | All screens (shared) | **BLUE** |
| RST | GP21 | 27 | All screens (shared) | **WHITE** |
| CS, player screen | GP22 | 29 | 2.8" ILI9341 (or 3.5" ST7796S) | **GREEN** |

**Counting physical pins:** the Pico's ground pins (3, 8, 13, 18, 23, 28, 33, 38) sit between the GPIO pins, so counting "GPIO numbers" along the edge drifts by one after each ground. Pin 23 is ground, so GP17 (pin 22) and GP18 (pin 24) are on either side of it.

The library holds every screen's CS pin high from the moment the bus starts, so a screen that isn't initialised yet can stay connected safely.

**Power:** Keep powering VCC the way that worked in your earlier tests. If you run several screens from the Pico's 3V3(OUT) pin (physical pin 36), remember it's limited to roughly 300 mA total. The 3.5" backlight is the largest single draw.

**Backlight brightness:** Each backlight pin is driven by PWM at 10 kHz, set per screen with `qg_screen_set_brightness()`. GP16 and GP15 belong to different PWM units, so the two screens dim independently.

## 4. Confirmed panel settings

These are the settings each board needs in its `qg_screen_config_t`. All three run at 37.5 MHz (`spi_hz = 40000000`) on the breadboard.

| Board | Driver | Size | `invert` | `bgr` | `mirror_x` | `mirror_y` |
|---|---|---|---|---|---|---|
| 2.0" blue | `QG_DRIVER_ST7789` | 240x320 | true | false | false | false |
| 2.8" red | `QG_DRIVER_ILI9341` | 240x320 | false | true | true | false |
| 3.5" blue | `QG_DRIVER_ST7796` | 320x480 | false | true | true | false |

The 3.5" board was sold as "ST7789V/ST7796S". It can't be an ST7789V (that chip tops out at 240x320), and it gave a clean image rather than scrambled colours with 16-bit data, which rules out the look-alike ILI9488. It's an ST7796S.

**Recognising wrong settings:**
- **Wrong `invert`:** every colour shows as its opposite. White becomes black, and yellow becomes blue.
- **Wrong `bgr`:** red and blue trade places, but black, white and green are correct.
- **Wrong mirror:** the corner squares in the orientation test swap sides.

## 5. Troubleshooting: backlight on, screen black

If the backlight lights but nothing is drawn, and the serial output shows normal timings, the Pico is sending data but the screen isn't accepting it. Check in this order:

1. **SPI speed.** Set `SPI_HZ` to `1000000` (1 MHz). If the picture appears, raise it in steps (5, 10, 20 MHz) to find the highest speed that works reliably, then use one step below it.
2. **Wiring, by physical pin number.** Use the table in section 3, and watch for the ground-pin drift described there. SCK and MOSI swapped, or CS on the wrong pin, both give exactly this symptom.
3. **Compare with the last working POC.** Same pins? Same SPI speed?
