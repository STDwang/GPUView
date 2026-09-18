# 真实PresentMon帧样本

采集日期：2026-09-18。目标为本项目 `tools/capture_target/main.cpp` 的D3D11窗口，持续清屏与Present；每60帧主动Sleep(70)，用于产生可定位的长帧。数据来自真实ETW采集，但工作负载是人为构造的，不代表游戏或GPU瓶颈。

工具：[PresentMon 1.9.2 官方发布](https://github.com/GameTechDev/PresentMon/releases/tag/v1.9.2)。可执行文件SHA256：`EE4EBA300A521B16291B7ACE14D95EFB9FA149B0EF1D31B15DF4EE345C4E002B`。

命令模板（PID使用刚启动的目标进程）：

```powershell
PresentMon-1.9.2-x64.exe -process_id <目标PID> -output_file data/raw/presentmon-target.csv -timed 8 -terminate_after_timed -no_top -session_name GPUViewOwnTarget
```

采集成功；非管理员模式提示部分其他账户/短命进程无法查询，本次按明确目标PID过滤，输出进程名匹配目标。原始采集仅保存在被忽略的data/raw目录。

公开CSV保留全部记录的TimeInSeconds、MsBetweenPresents、Dropped以及固定目标程序名；PID替换为1，交换链地址替换为chain-1，删除其他未使用列。没有修改时间或时长，不把匿名化文件称为原始采集。MsBetweenPresents是相邻Present调用间隔，不是Kernel、SM忙碌或实际屏幕显示FPS。

记录数：905。公开文件SHA256（UTF-8、LF换行）：`4a5c737351ccef5860294b911f18f01cbf39320dbf3af6945c5d62003b0b4057`。
