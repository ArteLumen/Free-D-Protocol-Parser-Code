# Free-D Camera Tracking Parser in C

A lightweight, high-performance C implementation designed for virtual cinematography and virtual production. This program listens for UDP packets containing **Free-D protocol** data from physical camera tracking systems (such as Stype, Mo-Sys, or custom mechanical encoders) and parses them into real-time camera position, rotation, and lens parameters.

## Features

- **UDP Network Listener:** Binds to a configurable port to receive real-time tracking streams.
- **24-Bit Bit-Shifting & Two's Complement:** Accurately decodes big-endian 24-bit signed integers into floating-point coordinate and angle values.
- **Full Parameter Extraction:** Parses Camera ID, Pan, Tilt, Roll, X, Y, Z Cartesian coordinates, Zoom, and Focus.
- **Low Latency:** Written entirely in pure C, making it ideal for real-time graphics pipelines and Unreal Engine bridges.

## Prerequisites

To compile and run this code, you need:
- A C compiler (e.g., `gcc` / `clang`)
- A POSIX-compliant operating system (Linux, macOS, or Windows via WSL/MinGW)

## Getting Started
