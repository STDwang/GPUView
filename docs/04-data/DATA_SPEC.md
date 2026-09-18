# 数据与指标规范

版本：v0.1｜状态：评审稿；字段映射须在开发准备阶段由真实样本冻结

## 当前实现范围（2026-09-18）

PresentMon 1.9.2 x64的实际v1表头已由本机采集核验。适配器要求Application、ProcessID、SwapChainAddress、TimeInSeconds（秒）、MsBetweenPresents（毫秒），列名大小写不敏感。数据和工具哈希见 [采集记录](../../data/samples/PRESENTMON_CAPTURE.md)。其他时间模式/缺必需列明确拒绝；并未实现下文所有可选指标。

- 时间归一化到首个有效Present，原点纳秒写入会话来源。事件ID为CSV逻辑记录序号，带引号跨行不当成多条记录。
- 帧矩形锚定当前Present，宽度编码前一Present间隔，仅作可视化编码；不是有真实GPU起止时间的区间。统计按Present时刻落入[a,b)选取，使用完整来源间隔。
- BOM、CRLF、引号/转义/字段内换行、无序数据、非有限/负数/溢出均处理。结构损坏引号拒绝整个文件；数值或列数损坏排除该记录并报告。既不插值也不按相邻行推导间隔，因此不会跨损坏行补出假长帧。
- 无序有效帧由核心索引排序；来源页保留无序计数和相邻重复时间计数，重复记录不静默去重。PID和交换链按独立字段组成分组键，不靠显示名称拆分。
- 统计实现均值、nearest-rank P50/P95/P99、间隔口径FPS、固定60FPS预算及全组历史的长帧规则。1% Low、目标FPS选择、Dropped筛选、可选GPU指标和完整质量位模型尚未实现。
- 帧图每像素显示最大帧间隔；密集时回退4096个预计算时间桶的最大值并标注概览。统计始终读取原始数据。教学区间统计仍按相交裁剪，不能混用。

以下章节保留完整目标规范；实现边界以此节及README为准。

## 1. 输入支持矩阵

| 输入 | 阶段 | 保证支持的范围 | 不提供的能力 |
|---|---|---|---|
| 本机 PresentMon CSV | A | 经验证的一个表头版本；逐帧时间、进程、交换链和实际存在的指标 | 不推导 Kernel 名、指令、真实调用栈 |
| NVIDIA 遥测 CSV + manifest | B | 本机验证可用的整卡利用率/显存；其他值可缺失 | 不默认等于目标进程资源使用量 |
| 项目教学 Trace JSON | B | 明确限定的 X/C/M 子集，合成来源标识 | 不宣称全面兼容 Chrome Trace 或真实硬件采样 |
| 其他版本 CSV、ETL、Nsight、Perfetto proto | 后续 | 当前报不支持并说明 | 不静默以现有格式读取 |

PresentMon 官方提供 CSV 输出，字段及时间模式受版本/选项影响。因此项目保存工具版本、表头、采集选项和单位映射，不能只记录扩展名。[PresentMon console 文档](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md)

