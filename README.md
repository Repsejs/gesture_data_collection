# Gesture Data Collection

Firmware for collecting accelerometer training data using the GD32VF103 with MPU6500.

## Setup

1. Place this project folder in your GD32VF103 toolchain projects directory
2. Build with `make`
3. Flash to your device

## Hardware Required

- GD32VF103 board (Longan Nano)
- MPU6500 accelerometer
- MicroSD card
- USB cable (for power, programming, and serial prompts)

## How to Use

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
