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

The Makefile defaults to `g++` and C++17. The first Phase 6 target build and all
unit tests passed natively on the QNX Raspberry Pi. A fake-mode lifecycle
regression was then found and fixed; the corrected Phase 6 runtime and its new
end-to-end test still require target revalidation.

## Host tests

The parser and feature-window code can also be compiled and tested on a
development host without mlpack:

```sh
make test-host
```

Passing these commands validates the host build only, not the target-QNX build.

The complete executable and anomaly tests require mlpack headers plus Armadillo.
This environment links `-larmadillo`; mlpack 4.7 is header-based for the KNN API
used here.

File mode accepts two independent inputs:

```sh
./surgymonitor --left-file data/left_normal.txt \
  --right-file data/right_normal.txt
```

Values A through E are normalized from 0–4095 to 0.0–1.0. Malformed lines are
ignored and counted.

Each hand has an independent 20-frame sliding window. Once full, it produces 15
features in a fixed order: five mean positions, five average absolute per-frame
movements, and five population standard deviations. This is approximately a
200 ms history when telemetry arrives at 100 Hz. Real QNX serial devices, mlpack
thread scheduling, and the final dashboard are added in later phases.

## Baselines and mlpack scoring

SurgY uses separate left and right baselines because sensor calibration and
normal task motion can differ by hand. Each CSV row contains exactly the 15
features in the order documented above. Blank lines are ignored; incorrect
column counts, invalid numbers, and non-finite values reject the baseline with a
line-specific error and counters.

Record overlapping feature windows from normal manipulation data:

```sh
./surgymonitor \
  --left-file data/left_normal.txt --right-file data/right_normal.txt \
  --record-left-baseline baseline-left.csv \
  --record-right-baseline baseline-right.csv
```

Score live/file input with a manually selected threshold:

```sh
./surgymonitor \
  --left-file data/left_normal.txt --right-file data/right_mixed.txt \
  --left-baseline baseline-left.csv --right-baseline baseline-right.csv \
  --threshold 0.5 --k 5
```

For each live vector, mlpack KNN searches the hand's calibrated normal vectors.
The raw anomaly score is the average Euclidean distance to the nearest `k`
vectors, and a score strictly greater than the threshold is classified as an
anomaly. If the baseline has fewer than `k` rows, all available rows are used and
the result reports that actual neighbour count.

Judge-facing explanation: "mlpack compares each live multi-finger movement
window against calibrated normal manipulation windows. If the current behaviour
is far from its nearest normal examples, the anomaly score increases."

This is behavioural anomaly detection for training feedback. It does not
diagnose tremor or neurological conditions and does not certify surgical
competence.

## Real dual-glove serial mode

Device paths are always supplied at runtime; none are hardcoded:

```sh
./surgymonitor \
  --left /dev/serusb1 --right /dev/serusb2 \
  --left-baseline baseline-left.csv \
  --right-baseline baseline-right.csv \
  --threshold 0.5 --k 5
```

Each device is opened independently and configured with POSIX `termios` for
115200 baud, 8 data bits, no parity, one stop bit, and raw input. USB is an
observation-only path; the monitor never sends haptic commands. Read chunks are
fed through the same newline accumulator and Alpha parser used by file mode.

QNX device names vary with enumeration. Inspect the serial USB devices available
under `/dev` after connecting each glove, then pass the observed paths with
`--left` and `--right`. The program does not assume `serusb1` or `serusb2`.

### Thread structure

- Left ingestion pthread: requests `SCHED_FIFO` priority 30.
- Right ingestion pthread: requests `SCHED_FIFO` priority 30.
- Feature/mlpack processing pthread: requests priority 20.
- Terminal dashboard pthread: requests priority 10.

Every request uses `pthread_setschedparam()`. If policy or permissions prevent
`SCHED_FIFO`, the program prints a warning and continues under normal scheduling.
These priorities express processing intent; they are not a hard real-time
guarantee.

Each ingestion thread owns its device and parser and writes valid frames to its
own bounded 256-frame queue. The processing thread drains both queues and owns
the feature windows and detectors. If a queue fills, its oldest frame is dropped
and counted instead of allowing unbounded memory growth. A read/open failure or
disconnect marks only that hand failed; the other ingestion thread continues.

For finite file/fake sources, EOF marks only that source complete. Processing
shuts down only after both sources are complete and both queues are empty. The
runtime then prints one authoritative final snapshot; it does not treat an early
zero-frame dashboard sample as the result of the run.

Health output includes valid frames, parse errors, read errors, dropped frames,
last-frame freshness, and `ONLINE`/`OFFLINE`. The default offline timeout is one
second and can be changed with `--offline-timeout-ms`.
