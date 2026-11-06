# ELAN 04f3:0903 Raw Fingerprint Capture

Captures raw image frames from ELAN 04f3:0903 fingerprint sensor using libusb and saves as BMP.

## Building
```bash
gcc -o elan_raw elan_raw.c $(pkg-config --cflags --libs libusb-1.0)
```

## Usage
```bash
sudo ./elan_raw
```
Then place and swipe your finger on the sensor. The program will save `raw_fingerprint.bmp`.
