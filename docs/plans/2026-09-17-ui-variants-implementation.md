# Three Monitoring UI Variants Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Implement three independently runnable Qt monitoring interfaces in the existing A, B, and C worktrees while preserving all acquisition and simulation behavior.

**Architecture:** Each worktree keeps the existing model, generator, detector, timer, and custom-painted list. Only `MainWindow` composition, QSS, waveform paint tokens, and UI-contract tests differ between branches.

**Tech Stack:** C++17, Qt 6 Widgets, QPainter, Qt Test, CMake, Git worktrees.

---

### Task 1: Implement variant A in `codex/ui-a`

**Files:**
- Modify: `tests/test_mainwindow.cpp`
- Modify: `tests/test_waveformwidgets.cpp`
- Modify: `mainwindow.cpp`

**Steps:**
1. Add failing tests for the `A` design marker, compact command-bar objects, and A waveform palette.
2. Run `test_mainwindow` and `test_waveformwidgets`; verify failures describe the missing A contract.
3. Implement the industrial command layout, QSS, row grouping, and non-color event marker.
4. Build and run all tests; expect 4/4 passing.
5. Commit to `codex/ui-a`.

### Task 2: Implement variant B in `codex/ui-b`

**Files:**
- Modify: `tests/test_mainwindow.cpp`
- Modify: `tests/test_waveformwidgets.cpp`
- Modify: `mainwindow.cpp`

**Steps:**
1. Add failing tests for the `B` design marker, instrument-panel objects, and light-background waveform palette.
2. Run the focused tests and verify the intended failures.
3. Implement the scientific instrument layout, light QSS, high-contrast grid, and event marker.
4. Build and run all tests; expect 4/4 passing.
5. Commit to `codex/ui-b`.

### Task 3: Implement variant C in `codex/ui-c`

**Files:**
- Modify: `tests/test_mainwindow.cpp`
- Modify: `tests/test_waveformwidgets.cpp`
- Modify: `mainwindow.cpp`

**Steps:**
1. Add failing tests for the `C` design marker, control-deck objects, and low-light waveform palette.
2. Run the focused tests and verify the intended failures.
3. Implement the low-light control deck, energy scale, restrained highlights, and event marker.
4. Build and run all tests; expect 4/4 passing.
5. Commit to `codex/ui-c`.

### Task 4: Cross-variant verification

**Steps:**
1. Confirm each worktree is on its intended branch and contains only its own design changes.
2. Re-run all tests in A, B, and C.
3. Compare window titles, design markers, palettes, and object hierarchy to ensure the variants are visibly distinct.
4. Report executable paths and comparison instructions.

