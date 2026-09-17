# Continuous Signal and FFT Analysis Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add continuous sine and realistic seismic simulation modes, then let users inspect any channel in a bottom time-domain/FFT drawer while preserving the 100-channel single-timer architecture.

**Architecture:** Keep `WaveformModel` as the shared 100-channel ring buffer and keep `MainWindow::generateData()` as the only timed orchestration point. Extend the existing seeded signal/event logic, add one dependency-free radix-2 spectrum analyzer, and add selection plus a lightweight `QPainter` detail view beneath the existing monitor list.

**Tech Stack:** C++17, Qt 6 Core/Widgets/Test, CMake/CTest, `QPainter`, `std::complex`

---

### Task 1: Add a tested spectrum analyzer

**Files:**
- Create: `spectrumanalyzer.h`
- Create: `spectrumanalyzer.cpp`
- Create: `tests/test_spectrumanalyzer.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing spectrum tests**

Define a small value type and wished-for API:

```cpp
struct SpectrumResult
{
    QVector<double> frequencies;
    QVector<double> magnitudesDb;
    double peakFrequencyHz = 0.0;

    bool isValid() const;
};

class SpectrumAnalyzer
{
public:
    static SpectrumResult analyze(const QVector<double> &samples,
                                  int sampleRate,
                                  int maximumInputSize = 4096,
                                  double maximumFrequencyHz = 200.0);
};
```

Add tests that:

- generate 20, 30 and 80 Hz sine waves at both 500 Hz and 1000 Hz sample rates and require the detected peak to be within one FFT bin;
- use more than 4096 input samples and verify the analyzer caps the FFT size safely;
- verify empty, too-short, invalid-rate and constant inputs produce an invalid/empty result rather than NaN or infinity;
- verify frequency and magnitude arrays have equal size, cover no values above 200 Hz, and keep magnitudes in the `[-60, 0]` dB display range.

**Step 2: Register and run the test to verify RED**

Add a `test_spectrumanalyzer` target linked with `Qt::Core` and `Qt::Test`.

Run:

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:\develop\Qt\6.11.2\mingw_64 -DCMAKE_CXX_COMPILER=D:\develop\Qt\Tools\mingw1310_64\bin\g++.exe -DCMAKE_MAKE_PROGRAM=D:\develop\Qt\Tools\mingw1310_64\bin\mingw32-make.exe
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_spectrumanalyzer -j 4
```

Expected: build fails because `SpectrumAnalyzer::analyze()` is not implemented.

**Step 3: Implement the minimal radix-2 FFT**

Implement:

- largest power-of-two selection capped at 4096;
- minimum useful input of 256 samples;
- mean removal and Hann windowing;
- iterative bit-reversal and Cooley–Tukey butterflies using `std::complex<double>`;
- one-sided bins from 0 Hz through `min(200 Hz, Nyquist)`;
- peak search excluding DC;
- normalization to the strongest non-DC bin, clamped to `-60 dB`.

Do not add external FFT or chart dependencies.

**Step 4: Run the focused test to verify GREEN**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_spectrumanalyzer -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R spectrumanalyzer --output-on-failure
```

Expected: `spectrumanalyzer` passes.

**Step 5: Commit**

```powershell
git add CMakeLists.txt spectrumanalyzer.h spectrumanalyzer.cpp tests/test_spectrumanalyzer.cpp
git commit -m "feat: add FFT spectrum analyzer"
```

### Task 2: Add continuous sine generation and richer seismic events

**Files:**
- Modify: `eventlogic.h`
- Modify: `eventlogic.cpp`
- Modify: `tests/test_eventlogic.cpp`

**Step 1: Write failing sine-generation tests**

Add a `sineSample(channel, timeSeconds, sampleRate, frequencyHz)` entry point to the existing seeded `SeismicSignalGenerator` rather than replacing the class.

Test that:

- 20, 30, 80 and 137.5 Hz requests remain continuously oscillatory for at least five seconds;
- zero crossings estimate the requested frequency within a small noise tolerance;
- the same seed reproduces the same samples;
- different channels have different phase/amplitude traces;
- invalid channels, sample rates and frequencies return `0.0` safely.

**Step 2: Run and verify RED**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_eventlogic -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R eventlogic --output-on-failure
```

Expected: compile failure because `sineSample()` is missing.

**Step 3: Implement the minimal continuous sine path**

