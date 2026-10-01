# 界面优化复盘

## 第一轮：密度、状态与搜索入口（2026-09-30）

面试问题：工具功能越来越多，怎样避免布局和反馈失控？

复现场景：原事件搜索把名称、时长、选区和导航挤在同一行；每个页面自行设置样式，禁用、焦点和选中状态难区分。底部面板关闭后，用户必须在菜单中寻找恢复入口。

解决思路：将主题集中到 [theme.cpp](../../src/ui_widgets/theme.cpp)，把搜索/导航与时长条件分成两行；保留高对比滚动条，数字右对齐、交替行和清晰焦点边框帮助阅读。Ctrl+F先恢复Dock再聚焦搜索框，不依赖输入框已经可见。

代码例子：在 [EventExplorer::resetFilters](../../src/ui_widgets/event_explorer.cpp) 中，用四个QSignalBlocker批量恢复输入，最后只调用一次schedule。重置不改用户排序，输入编辑立即取消旧结果，回车则停止防抖立即提交。[MainWindow](../../src/ui_widgets/main_window.cpp) 用窗口级QAction组织恢复和聚焦。

验证证据：[UI回归](../../tests/ui/ui_tests.cpp) 检查重置后名称/上下限/选区和排序、隐藏Dock恢复及焦点；本轮Debug/Release各57个实际用例通过。README中的百万教学/905帧截图来自Release实际运行并人工检查。

额外发现：固定延迟截图可能捕获仍在分析的空表。[应用入口](../../src/app/main.cpp) 改为检查模型发布结果是否属于当前快照，再截图；30秒仍未就绪以非零码退出。等待时间不当作性能数据。

不足：离屏截图不代表真实屏幕流畅度；各DPI、窄屏和屏幕阅读器仍需人工验收。样式表不解决原始数据排序成本，下一轮单独测量和优化计算链路。

## 第二轮：排序复用（Q08 / Q10）

面试问题：只有表头排序变化，为什么还要重新搜索百万事件？缓存如何证明正确？

复现：加载百万教学事件，切换名称汇总并点击“名称”。此前重新进行扫描、聚合、事件排序和ID索引排序，只为改变四个名称组的顺序。

解决思路：在 [canReuseEventSelection / analyzeEvents](../../src/core/event_analysis.cpp) 比较共享快照身份、名称、上下限、范围、轨道，排序不属于筛选键。命中时复制旧匹配结果，只排序变化的表；仅汇总变化时保留事件顺序及ID映射。任何筛选键不一致都冷计算，轨道顺序不同保守失效。两个不同快照即使版本相同也不能复用。

代码例子：分析入口的previous参数是shared_ptr<const EventAnalysis>；copyCancellable每1024项检查取消，写入全新的out，不修改旧对象。[控制器](../../src/application/event_analysis_controller.cpp) 仅缓存最新成功发布结果；cancel(true)给防抖保留单项缓存，普通cancel()和换会话释放；旧代次完成不写缓存。

证据：新增eventSortReuseMatchesColdAndInvalidates逐行比对冷热结果，遍历全部排序列与方向、7项筛选/身份变化；eventControllerReuseAndRelease验证弱引用释放；UI排序用例断言实际命中复用。Debug/Release均通过59个实际用例（Core47+UI12）。[同机基准](../../benchmarks/reports/EVENT_SORT.md)记录原始40条测量。

30秒讲述：我把筛选键和排序键分开，先证明冷计算与复用结果逐行一致，再用同一程序交替测量。百万事件只改名称汇总排序从200.30ms降至7.84ms；事件时长排序没有改善，所以结论只覆盖汇总排序。代价是复制新结果的O(N)内存，不能把单项缓存说成零复制。

## 第三轮：联动导航与可恢复布局

面试问题：多视图联动正确了，为什么使用时还会觉得跳动？

复现：表格切换到当前已经可见的另一条轨道，以前selectEvent仍将它移到首行，用户失去相邻轨道的空间参照。

解决思路：[TimelineWidget::selectEvent](../../src/ui_widgets/timeline_widget.cpp)只在target小于first或大于等于first+page时滚动；向下滚动的首行取target-page+1。选择和高亮仍按源事件ID，不改变时间视口或分析范围。

[MainWindow](../../src/ui_widgets/main_window.cpp)保存初始Dock状态，一键恢复时不重置会话、搜索和窗口尺寸，帧面板是否可用仍取决于数据类型；空闲时隐藏加载进度。轨道过滤只经selectionCleared的统一连接请求统计，去掉重复发起/取消任务。

验证：selectedTrackScrollsOnlyWhenOutsideViewport检查视野内不发滚动通知、向下/上只滚动到边缘；defaultLayoutPreservesDataAndFilters检查移动/隐藏Dock后恢复、源快照身份和5000条Kernel搜索结果保留。Debug/Release通过61个实际用例。布局只保存在窗口内，尚未跨进程记忆个人布局。

## 第四轮：输入合并不等于所有操作延迟（Q04）

面试问题：算法只用了几毫秒，为什么点排序还感到延迟？

复现：原schedule对表头点击和连续打字都等待180ms；在汇总排序降到约8ms后，这段固定等待成为明显的额外成本。

