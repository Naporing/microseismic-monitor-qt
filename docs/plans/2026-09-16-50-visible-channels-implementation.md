# 50 Visible Channels and Slow Waveform Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Show exactly 50 of 100 channels per viewport, slow horizontal motion with a five-second buffer, and add orange as a fifth amplitude color.

**Architecture:** Keep the existing shared timer and drawing widget. Add a small scroll-area subclass that keeps the list at twice the viewport height, let the list derive row height from its current geometry, and expand model capacity according to sample rate times five seconds.

**Tech Stack:** C++17, Qt 6 Widgets, Qt Test, CMake/CTest

---

### Task 1: Specify adaptive density and five colors

**Files:**
- Modify: `tests/test_waveformwidgets.cpp`
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

1. Change widget tests to call `setViewportHeight(650)` and require a 1300-pixel list, 13-pixel row height, channels 0–49 in the first viewport, and channels 50–99 in the second.
2. Add orange expectations at `0.75` and red expectations at `1.00`.
3. Run the focused test and verify it fails for the missing API/old colors.
4. Implement dynamic row geometry and the orange threshold.
5. Add a `QScrollArea` subclass that calls `setViewportHeight(viewport()->height())` after resize.
6. Re-run the focused test and verify it passes.

### Task 2: Expand the display window to five seconds

**Files:**
- Modify: `tests/test_mainwindow.cpp`
- Modify: `mainwindow.cpp`

1. Change the buffer-duration test to require 5000 points at 1000 Hz after 251 ticks.
2. Require 2500 maximum points after selecting 500 Hz.
3. Run the focused test and verify it fails against the one-second buffer.
4. Initialize and resize `WaveformModel` buffers to `sampleRate * 5`.
5. Update visible text from one second to five seconds.
6. Re-run the focused test and verify it passes.

### Task 3: Complete verification

**Files:**
- Verify only

1. Build all targets with the configured Qt CMake executable.
2. Run all CTest tests with failure output enabled.
3. Run `git diff --check` and inspect the focused diff.