Reuse the generator's seeded per-channel phase and gain. Produce a stable main sine with small deterministic noise; clamp accepted frequency to the validated 1–200 Hz UI range without introducing another timer or state owner.

**Step 4: Add failing seismic-shape tests**

Specify that one event contains:

- non-zero early P-wave energy;
- stronger S-wave energy after a deterministic P/S delay;
- a non-zero, decaying coda beyond the old 0.32-second cutoff;
- bounded output after the event duration;
- different shapes for consecutive event indices while remaining deterministic for the same seed.

Keep the existing 4–9 spatially adjacent targets, propagation delay, attenuation and detector-confirmation tests.

**Step 5: Run and verify the new seismic tests fail**

Expected: failures at the coda and event-variation assertions because the current impulse is a fixed short 18 Hz waveform.

**Step 6: Implement P, S and coda components**

Use deterministic event-index-derived parameters for P frequency, S frequency, duration, amplitude and P/S spacing. Apply smooth attack/decay envelopes and retain distance attenuation and arrival delay. Increase the continuous background microtremor only enough to remain visible in narrow rows without triggering the `0.78` detector threshold.

**Step 7: Run focused tests and commit**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_eventlogic -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R eventlogic --output-on-failure
git add eventlogic.h eventlogic.cpp tests/test_eventlogic.cpp
git commit -m "feat: add continuous sine and seismic signals"
```

Expected: all event-logic tests pass.

### Task 3: Add mode and frequency controls to the existing B interface

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_mainwindow.cpp`

**Step 1: Write failing control-contract tests**

Require object names so tests and future UI work remain stable:

```text
signalModeBox
frequencyPresetBox
customFrequencySpin
```

Test that:

- the mode box contains `地震模拟` and `正弦测试`;
- the preset box contains 20, 30, 80 and custom choices;
- the custom control accepts 1–200 Hz;
- frequency controls are disabled in the default seismic mode;
- choosing sine mode enables presets, while choosing custom also enables the numeric input;
- the window still owns exactly one `QTimer`.

**Step 2: Run and verify RED**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_mainwindow -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R mainwindow --output-on-failure
```

Expected: failures because the controls do not exist.

**Step 3: Add the minimum controls and state**

Add a two-value signal-mode enum owned by `MainWindow`, defaulting to seismic to preserve current startup behavior. Add the three compact controls to `readoutStrip`; use `QDoubleSpinBox` with 0.1 Hz precision for custom input.

Create one reset helper that clears `WaveformModel`, detector state, generator state and detail spectrum whenever mode, frequency or sample rate changes. Keep total acquisition count and the existing timer object intact.

**Step 4: Add failing generation-routing tests**

Update the existing seeded generator test to explicitly cover seismic mode, then add tests proving that 20/30/80/custom selections route all 100 channels through `sineSample()` and fill the same number of samples per tick.

**Step 5: Implement generation routing**

In `generateData()` choose exactly one path:

```cpp
if (signalMode == SignalMode::Sine)
    value = signalGenerator.sineSample(channel, t, sampleRate, sineFrequencyHz);
else
    value = signalGenerator.sample(channel, t, sampleRate)
            + eventScheduler.impulse(channel, t);
```

Do not create per-channel objects or timers.

**Step 6: Run focused tests and commit**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_mainwindow -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R mainwindow --output-on-failure
git add mainwindow.h mainwindow.cpp tests/test_mainwindow.cpp
git commit -m "feat: add simulation mode controls"
```

### Task 4: Add channel selection and activation without changing list architecture

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_waveformwidgets.cpp`

**Step 1: Write failing interaction tests**

Specify this `WaveformListWidget` API:

```cpp
int selectedChannel() const;
void setSelectedChannel(int channel);

signals:
    void channelSelected(int channel);
    void channelActivated(int channel);
```

Use `QTest::mouseClick()` and `QTest::mouseDClick()` at known row coordinates. Verify correct channel mapping, safe clicks outside rows, signal emission, and state changes.

Render the selected row into a `QImage` and verify it has a distinct border/background in addition to the existing amplitude colors and event marker.

**Step 2: Run and verify RED**

Expected: compile failure because the selection API and signals are absent.

**Step 3: Implement minimal mouse handling and paint state**

Override `mousePressEvent()` and `mouseDoubleClickEvent()`, reuse `channelAtY()`, and update only the old/new row regions when selection changes. Preserve the single custom list widget, proportional channel mapping and pixel-bucket downsampling.

**Step 4: Add configurable visible-row count**

Add `setVisibleRows(int)` with default 50. When the drawer opens it will use 34; when it closes it will return to 50. Make `setViewportHeight()` derive fixed list height from this value.

Test both values at the current minimum and default window sizes.

**Step 5: Run tests and commit**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_waveformwidgets -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R waveformwidgets --output-on-failure
git add mainwindow.h mainwindow.cpp tests/test_waveformwidgets.cpp
git commit -m "feat: add waveform channel selection"
```

