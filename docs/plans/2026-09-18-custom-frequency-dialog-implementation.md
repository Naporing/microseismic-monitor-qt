# Custom Frequency Dialog Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Remove the permanent custom-frequency spin box and request a custom sine frequency only when the user selects “自定义”.

**Architecture:** Keep `frequencyPresetBox` as the single frequency control and handle user activation rather than raw index changes, so selecting the current custom item can reopen the dialog. `MainWindow` remembers the last accepted preset index, restores it on cancel, and continues routing the accepted frequency through the existing `sineFrequencyHz` field and reset path.

**Tech Stack:** C++17, Qt 6 Widgets (`QComboBox`, `QInputDialog`, `QSignalBlocker`), Qt Test, CMake/CTest.

---

### Task 1: Specify custom-dialog behavior with Qt tests

**Files:**
- Modify: `tests/test_mainwindow.cpp:150-225`

**Step 1: Write the failing tests**

- Update the controls test to require that no `customFrequencySpin` exists.
- Replace the existing custom-frequency test with a modal-dialog test. Schedule a zero-delay callback that finds the active `QInputDialog`, sets `137.5`, and accepts it; activate the combo’s custom item and assert:
  - the dialog appeared;
  - item 3 displays `自定义 137.5 Hz`;
  - generated channel-0 samples match `sineSample(..., 137.5)`.
- Add a cancel test. Start on 30 Hz, generate one batch, reject the custom dialog, then assert the combo returns to 30 Hz and samples are unchanged.
- Trigger frequency choices through the combo’s `activated(int)` signal in tests, matching real user interaction.

**Step 2: Run the focused tests to verify RED**

Run:

```powershell
& 'D:\develop\Qt\Tools\CMake_64\bin\cmake.exe' --build build --target test_mainwindow -j 4
$env:QT_QPA_PLATFORM='minimal'
& '.\build\test_mainwindow.exe' exposesSimulationModeControls routesCustomSineFrequency cancelingCustomFrequencyKeepsPreviousSelection
```

Expected: FAIL because `customFrequencySpin` still exists and selecting custom does not open `QInputDialog`.

### Task 2: Replace the permanent editor with an on-demand dialog

**Files:**
- Modify: `mainwindow.h:90-145`
- Modify: `mainwindow.cpp:1-20, 498-513, 668-671, 754-790`

**Step 1: Implement the minimal UI change**

- Remove the `QDoubleSpinBox` include, member, construction, layout insertion, signal connection, and `changeCustomFrequency` slot.
- Add `QInputDialog` and `QSignalBlocker` includes.
- Add `int lastFrequencyPresetIndex = 0;` to `MainWindow`.
- Connect `frequencyPresetBox` using `QComboBox::activated`, allowing the currently selected custom item to be activated again.

**Step 2: Implement selection and cancellation**

Use the existing preset mapping for indexes 0–2. For index 3, call:

```cpp
bool accepted = false;
const double frequencyHz = QInputDialog::getDouble(
    this,
    "自定义正弦频率",
    "频率（Hz）",
    sineFrequencyHz,
    1.0,
    200.0,
    1,
    &accepted,
    Qt::WindowFlags(),
    0.5);
```

- On cancel, block combo signals and restore `lastFrequencyPresetIndex`; return without resetting the model.
- On accept, save the value, set item 3 to `自定义 %1 Hz`, save index 3, and reset the simulation in sine mode.
- On preset selection, restore item 3 text to `自定义`, update `lastFrequencyPresetIndex`, and retain the existing reset behavior.

**Step 3: Run the focused tests to verify GREEN**

Run the three focused tests from Task 1.

Expected: PASS.

**Step 4: Run the full verification suite**

```powershell
& 'D:\develop\Qt\Tools\CMake_64\bin\cmake.exe' --build build -j 4
$env:QT_QPA_PLATFORM='minimal'
& 'D:\develop\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build --output-on-failure
```

Expected: 5/5 test groups pass.

**Step 5: Commit**

```powershell
git add mainwindow.cpp mainwindow.h tests/test_mainwindow.cpp
git commit -m "feat: request custom frequency on demand"
```
