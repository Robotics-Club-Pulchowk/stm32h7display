"""
test_rx.py — sends known byte patterns to the STM32H743 board's UART RX
(page 4 on the display) so you can visually confirm what's received matches
what was sent.

Wiring (USB-TTL adapter <-> board):
    adapter TX  ->  board PB7 (USART1_RX)
    adapter RX  <-  board PB6 (USART1_TX)   (optional, only needed if you
                                              also want to read the board's
                                              own periodic TX frames)
    adapter GND -   board GND   (required — common ground)

Usage:
    python test_rx.py <port> [mode]

    mode: counter (default) | ascii | burst | flood

Modes:
    counter  - sends one incrementing byte (0x00, 0x01, 0x02, ...) every
               second. Slow and easy to eyeball: whatever's on screen
               should match the last value printed here.
    ascii    - sends a fixed, recognizable ASCII string every second
               ("HELLO STM32 0", "HELLO STM32 1", ...). Good for
               confirming multi-byte sequences show up in the right order.
    burst    - sends 80 sequential bytes (0x00..0x4F) in one write, once.
               Your RX page buffer only shows the ~42 most recent bytes
               (RX_HEX_TEXT_LEN=128, 3 chars/byte), so this checks that the
               tail of the burst (0x1F..0x4F) is what's displayed, not
               that the whole thing got lost.
    flood    - sends 1 byte as fast as possible for 5 seconds, then stops.
               Use this to watch the "Errs:" counter on the RX page: it
               should stay put on a healthy link. If the board can't keep
               up or the line is noisy, Errs will start climbing.
"""

import sys
import time
import serial

def send_and_report(ser, data: bytes, label: str):
    ser.write(data)
    hexstr = " ".join(f"{b:02X}" for b in data)
    print(f"[{label}] sent {len(data)} byte(s): {hexstr}")

def run_counter(ser):
    counter = 0
    while True:
        payload = bytes([counter & 0xFF])
        send_and_report(ser, payload, "counter")
        counter += 1
        time.sleep(1.0)

def run_ascii(ser):
    counter = 0
    while True:
        payload = f"HELLO STM32 {counter}".encode("ascii")
        send_and_report(ser, payload, "ascii")
        counter += 1
        time.sleep(1.0)

def run_burst(ser):
    payload = bytes(range(80))
    send_and_report(ser, payload, "burst")
    print("Expect the RX page to show the TAIL of this sequence")
    print("(around ...2F 30 31 ... 4E 4F), not the start (00 01 02 ...),")
    print("since the on-screen buffer only keeps the most recent bytes.")

def run_flood(ser):
    print("Flooding single bytes for 5 seconds. Watch the Errs: counter")
    print("on the RX page — it should not climb on a healthy link.")
    end_time = time.time() + 5.0
    n = 0
    while time.time() < end_time:
        ser.write(bytes([n & 0xFF]))
        n += 1
    print(f"Sent {n} bytes total.")

MODES = {
    "counter": run_counter,
    "ascii": run_ascii,
    "burst": run_burst,
    "flood": run_flood,
}

def main():
    if len(sys.argv) < 2:
        print(f"Usage: python {sys.argv[0]} <port> [mode]")
        print(f"  modes: {', '.join(MODES.keys())} (default: counter)")
        sys.exit(1)

    port = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else "counter"

    if mode not in MODES:
        print(f"Unknown mode '{mode}'. Choose from: {', '.join(MODES.keys())}")
        sys.exit(1)

    ser = serial.Serial(port, 115200, timeout=1)
    try:
        MODES[mode](ser)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()

if __name__ == "__main__":
    main()