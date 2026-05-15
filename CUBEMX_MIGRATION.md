# CubeMX Baseline Migration (STM32H743IITx)

This repository is being transitioned to a **CubeMX-owned hardware baseline** while keeping display/touch/application logic in user-owned files.

## 1) CubeMX baseline target

- MCU: `STM32H743IITx` (LQFP176)
- Toolchain: `Makefile / GCC`
- Debug: `Serial Wire`
- RCC input: `HSE Crystal/Ceramic Resonator (25 MHz)`

## 2) Clock tree baseline to replicate

- System clock source: PLL1 from HSE
- PLL1: `M=5, N=192, P=2, Q=4`
  - SYSCLK = 480 MHz
  - CPU (D1CPRE /1) = 480 MHz
  - AHB (HPRE /2) = 240 MHz
  - APB1/2/3/4 (/2) = 120 MHz
- Voltage scaling / overdrive enabled for 480 MHz operation

## 3) FMC kernel clock path

- PLL2: `M2=25, N2=440, P2=2, R2=2`
- FMC kernel source: `PLL2R` (220 MHz)

## 4) LTDC pixel clock path

- LTDC source: `PLL3R`
- Base suggestion: `M3=25, N3=300`
- Pick `R3` by panel:
  - 800x480 -> `R3=9` (~33 MHz)
  - 1024x600 -> `R3=6` (50 MHz)
  - 1280x800 -> `R3=5` (60 MHz)

## 5) FMC SDRAM baseline

- FMC SDRAM Bank1 (`0xC0000000`)
- 16-bit, 4 internal banks, Row=13, Col=9
- CAS=2, SDClock=`FMC/2`, ReadBurst=Enable, ReadPipe=1
- Timing:
  - `TMRD=2`
  - `TXSR=8`
  - `TRAS=7`
  - `TRC=7`
  - `TWR=2`
  - `TRP=2`
  - `TRCD=2`
- Refresh count: `839`

## 6) LTDC timing baseline example (800x480)

- HSW=1, HBP=46, HFP=210
- VSW=1, VBP=23, VFP=22

## 7) Ownership boundary rule

- CubeMX owns:
  - clock init
  - GPIO init
  - FMC init skeleton
  - LTDC init skeleton
- User code owns:
  - display driver logic
  - touch driver logic
  - UI/app behavior

Do not duplicate CubeMX-generated peripheral init with separate vendor-style register init paths.

## 8) Validation sequence after each regeneration

1. Boot + UART log
2. SDRAM read/write test
3. LTDC color fill / color bars
4. Backlight + reset pin behavior
5. Touch probe (I2C/SPI + IRQ path used on board)

## 9) Build note for this repository

Build command is:

```bash
make
```

In this current sandbox run, build could not complete due missing toolchain binary:
`arm-none-eabi-gcc: not found`.
