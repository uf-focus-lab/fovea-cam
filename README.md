# FoveaCam++

**Systems-Level Advances for Long Range Multi-Object High-Resolution Tracking**

FoveaCam++ is a dual-camera foveated imaging system that combines a wide-angle camera with a MEMS mirror-steered telephoto camera to track multiple targets at up to 1km distance with high resolution. The system weighs approximately 1kg, occupies a 20cm cubic volume, and is designed for mounting on drones or similar robotic platforms.

> **Paper:** Y. Zhang and S. J. Koppal, "FoveaCam++: Systems-Level Advances for Long Range Multi-Object High-Resolution Tracking," *Intelligent Robots and Systems (IROS) 2024*. [IEEE Xplore](https://ieeexplore.ieee.org/document/10802188) | [PDF](https://focus.ece.ufl.edu/wp-content/uploads/2024/08/IROS-FoveaCamPlus.pdf) | [Video](https://www.youtube.com/watch?v=2JpOjopO74w)

## How It Works

A wide-angle camera (WAC) provides scene-level awareness, while a telephoto camera images reflections off a fast-steering MEMS mirror. By controlling the mirror's voltage, the telephoto view can be pointed at any region within the wide-angle field of view. The mirror interleaves across multiple targets, providing near-simultaneous high-resolution video of each.

## Features

- **Multi-object tracking** with KCF trackers dispatched per target
- **Image stabilization** with predictive motion model for motion blur cancellation
- **Strobe-synchronized** mirror control via custom MCU firmware (<200ns jitter)
- **Adaptive depth-invariant calibration** with in-situ drift correction
- **Real-time multi-threaded architecture** with lock-free data pipelines
- **Interactive kiosk UI** for task selection and camera tuning

## Building

### Dependencies

| Dependency | Purpose |
|-----------|---------|
| [OpenCV 4+](https://opencv.org/) | Image processing, tracking, calibration |
| [Spinnaker SDK](https://www.flir.com/products/spinnaker-sdk/) | FLIR camera capture |
| [libusb-1.0](https://libusb.info/) | USB communication with MEMS controller |
| [SDL2](https://www.libsdl.org/) | Display rendering |
| CMake 3.10+ | Build system |
| C++20 compiler | GCC or Clang |

### Build Commands

```bash
# Debug build
make debug

# Release build
make release

# Clean all artifacts
make clean
```

The build output is a single executable: `FoveaCam`.

## Usage

```bash
# Launch interactive kiosk mode (default)
./build/release/FoveaCam

# Run a specific task directly
./build/release/FoveaCam <task-name>
```

### Available Tasks

| Task | Description |
|------|-------------|
| `tune` | Adjust camera exposure, gain, gamma with live histogram |
| `align` | Align wide and fovea camera views |
| `checker` | Detect checkerboard patterns for intrinsic calibration |
| `aruco` | ArUco marker detection and PID-controlled tracking |
| `match` | Template matching between cameras with drift calibration |
| `track` | Multi-object KCF tracking with MEMS mirror interleaving |
| `stabilize` | Image stabilization with predictive MEMS steering |

### Configuration

Settings are stored in `~/.config/FoveaCam/config.env` (or `./config.env` as fallback) and are loaded/saved automatically. Parameters include per-camera FPS, exposure, gain, gamma, black level, lens position, zoom scale, and calibration coefficients.

## Hardware

- 2x FLIR Blackfly cameras (USB3, Spinnaker-compatible) — one wide-angle, one telephoto
- Kurokesu motorized zoom lens
- MEMS mirror with custom 3D-printed mount and beam splitter
- Microcontroller for strobe synchronization and MEMS voltage regulation
- Single board computer for real-time processing

> Hardware walkthrough coming soon. For now, please contact the authors for design source files and instructions on replicating the system.

## Software Architecture

```
External Devices      Service Threads       Shared Pipes       Task Threads
┌───────────┐         ┌──────────────┐      ┌───────────┐     ┌─────────────┐
│ Wide Cam  │────────>│ Capture/WIDE │─────>│ CapWide   │────>│ Main Loop   │
│ Fovea Cam │────────>│ Capture/FOVEA│─────>│ CapFovea  │     │ Trackers    │
│ MEMS Ctrl │<──┬────>│ MEMS/Rx      │─────>│ SyncPipe  │     │ Renderers   │
│           │   └────<│ MEMS/Tx      │<─────│ PosFIFO   │<────│ Dispatcher  │
└───────────┘         └──────────────┘      └───────────┘     └─────────────┘
```

The system uses a producer-consumer architecture with two types of data pipes:
- **FastIO** — lock-free single-writer multi-reader for camera frames (low latency)
- **FIFO** — thread-safe blocking queue for mirror position commands

Communication with the MEMS controller uses **FCMP** (FoveaCam MEMS Protocol), a custom binary protocol with COBS framing over USB CDC.

## Linux Deployment

```bash
# Install systemd services
make init

# Start as a service
make start
```

Service files for `FoveaCam`, `Xorg`, and RTMP streaming are provided in `scripts/`.

## Project Structure

```
├── include/          # Public API headers (global config, tasks, threads, GUI)
├── lib/
│   ├── calib/        # Pixel ↔ MEMS voltage coordinate transforms
│   ├── cobs/         # COBS byte stuffing for serial framing
│   ├── fcmp/         # FoveaCam MEMS Protocol implementation
│   ├── graphics/     # Multi-backend rendering (SDL2, X11, OpenCV)
│   ├── mems/         # MEMS mirror position and sync management
│   ├── threading/    # FIFO, FastIO, and exception primitives
│   ├── usb/          # USB/serial device abstraction (libusb)
│   └── util/         # Spinnaker wrapper, timing, formatting
├── src/
│   ├── main.cpp      # Entry point and task dispatcher
│   ├── kiosk.cpp     # Interactive touch-based menu UI
│   ├── global.cpp    # Configuration I/O and signal handling
│   ├── threads/      # Camera capture and MEMS control threads
│   ├── tasks/        # Application task implementations
│   └── GUI/          # Button interaction and tile rendering
├── assets/           # Splash screen and logo images
├── scripts/          # Systemd services, CMake modules, utilities
└── docs/             # Documentation (Vitepress)
```

## Citation

If you find this project useful, please consider citing our paper:

```bibtex
@inproceedings{zhang2024foveacamplus,
  author      = {Zhang, Yuxuan and Koppal, Sanjeev J.},
  booktitle   = {2024 IEEE/RSJ International Conference on Intelligent Robots and Systems (IROS)},
  title       = {FoveaCam++: Systems-Level Advances for Long Range Multi-Object High-Resolution Tracking},
  year        = {2024},
  pages       = {11594-11601},
  keywords    = {Micromechanical devices;Target tracking;Image resolution;Robot vision systems;Pipelines;LoRa;Cameras;Real-time systems;Intelligent robots;Image fusion},
  doi         = {10.1109/IROS58592.2024.10802188}
}
```

## License

MIT

## Acknowledgment

This work was supported in part by the ONR (N00014-18-1-2663, N00014-23-1-2429) and the NSF (1942444, 2330416).
