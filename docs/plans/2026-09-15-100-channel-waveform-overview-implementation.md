# 100 路模拟波形总览 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 将单路 Qt 波形 Demo 改造成可同屏监测 100 路、点击查看详情并能平滑刷新的一体化桌面界面。

**Architecture:** 把通道缓存与统计计算从绘制逻辑中分离成轻量数据模型；使用一个自绘总览控件绘制 10×10 矩阵，另一个自绘详情控件显示选中通道。主窗口每 20 ms 为全部通道批量写入模拟数据，并一次性刷新界面。

**Tech Stack:** C++17、Qt 6 Core、Qt 6 Widgets、Qt 6 Test、CMake/CTest。

---

### Task 1: 建立 100 路固定长度数据模型

**Files:**
- Create: `waveformmodel.h`
- Create: `waveformmodel.cpp`
- Create: `tests/test_waveformmodel.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing test**

新增 Qt Test，用例覆盖：

```cpp
void WaveformModelTest::initializesOneHundredChannels()
{
    WaveformModel model(100, 800);
    QCOMPARE(model.channelCount(), 100);
    QCOMPARE(model.samples(0).size(), 0);
}

void WaveformModelTest::capsEachChannelBuffer()
{
    WaveformModel model(100, 3);
    model.appendSamples(0, {1.0, 2.0, 3.0, 4.0});
    QCOMPARE(model.samples(0), QVector<double>({2.0, 3.0, 4.0}));
}

void WaveformModelTest::calculatesPeakAndRms()
{
    WaveformModel model(100, 8);
    model.appendSamples(7, {3.0, 4.0});
    QCOMPARE(model.peak(7), 4.0);
    QVERIFY(qAbs(model.rms(7) - qSqrt(12.5)) < 0.0001);
}
```

**Step 2: Run test to verify it fails**

Run:

```powershell
cmake -S . -B build/tests -DBUILD_TESTING=ON
cmake --build build/tests --config Debug
ctest --test-dir build/tests -C Debug --output-on-failure
```

Expected: FAIL，因为 `WaveformModel` 尚不存在。

**Step 3: Write minimal implementation**

实现 `WaveformModel`：

```cpp
class WaveformModel
{
public:
    explicit WaveformModel(int channelCount = 100, int maxPoints = 800);
    int channelCount() const;
    const QVector<double> &samples(int channel) const;
    void appendSamples(int channel, const QVector<double> &values);
    double peak(int channel) const;
    double rms(int channel) const;

private:
    QVector<QVector<double>> m_channels;
    int m_maxPoints;
};
```

无效通道索引返回空数据或忽略写入；追加后只保留最新 `m_maxPoints` 个点。

**Step 4: Run test to verify it passes**

Run the same configure/build/CTest commands.

Expected: all `WaveformModelTest` cases PASS。

**Step 5: Version checkpoint**

当前目录不是 Git 仓库，跳过提交；若用户后续初始化 Git，再提交 `waveformmodel.*`、测试与 CMake 变更。

### Task 2: 实现总览矩阵、点击选择与详情绘制

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Create: `tests/test_waveformwidgets.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing test**

为绘制控件的可测试行为增加用例：

```cpp
void WaveformWidgetsTest::mapsPointToChannel()
{
    OverviewWidget overview;
    overview.resize(1000, 1000);
    QCOMPARE(overview.channelAt(QPoint(50, 50)), 0);
    QCOMPARE(overview.channelAt(QPoint(950, 950)), 99);
}

void WaveformWidgetsTest::ignoresPointsOutsideGrid()
{
    OverviewWidget overview;
    overview.resize(1000, 1000);
    QCOMPARE(overview.channelAt(QPoint(-1, 20)), -1);
}
```

**Step 2: Run test to verify it fails**

Run the build and CTest commands from Task 1.

Expected: FAIL，因为 `OverviewWidget` 和 `channelAt()` 尚不存在。

**Step 3: Write minimal implementation**

- 将原 `WaveformWidget` 替换为 `OverviewWidget` 与 `DetailWaveformWidget`。
- 两个控件仅保存 `WaveformModel` 指针，不复制采样数据。
- `OverviewWidget::paintEvent()` 计算 10×10 单元格，在每格绘制标题、状态点、零线和按像素抽取后的波形。
- `mousePressEvent()` 调用 `channelAt()`，更新选中索引并发出 `channelSelected(int)`。
- `DetailWaveformWidget::paintEvent()` 绘制网格、时间轴提示、幅值提示及选中通道波形。
- 使用深色背景、青绿色正常波形、橙红色告警波形、蓝色选中边框。

