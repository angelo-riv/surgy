# SurgY monitor for QNX

This directory contains the standalone QNX monitoring application. The current
Phase 3 implementation parses newline-delimited LucidGloves Alpha telemetry from
files using POSIX `open()`/`read()`. Reads use deliberately arbitrary chunk sizes;
only complete newline-delimited frames are parsed.

## Target environment

The deployment target is a Raspberry Pi 5 (`CPU:AARCH64`, four Cortex-A76
cores, approximately 8 GB RAM) running QNX 8.0.0. The live system reports QNX
8.0.0 even though its package repositories also contain packages built for
newer 8.0.x repository releases.

The target uses native, self-hosted `/usr/bin/g++`; `qcc` and `q++` are not
installed or required. Required AI and numerical packages are:

- `mlpack-4.7.0-r0`
- `mlpack-dev-4.7.0-r0`
- `armadillo-15.2.7-r0`
- `armadillo-dev-15.2.7-r0`
- `cereal-1.3.2-r0`
- `ensmallen-3.11.0-r0`

Native target build commands are:

```sh
make
make test
```

The Makefile defaults to `g++` and C++17. These commands describe the intended
target workflow; the complete project has not yet been copied to and compiled
on the QNX Raspberry Pi, so a target-QNX build is not yet claimed as validated.

## Host tests

The parser can also be compiled and tested on a development host:

```sh
make test
make
./surgymonitor --fake
```

Passing these commands validates the host build only, not the target-QNX build.

File mode accepts two independent inputs:

```sh
./surgymonitor --left-file data/left_normal.txt \
  --right-file data/right_mixed.txt
```

Values A through E are normalized from 0–4095 to 0.0–1.0. Malformed lines are
ignored and counted.

Each hand has an independent 20-frame sliding window. Once full, it produces 15
features in a fixed order: five mean positions, five average absolute per-frame
movements, and five population standard deviations. This is approximately a
200 ms history when telemetry arrives at 100 Hz. Real QNX serial devices, mlpack
scoring, thread scheduling, and the final dashboard are added in later phases.
