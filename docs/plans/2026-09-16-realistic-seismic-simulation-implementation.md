# Realistic Seismic Simulation Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace repetitive fixed sine data with deterministic non-stationary seismic background and spatially propagating events, then apply the selected deep navy visual palette.

**Architecture:** Add a lightweight stateful signal generator to the existing event-logic module. Keep `MainWindow::generateData()` as the single batch orchestration point and keep `DemoEventScheduler` as the event source, but make its targets spatially local and its waveform distance-dependent.

**Tech Stack:** C++17, Qt 6 Core/Widgets/Test, CMake/CTest

---

### Task 1: Specify realistic background behavior

**Files:**
- Modify: `tests/test_eventlogic.cpp`
- Modify: `eventlogic.h`
- Modify: `eventlogic.cpp`

1. Add failing tests for deterministic seeded output, non-repeating consecutive batches, channel diversity, and bounded quiet-background amplitude.
2. Run `test_eventlogic` and verify failure because `SeismicSignalGenerator` is missing.
3. Add per-channel filter state and deterministic channel parameters.
4. Generate colored noise, slow modulation, drift, and weak environmental components without a fixed dominant sine.
5. Run the focused test and verify it passes.

### Task 2: Specify spatial event propagation

**Files:**
- Modify: `tests/test_eventlogic.cpp`
- Modify: `eventlogic.h`
- Modify: `eventlogic.cpp`
- Modify: `tests/test_mainwindow.cpp`

1. Replace old rotating-target tests with failing tests for spatially adjacent targets, irregular event start offsets, delayed arrivals, attenuation, and repeatability.
2. Implement deterministic event source positions, slot jitter, local target selection, and distance-based waveform synthesis.
3. Update window event tests to derive their wait duration from `eventStartTime(0)`.
4. Run `test_eventlogic` and `test_mainwindow` until both pass.

### Task 3: Integrate the generator and visual palette

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_mainwindow.cpp`

1. Add a failing window test proving consecutive channel batches are not identical and quiet channels differ.
2. Replace the fixed-frequency formula in `generateData()` with `SeismicSignalGenerator::sample()` plus scheduled event impulse.
3. Reset generator state when the sample rate changes.
4. Replace the current stylesheet and waveform background/grid colors with the approved deep navy palette.
5. Run focused tests, then build all targets and run complete CTest.
6. Run `git diff --check` and inspect the final focused diff.