关键接口：

```cpp
class OverviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit OverviewWidget(WaveformModel *model, QWidget *parent = nullptr);
    int channelAt(const QPoint &point) const;
    void setSelectedChannel(int channel);
signals:
    void channelSelected(int channel);
};

class DetailWaveformWidget : public QWidget
{
public:
    explicit DetailWaveformWidget(WaveformModel *model, QWidget *parent = nullptr);
    void setChannel(int channel);
};
```

**Step 4: Run test to verify it passes**

Run the build and CTest commands.

Expected: model and widget tests PASS。

**Step 5: Version checkpoint**

当前无 Git 仓库，记录检查点但不提交。

### Task 3: 重构主窗口并接入 100 路批量模拟数据

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Create: `tests/test_mainwindow.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing test**

为主窗口中的关键控件设置稳定 `objectName`，测试结构与状态：

```cpp
void MainWindowTest::containsOverviewAndDetailPanels()
{
    MainWindow window;
    QVERIFY(window.findChild<OverviewWidget *>("overviewWidget"));
    QVERIFY(window.findChild<DetailWaveformWidget *>("detailWaveformWidget"));
    QCOMPARE(window.model()->channelCount(), 100);
}

void MainWindowTest::oneTickUpdatesEveryChannel()
{
    MainWindow window;
    window.generateData();
    for (int channel = 0; channel < 100; ++channel)
        QCOMPARE(window.model()->samples(channel).size(), 20);
}
```

测试通过友元测试类或最小只读访问器访问模型，生产代码不暴露可变数据。

**Step 2: Run test to verify it fails**

Run the build and CTest commands.

Expected: FAIL，因为新布局、100 路批量更新和测试访问接口尚未实现。

**Step 3: Write minimal implementation**

- 主窗口默认尺寸约 1500×900，并设置合理最小尺寸。
- 使用三栏布局：左侧 220 px 控制栏、中间可伸缩总览、右侧约 360 px 详情栏。
- 初始化 `WaveformModel(100, sampleRate * displaySeconds)`。
- 每次定时触发，为每个通道生成 `sampleRate / 50` 个采样点，再统一更新两个绘制控件。
- 信号参数由通道号确定，确保可重复的频率、相位和幅值差异；噪声仍使用随机值。
- 选择通道时更新详情控件及编号、峰值、RMS、采样率、状态标签。
- 开始/停止状态、按钮可用性和暂停保留画面沿用现有行为。
- 样式集中在主窗口级 Qt Style Sheet 中，不引入资源文件或主题抽象层。

**Step 4: Run test to verify it passes**

Run the build and CTest commands.

Expected: all tests PASS。

**Step 5: Version checkpoint**

当前无 Git 仓库，记录检查点但不提交。

### Task 4: 构建、运行与视觉验收

**Files:**
- Modify only if verification exposes a defect: `mainwindow.cpp`, `mainwindow.h`, `waveformmodel.cpp`, `waveformmodel.h`

**Step 1: Run the complete automated suite**

```powershell
cmake -S . -B build/final -DBUILD_TESTING=ON
cmake --build build/final --config Debug
ctest --test-dir build/final -C Debug --output-on-failure
```

Expected: configure/build succeed and all tests PASS。

**Step 2: Launch the application**

```powershell
build\final\Debug\Seismic_Waveforms_demo.exe
```

若生成器是单配置，则运行 `build\final\Seismic_Waveforms_demo.exe`。

**Step 3: Verify visual behavior**

- 100 个通道单元完整显示且编号为 CH-001 至 CH-100。
- 总览波形不相同，网格与文字在深色背景下清晰。
- 点击左上、中部、右下通道，详情栏对应切换。
- 开始后统计值更新，停止后画面冻结。
- 在 1000 Hz 下持续运行至少 30 秒，窗口拖动和通道选择保持响应。

**Step 4: Re-run tests after any visual correction**

Run the complete suite again.

Expected: all tests remain PASS。

**Step 5: Version checkpoint**

当前无 Git 仓库，无法提交最终版本；向用户交付变更清单、测试结果及可执行文件路径。
