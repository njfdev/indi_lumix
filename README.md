# INDI Lumix Driver

> NOTE: This driver is EXTREMELY early in development, so expect many issues. However, I aim for this to become a stable driver over time.

This driver provides an interface to any Panasonic Lumix cameras with the WiFi API. It is based off of the (liblumix)[https://github.com/njfdev/liblumix] driver, which I am developing specifically for this INDI driver.

This driver connects to the camera over a LAN connection only. Directly connecting to the camera with WiFi and tethering over USB is planned for the future.

## Installation

First, install the liblumix driver from the [GitHub repository](https://github.com/njfdev/liblumix).

Next, clone this repository and go into it:

```bash
git clone https://github.com/njfdev/indi_lumix
cd indi_lumix
```

Make a build folder and go into it:

```bash
mkdir build
cd build
```

Build and install the driver:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release ../
make
sudo make install
```

And that's it! Make sure to open your INDI client (e.g. KStars) or restart it if it was already open. The driver should be under "Panasonic" and called "Lumix Camera". You must manually connect the Camera to a LAN and setting the IP Address in the driver control panel.

## Issues and Important Notes

- This driver is being developed with a Lumix S5IIX camera. Many variables are hardcoded for this camera with certain settings. Eventually, hardcoded variables will be replaced with the proper API calls.
- There is a common issue where the camera will stop taking photos. Reconnecting the driver will temporarily fix this issue.
