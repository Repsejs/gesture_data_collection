# Gesture Data Collection

Firmware for collecting accelerometer training data using the GD32VF103 with MPU6500.

## Setup

1. Clone this repository into your GD32VF103 toolchain projects directory
2. Create a copy of the makefile, .deps folder and .vscode folder
3. Place the copies in the cloned repo so that you can compile it 
4. Build with `make`
5. Flash to your device

## Hardware Required

- GD32VF103 board (Longan Nano)
- MPU6500 accelerometer (**Check that your board has it!**)
- micro-USB cable (for power, programming, and serial prompts)

## How to use serial (Recommended)

0. Flash MCU with the software or it will not be detected as a USB device
1. Connect USB cable
2. Press Reset on the MCU 
3. wait .5 seconds
4. Run "uv run capture_serial_data.py" in the gesture-recognition-lab directory
5. Device will prompt you for each gesture
6. Press ENTER when ready, then perform the gesture
7. Repeat for all gestures
8. Data saved to `TDATA_serial.CSV` on computer

## How to Use SD Card

0. Flash MCU with the software or it will not be detected as a USB device
1. Insert SD card
2. Connect USB cable
3. Open serial terminal (115200 baud)
4. Device will prompt you for each gesture
5. Press ENTER when ready, then perform the gesture
6. Repeat for all gestures
7. Data saved to `TDATA.CSV` on SD card

## Configure Gestures

Edit `main.c`:

```c
#define NUMBER_OF_GESTURES 3              // How many different gestures
#define MEASUREMENTS_OF_EACH_GESTURE 100  // Repetitions per gesture
#define MEASUREMENTS_PER_GESTURE 100      // Samples per repetition
```

Change labels (line 37):
```c
static const char* labels[] = {"idle", "waving", "sliding"};
```

## Data Format

CSV file with format:
```
X-acc;Y-acc;Z-acc;label
-234;1024;-15678;waving
128;-512;16200;waving
...
```

Default configuration: 30,000 samples (3 gestures × 100 reps × 100 samples)

Use this data to train gesture recognition models.
