# Rotating Event Detection Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace fixed three-channel alarms with reproducible rotating event injection and data-driven event detection over a true one-second window.

**Architecture:** Add a small event scheduler and detector independent of the waveform model. MainWindow composes generated baseline data with scheduler output, feeds only samples to the detector, and publishes detector state through WaveformModel for rendering.

**Tech Stack:** C++17, Qt 6 Core/Widgets/Test, CMake/CTest

---

### Task 1: One-second waveform model state

**Files:**
- Modify: `waveformmodel.h`
- Modify: `waveformmodel.cpp`
- Test: `tests/test_waveformmodel.cpp`

1. Add failing tests for resizing the cache, clearing samples, and clearing event state.
2. Run `test_waveformmodel` and confirm the new tests fail because the API is absent.
3. Add `setMaxPoints`, `clear`, `setEventDetected`, and `eventDetected` with minimal storage.
4. Rebuild and run `test_waveformmodel`; expect all cases to pass.

### Task 2: Deterministic scheduler and stateful detector

**Files:**
- Create: `eventlogic.h`
- Create: `eventlogic.cpp`
- Create: `tests/test_eventlogic.cpp`
- Modify: `CMakeLists.txt`

1. Add tests proving seeded scheduling is repeatable, event target counts vary from 1 to 3, adjacent event targets do not overlap, confirmation requires two abnormal batches, and recovery requires 50 normal batches.
2. Build and run the test; confirm failure because the implementation is absent.
3. Implement the smallest scheduler and detector satisfying those behaviors.
4. Rebuild and run `test_eventlogic`; expect all cases to pass.

### Task 3: Main window integration

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_mainwindow.cpp`
- Modify: `CMakeLists.txt`

1. Add failing tests for a 1000-point cap and for clearing/resizing state when switching to 500 Hz.
2. Run `test_mainwindow` and confirm the new tests fail against the 5000-point fixed cache.
3. Replace fixed channel injection with scheduler output, feed batches to the detector, publish detector state to the model, and render “检测到事件”.
4. Clear samples/detection state and set the cache limit on sample-rate changes.
5. Rebuild and run `test_mainwindow`; expect all cases to pass.

### Task 4: Full verification

**Files:**
- Verify only

1. Build all targets.
2. Run the full CTest suite with failure output enabled.
3. Confirm no fixed channel list remains and all event colors/statuses read `eventDetected`.
