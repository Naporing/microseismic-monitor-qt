# 100 Channel Waveform List Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the 10×10 overview and single-channel detail UI with a smooth, vertically scrollable 100-channel waveform list using four-level segment coloring and one shared timer.

**Architecture:** Keep `WaveformModel`, simulated signal generation, scheduled events, and `MainWindow`'s shared timer. Replace the two existing waveform widgets with one custom `WaveformListWidget` inside a `QScrollArea`; the widget draws only exposed rows and downsamples to the available pixel width.

**Tech Stack:** C++17, Qt 6 Widgets, Qt Test, CMake/CTest

---

### Task 1: Specify the waveform list widget contract

**Files:**
- Modify: `tests/test_waveformwidgets.cpp`
- Modify: `mainwindow.h`

**Step 1: Write failing widget tests**

Replace the grid-selection tests with tests that require:

```cpp
QCOMPARE(WaveformListWidget::rowHeight(), 54);
QCOMPARE(list.channelAtY(0), 0);
QCOMPARE(list.channelAtY(53), 0);
QCOMPARE(list.channelAtY(54), 1);
QCOMPARE(list.channelAtY(5399), 99);
QCOMPARE(list.channelAtY(-1), -1);
QCOMPARE(list.channelAtY(5400), -1);
```

Add data-driven boundary checks for `colorForAmplitude()` at `0.0`, `0.249`, `0.25`, `0.499`, `0.50`, `0.749`, and `0.75`.

**Step 2: Run the focused test to verify RED**

Run: `cmake --build build --target test_waveformwidgets && ctest --test-dir build -R waveformwidgets --output-on-failure`

Expected: compilation fails because `WaveformListWidget` does not exist.

**Step 3: Add the minimal public widget interface**

In `mainwindow.h`, replace `OverviewWidget` and `DetailWaveformWidget` with `WaveformListWidget`, exposing `rowHeight()`, `channelAtY()`, and `colorForAmplitude()` and declaring `paintEvent()`.

**Step 4: Build again to expose missing implementation**

Run the focused build command again.

Expected: linkage fails for the newly declared methods, proving the tests reach the intended API.

### Task 2: Implement efficient list painting and four-level segment colors

**Files:**
- Modify: `mainwindow.cpp`
- Test: `tests/test_waveformwidgets.cpp`

**Step 1: Implement the minimal mapping and color methods**

Use a fixed 54-pixel row height and these thresholds:

```cpp
if (amplitude < 0.25) return QColor("#35D07F");
if (amplitude < 0.50) return QColor("#34C9E8");
if (amplitude < 0.75) return QColor("#F2C94C");
return QColor("#FF5D73");
```

**Step 2: Implement visible-row painting**

Set the widget minimum height to `channelCount * rowHeight()`. In `paintEvent()`, derive the first and last rows from `event->rect()`, draw channel labels and plot backgrounds, then draw adjacent downsampled segments with colors based on the maximum absolute endpoint amplitude.

**Step 3: Run the focused test to verify GREEN**

Run: `cmake --build build --target test_waveformwidgets && ctest --test-dir build -R waveformwidgets --output-on-failure`

Expected: all waveform widget tests pass.

### Task 3: Replace the main window layout while preserving the shared data path

**Files:**
- Modify: `tests/test_mainwindow.cpp`
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`

**Step 1: Write failing window structure tests**

Require a `WaveformListWidget` named `waveformListWidget`, no legacy detail widget, 100 model channels, and exactly one descendant `QTimer`:

```cpp
QVERIFY(window.findChild<WaveformListWidget *>("waveformListWidget"));
QCOMPARE(window.findChildren<QTimer *>().size(), 1);
QCOMPARE(window.model()->channelCount(), 100);
```

Update the scheduled-event test to inspect model event flags rather than a removed selected-channel status label. Remove the obsolete channel-selection title test.

**Step 2: Run the focused test to verify RED**

Run: `cmake --build build --target test_mainwindow && ctest --test-dir build -R mainwindow --output-on-failure`

Expected: compilation or assertion failure because the new list is not in the window yet.

**Step 3: Simplify `MainWindow`**

Remove selection/detail members and slots. Build a compact top control/status strip and a main panel containing `QScrollArea` plus `WaveformListWidget`. Preserve the existing timer construction, `generateData()`, sample-rate changes, event scheduling/detection, and statistics.

**Step 4: Refresh only the list widget**

After each generated batch and sample-rate reset, call `waveformList->update()` once. Do not create any timer in the list widget or per channel.

**Step 5: Run the focused test to verify GREEN**

Run: `cmake --build build --target test_mainwindow && ctest --test-dir build -R mainwindow --output-on-failure`

Expected: all main window tests pass.

### Task 4: Verify the complete application

**Files:**
- Verify only

**Step 1: Build every target**

Run: `cmake --build build`

Expected: exit code 0.

**Step 2: Run the complete test suite**

Run: `ctest --test-dir build --output-on-failure`

Expected: all tests pass.

**Step 3: Inspect the final diff**

Run: `git diff --check` and `git diff -- mainwindow.h mainwindow.cpp tests/test_waveformwidgets.cpp tests/test_mainwindow.cpp`

Expected: no whitespace errors; changes are limited to the B-version UI and its tests, while existing model and event logic remain intact.