解决思路：[EventExplorer::submitNow](../../src/ui_widgets/event_explorer.cpp)仍先schedule取消旧代次和清空过期行，但立即停止定时器并submit；表头、回车、重置使用它，名称/时长编辑仍合并输入。F3和Shift+F3复用现有navigate，按钮禁用时快捷键也不可激活。上限小于下限在GUI直接中文提示并释放缓存，不启动必然报错的Worker；core仍独立验证，不能以UI校验代替核心边界。

视觉细节：[DurationSpinBox](../../src/ui_widgets/duration_spin_box.cpp)仅补画矢量箭头，按钮矩形取自QStyle，输入/点击/键盘语义仍由QDoubleSpinBox负责；不引入SVG图像插件或逐帧对象。Fusion和统一调色板用于标准控件，样式表显式区分箭头按钮可用/禁用/悬停背景。

验证：searchValidationAndKeyboardNavigation通过实际键盘事件检查非法提示、恢复查询、F3下一项和Shift+F3上一项；本轮最终62个实际用例（Core47+UI15），Release界面截图已目视检查箭头和结果行。没有把移除固定等待宣称为端到端P95；高DPI人工验收仍待进行。

## 第五轮：最小尺寸与字体度量（2026-10-01）

面试问题：窗口设置成800像素高，为什么导入数据后又长高了？单纯按DPI缩放能解决文字重叠吗？

复现：用实际应用中文字体，将窗口设为1200×800再导入905帧夹具。修复前Qt布局的minimumSizeHint为1037×980，窗口实际变为1200×980；中央图表/时间轴要求560高，底部搜索面板要求292高，再加工具栏和Dock等边界。Qt遵守最小尺寸约束，因此直接resize无效。

解决思路：保留时间轴刻度和至少两条完整轨道，将其最小高度改为120；帧图常规建议150高，但允许压至110；统计文本与搜索表格允许缩小并继续滚动。搜索区减少空白边距，不隐藏筛选或截断数据。[FrameTimeWidget::sizeHint](../../src/ui_widgets/frame_time_widget.h)区分“偏好高度”和“硬下限”。最终同场景minimumSizeHint为1037×798，窗口保持1200×800，底部仍容纳两条完整结果行。

第二个问题是固定九个刻度在约400像素的绘图区重叠。[timeAxisTicks](../../src/ui_widgets/time_axis.cpp)使用当前QFontMetrics试排，逐步减少间隔，保证文字不越界且间隔至少10逻辑像素。极窄区域只显示省略后的起点；这只改变标签数量，不改变数据范围、查询或单位。最多九个字符串，工作量与事件数无关。

验证：应用和UI测试共用[字体/主题配置](../../src/ui_widgets/theme.cpp)，避免默认测试字体偏小而漏报问题。frameWorkspaceFitsCompactWindow检查真实帧加载后窗口尺寸、两行结果和图表对齐；timeAxisLabelsFitAvailableWidth覆盖10/16磅字体、40至1000像素宽和三种时间范围。Debug/Release共64个实际用例通过；另用QT_SCALE_FACTOR=1.5运行两项布局测试并[检查1800×1200物理像素截图](../../assets/screenshots/compact-150.png)，逻辑窗口仍为1200×800。

边界：这不代表任意屏幕都能容纳所有Dock，也不替代多显示器实际拖动和系统字体变更测试。较小的可用逻辑空间仍可通过关闭面板腾出空间；尚未实现自动紧凑模式。性能基准没有因刻度改动重新宣称提升。

## 第六轮：统计的信息层次与空状态

面试问题：数值都算对了，为什么用户仍难以找到关键指标？过滤后的空表为什么需要单独设计？

复现：原统计页先展示多行范围和口径说明，再展示计数、总和、均值，P95/P99被挤到初始视口之外。紧凑窗口中，用户必须滚动才能看到尾延迟。

解决思路：[statisticsHtml](../../src/ui_widgets/statistics_view.cpp)从MainWindow中独立，消费已完成统计，按两列展示数量/均值/P95/P99。标题保留全会话/选区及帧/Trace口径；总和、P50、长帧规则和完整分布留在下方，不删除信息。输出只含固定标签和数值，不插入文件路径或用户HTML，也不重算百分位。

空结果显示数量0、均值/百分位N/A及恢复轨道/清除选区提示，不沿用上次有效值。定位最长项的按钮和长帧列表随同一结果更新；不能只修改一行“暂无数据”文字，却继续允许定位旧事件。

验证：compactStatisticsPrioritizeMetricsAndClearEmptyValues使用实际字体和290×120区域，以QTextBrowser.cursorRect验证P99数值完整落在初始视口；检查空状态和Trace不显示FPS。真实905帧导入用例进一步过滤全部轨道，确认列表清空、定位禁用，再恢复轨道验证P99和定位恢复。Debug/Release共65个实际用例通过，帧分析截图已更新检查。

取舍：富文本只描述固定数量指标，不为每个事件创建控件；与Qt Quick学习阶段可以对照排版方式。这里改善的是信息查找成本，没有声称计算提速；更大的系统字体和完整屏幕阅读器体验仍需另行验证。
