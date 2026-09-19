# 完整帧明细性能测量

日期：2026-09-19。Windows 11 / MSVC 2022 / Qt 6.8.3 / Release，offscreen插件。数据为固定规则的**合成帧**：16.666667ms间隔，每60条有一条70ms。不是百万条真实采集。

[原始CSV](../results/frame-analysis.csv) · [测量代码](../../tools/frame_benchmark/main.cpp)

每个规模预热1轮后测3轮，同一进程。analysis包含全范围统计、历史长帧判断、Duration降序排序、ID映射、热力图。model_reset仅计模型重置；first_table_paint计1000×260表格grab完成，不包含分析，也不包含OS输入排队、合成器或屏幕刷新。每轮检查完整行数和首行70ms，防止用截断换性能。

| 合成帧数 | 分析中位数ms | 模型重置中位数ms | 首次离屏绘制中位数ms | 进程累计峰值MiB |
|---|---:|---:|---:|---:|
| 100,000 | 185.489 | 0.2288 | 1.3583 | 35.715 |
| 1,000,000 | 764.882 | 1.8442 | 1.4415 | 166.438 |

内存为Windows PeakWorkingSetSize，含Qt、快照及临时数据，是同进程累计高水位，不是单模块占用或独立场景峰值。3次样本不能代表稳定P95；本次没有旧版百万行表格作对照，不声称优化倍率。后台分析仍有约0.8秒等待，后续可测取消响应与排序密集输入，确定是否需要复用统计和热力图。

复现（先按README构建Release）：

```powershell
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $PWD '.tools/Qt/6.8.3/msvc2022_64/plugins/platforms'
./build/Release/gpuview_frame_benchmark.exe data/samples/presentmon-real.csv artifacts/analysis-check benchmarks/results/frame-analysis-repro.csv -platform offscreen
```

工具同时用公开905帧样本生成summary.csv、report.md、frames.csv到指定目录。该部分是报告完整性验证，不计入百万帧性能数字。外部CSV解析验证905个唯一ID、时长降序、最大92.3161ms、SHA-256与文件一致、多行含引号备注保真。