### Task 5: Add the bottom analysis drawer

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_mainwindow.cpp`
- Modify: `tests/test_waveformwidgets.cpp`

**Step 1: Write failing drawer behavior tests**

Require these stable object names:

```text
monitorSplitter
channelDetailPanel
detailCloseButton
detailChannelLabel
detailPeakLabel
detailRmsLabel
detailFrequencyLabel
channelAnalysisPlot
```

Test that:

- the detail panel starts hidden;
- double-clicking channel 36 selects it and opens the panel;
- the list switches to 34 visible rows while open;
- clicking another channel updates the detail channel label without closing the panel;
- the close button hides the panel and restores 50 visible rows;
- opening and closing never changes the number of `QTimer` children.

**Step 2: Run and verify RED**

Expected: failures because the splitter and panel do not exist.

**Step 3: Build the layout shell**

Place the existing waveform panel and a hidden detail panel in a vertical `QSplitter`. On activation, show the detail panel and set approximately 68/32 sizes. The header contains selected channel, peak, RMS, main frequency and a real close button.

Keep the current B palette, typography and compact scientific-instrument styling. Do not add animation timers.

**Step 4: Write failing plot rendering tests**

Feed a known 30 Hz channel buffer, open the panel, force a refresh and render `channelAnalysisPlot` to an image. Verify:

- both time-domain and spectrum plot regions contain non-background pixels;
- the detail frequency label reports a value near 30 Hz;
- fewer than 256 samples render the collecting-data placeholder safely;
- switching channels changes the displayed statistics.

**Step 5: Run and verify RED**

Expected: failures because the detail plot has no data or spectrum integration.

**Step 6: Implement the lightweight detail painter**

Add a small `ChannelAnalysisPlot` custom widget that receives the model, selected channel, sample rate and cached `SpectrumResult`. Paint:

- upper time plot using the same five amplitude colors;
- lower spectrum plot using the scientific blue accent and labeled 0/50/100/150/200 Hz grid;
- a collecting placeholder for insufficient data.

Compute/copy the selected-channel spectrum only while the drawer is visible and only every fifth `generateData()` tick (about 10 Hz). Refresh labels from `WaveformModel::peak()`, `rms()` and the cached peak frequency. Reuse the existing timer counter; do not add a timer.

**Step 7: Run focused tests and commit**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build --target test_waveformwidgets test_mainwindow -j 4
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build -R "waveformwidgets|mainwindow" --output-on-failure
git add CMakeLists.txt mainwindow.h mainwindow.cpp tests/test_mainwindow.cpp tests/test_waveformwidgets.cpp
git commit -m "feat: add channel FFT detail drawer"
```

### Task 6: Verify the complete 100-channel workflow

**Files:**
- Modify only if a failing verification exposes a directly related defect.

**Step 1: Build every target**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\cmake.exe --build build -j 4
```

Expected: clean build with no compiler or linker errors.

**Step 2: Run the full automated suite**

```powershell
D:\develop\Qt\Tools\CMake_64\bin\ctest.exe --test-dir build --output-on-failure
```

Expected: all original tests plus `spectrumanalyzer` pass.

**Step 3: Run structural checks**

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors; only intentional files are modified before the final commit.

**Step 4: Perform focused manual UI verification**

Launch `build\Seismic_Waveforms_demo.exe` and verify:

- seismic mode starts with continuous non-flat traces;
- sine presets and a custom frequency visibly change all channels;
- single-click selection and double-click drawer behavior are clear;
- time and FFT plots update together and the FFT peak follows the sine setting;
- closing the drawer restores 50 visible rows;
- acquisition remains responsive with 100 channels and one timer.

**Step 5: Review the final diff and commit any verification-only correction**

```powershell
git diff --stat
git diff -- CMakeLists.txt eventlogic.h eventlogic.cpp spectrumanalyzer.h spectrumanalyzer.cpp mainwindow.h mainwindow.cpp tests
```

If no correction was needed, do not create an empty commit.