NVIDIA 遥测可提供设备信息和监控数据，但不支持的值要保留为不可用状态；本项目不把整卡数值归因到单个进程。[nvidia-smi 官方说明](https://docs.nvidia.com/deploy/nvidia-smi/index.html)

## 2. 内部逻辑模型

以下为逻辑字段，不要求一比一堆对象存储。

| 类型 | 主要字段 | 语义 |
|---|---|---|
| SessionMetadata | sessionId、schemaVersion、sourceKind、sourceVersion、sha256、synthetic、clockInfo、capabilities | 所有结果的来源依据 |
| FrameSample | id、processKey、swapchainKey、timestampNs、frameTimeNs?、cpuBusyNs?、gpuBusyNs?、displayLatencyNs?、displayed?、quality | 一条帧记录；可选值不能补0 |
| IntervalEvent | id、trackId、startNs、durationNs、nameId、categoryId、parentId?、correlationId? | 有明确起止的区间，start+duration检查溢出 |
| CounterSample | trackId、timestampNs、value?、unit、scope、quality | 标量采样，可能整卡/进程/模拟范围 |
| TrackDescriptor | trackId、name、type、process/thread/device/stream关联、sourceId | 轨道本身的身份与数据能力 |
| Annotation | id、range、reason、ruleVersion、evidenceIds | 长帧等规则标记，不能替代原始数据 |

主时间统一为会话相对起点的 int64 纳秒；展示可切换 ms/s。保留原始时间与单位映射用于追溯。可选值用显式空值/有效位，区分缺失、非法和真实0。来源字符串通过字典驻留，原始扩展字段按需读取或稀疏保存。

## 3. PresentMon 适配与数据清洗

开发准备先采一份短样本，记录可执行文件版本和哈希、实际表头、行数及命令。单横线/双横线参数依工具帮助决定；本机版本不能直接套用当前上游说明。

字段映射配置需包含：字段名及别名、单位、时间基准、有效范围、缺失标记、指标含义。基本帧间隔可使用来源明确的间隔字段；或在同一进程/交换链连续有效时间点间作差。两种来源不得混算，UI须标出选择的口径；如果是相邻时间差，首个样本没有间隔。

CSV 解析处理UTF-8 BOM、带引号字段、转义引号、CRLF和空行；数值按登记格式解析，不受系统小数点区域设置影响。表头未知或必需字段缺失则拒绝；可选列缺失则降级。

损坏行计数并报告行号；若导致时间连续性不可信，下一间隔也标无效，不能跨断点补出一个巨大长帧。非有限值、负时长和溢出拒绝；重复时间记录保留身份并打标。无序数据可稳定排序，但排序前后数量、断点信息必须保留；不能为计算方便抹掉源数据问题。

多交换链分组后单独分析；显示/未显示帧分开标注。默认采用“源中全部有效应用帧间隔”，显示筛选状态；不是默认等同屏幕显示FPS。缺失段与暂停间隔在图上有可见断点。

## 4. 统一统计口径

选区为 [a,b)。帧样本按归一化 timestampNs 落在范围内计入；是否是帧起点或 Present 时刻由适配器元数据注明。统计只含当前过滤后、同一进程/交换链的有效样本。以下 d 为毫秒。

| 指标 | 本项目定义 |
|---|---|
| 有效样本 N | d>0、有限、无断点影响的帧间隔数；另报总记录/缺失/排除数 |
| 平均帧时 | sum(d)/N |
| 平均 FPS（间隔口径） | 1000×N/sum(d)，不取瞬时FPS的算术平均；不代表屏幕实际显示率 |
| P50/P95/P99 | 升序d，nearest-rank：d[ceil(p×N)-1]，p取0.50/0.95/0.99 |
| 1% Low（可选展示） | 取最慢ceil(0.01×N)个间隔的平均值，计算1000/mean；N<100显示样本不足；不等于1000/P99 |
| 长帧 | d>max(2×预算帧时, 最近至多120个有效先前间隔中位数×2)；不足30个历史值只用固定门槛 |
| 预算帧时 | 默认1000/60 ms，可选择30/60/120/144等目标；这是参考预算，不是检测到的实际刷新率 |
| 热力图桶值 | 默认1秒桶内的最大有效帧时；空桶N/A，色标注明max，不能标GPU利用率 |

长帧规则只是异常筛选启发式，不是行业统一卡顿定义。滚动基线用全会话同组有效历史，框选不能改变同一帧是否长帧；改变过滤或目标帧率需重算并记录ruleVersion。阈值等号不计长帧。

手算夹具：帧间隔 [10,10,20,60] ms，N=4，均值25ms，平均40FPS，P50=10ms，P95=P99=60ms。60FPS预算且历史不足时仅60ms判为长帧。1% Low因样本不足显示N/A。

区间Trace聚合：只统计与选区相交的事件。count统计相交事件数，sum/mean/P95使用裁剪到选区内的时长，并明确标签“选区内时长”。并发重叠事件的sum可能超过选区墙钟长度。若展示busy coverage，必须按指定设备/轨道集合合并区间求并集再除以选区长度；它仍不代表SM利用率。计数器不参与区间时长求和。

## 5. 多源时钟与遥测

外部帧 CSV 的相对秒数与遥测墙钟不能直接相减。每个源记录 clockDomain、origin、scale、offset、对齐依据和误差估计。

优先方式是采集端同时记录单调时钟与墙钟锚点，并把采样起止区间写入 manifest；帧源若使用QPC须记录频率/原点转换。已有文件缺少共同锚点时，不自动宣称精确对齐；允许用户手工偏移，并在界面/导出持续标记“人工近似对齐”。无依据则仅并排查看。

遥测建议先1Hz验证，工具与开销允许时再试5Hz（约200ms）；UI20Hz发布不创造20Hz硬件样本。采用同一基准时间，近邻展示须显示样本年龄；超过2个名义采样周期不延长旧值。离线默认不插值，无效值断线。相邻采样间发生的单帧尖峰可能完全看不到。

## 6. 教学 Trace 子集

采用Chrome Trace形状的JSON对象，根字段 traceEvents 为数组。项目约定：X记录必须有name、pid、tid、ts、dur（ts/dur以微秒输入，转纳秒）；C记录必须有数值args；M仅支持process_name/thread_name命名。其他ph统计为unsupported并显示警告，B/E配对、flow、async、stackFrames暂不支持。

轨道根据pid/tid及显式元数据生成；category为项目约定的CPU/Kernel/Memcpy等类别，不能仅凭名字断言是真实GPU事件。synthetic、generatorVersion、seed、expectedSummary写在配套manifest中。嵌套与并发必须支持可视区查询；parentId缺失时不提供调用树。

一百万事件优先用行式项目内部格式或流式解析器生成/导入；不要用整文件QJsonDocument作为无限规模解析策略。正式选JSON流解析依赖前，在环境任务中记录库版本及使用范围。通用格式参考入口：[Chromium Trace Viewer](https://github.com/catapult-project/catapult/blob/master/tracing/README.md)。

## 7. 数据集与可复现性

| 数据集 | 用途 | 来源要求 |
|---|---|---|
| tiny-known | 手算统计、边界、单位、缺失测试 | 人工可核算，几十条 |
| real-short | A端到端真实演示 | 本机可重复场景1～5分钟，来源/版本/参数完整 |
| synthetic-100k | 开发中交互基准 | 固定seed，10万事件 |
| synthetic-1m | B规模验收 | 固定seed，100万事件，至少64轨道含长跨界事件/密集重叠 |
| mixed-clock | 时钟与缺失处理 | 已知偏移、间隙、未知锚点三种 |
| malformed | 鲁棒性 | 引号、无序、重复、缺列、溢出、负时长 |

每个manifest包含schemaVersion、文件hash、来源、工具版本、单位、时钟、场景/时长、数量、synthetic、脱敏状态、生成seed（如适用）。小样本入tests/fixtures，大文件放data；不把重复复制的游戏帧伪装成更长真实录制。
