---
status: record
owner: codex
updated_at: 2026-08-07
source_commit: c5cfe2e7508fa38273041ed415f1fa096e4b7ad3
---
# GameXXK 城镇 UI 性能与内存基线（2026-08-04）

## 记录目的

本文件保存城镇 HUD 优化前的实际 PIE 性能与内存证据，供 HUD 图片、样式和 PSD-to-UMG 流程完成后做前后对比。

当前阶段只记录，不实施性能优化。未经后续单独确认，不调整控件生命周期、纹理尺寸、压缩格式、流送设置、3D 场景或 Texture Pool。

## 测试环境与场景

- 项目：`D:/UE5 demo/GameXXK/GameXXK.uproject`
- 引擎：UE 5.8 Editor
- PIE 地图：`/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`
- 运行界面：城镇 HUD
- 机器物理内存：15.74 GiB
- 测试方式：UE 处于 Windows 前台时，使用 UE CSV Profiler 连续采样 12 秒

## 前台帧时基线

原始 CSV：

`D:/UE5 demo/GameXXK/Saved/Profiling/CSV/Profile(20260804_110320).csv`

| 指标 | 结果 |
|---|---:|
| 采样帧数 | 721 |
| 平均帧率 | 59.9726 FPS |
| 平均帧时 | 16.6743 ms |
| 中位帧时 | 16.6670 ms |
| P95 帧时 | 16.6818 ms |
| P99 帧时 | 16.7036 ms |
| 超过 50 ms 的卡顿帧 | 0 |
| Game Thread 中位 | 8.0657 ms |
| Render Thread 中位 | 7.8020 ms |
| GPU 中位 | 8.5356 ms |
| RHI Thread 中位 | 3.3838 ms |
| Draw Calls 中位 | 2610 |
| GPU 本地显存使用中位 | 3796 MB |
| UE PhysicalUsedMB 中位 | 2348.625 MB |

结论：当前城镇 PIE 在编辑器前台时稳定维持约 60 FPS，没有帧时卡顿。此前编辑器标题栏出现的 8-10 FPS 属于 UE 失去前台焦点后的编辑器节流，不能作为实际游戏性能结论。

## 系统内存基线

采样时 Windows 状态：

| 指标 | 结果 |
|---|---:|
| 总物理内存 | 15.74 GiB |
| 已用物理内存 | 13.27 GiB |
| 可用物理内存 | 2.47 GiB |
| UnrealEditor 工作集 | 2328.6 MiB |
| UnrealEditor Private Bytes | 8867.0 MiB |
| Memory Compression 工作集 | 1146.3 MiB |
| ChatGPT 主进程工作集 | 1428.1 MiB |
| Codex 工作集 | 887.1 MiB |

说明：UnrealEditor 的 Private Bytes 是提交空间，不等于实际占用了同等数量的物理内存。系统只有约 2.47 GiB 可用内存且内存压缩超过 1 GiB，说明整机确实处于内存压力状态；压力来源不只包括 UE。

## UE MemReport 基线

原始 MemReport：

`D:/UE5 demo/GameXXK/Saved/Profiling/MemReports/UEDPIE_0_L_Qingshan_AsianVillage_Demo-WindowsEditor-08.04-10.57.00/Pid25020_UEDPIE_0_L_Qingshan_AsianVillage_Demo-WindowsEditor-04-10.57.00.memreport`

主要数据：

| 指标 | 结果 |
|---|---:|
| Process Physical Memory | 2764.16 MB |
| Process Physical Peak | 5580.69 MB |
| Texture 2D Memory | 426.520 MB |
| Texture Memory Used | 408.641 MB |
| TEXTUREGROUP_UI | 136.430 MB |
| TEXTUREGROUP_World | 175.961 MB |
| TEXTUREGROUP_WorldNormalMap | 77.199 MB |
| NeverStream | 68.113 MB |
| Unstreamable Render Assets | 294.859 MB |
| Streaming Pool 容量 | 1000 MB |
| Streaming Pool 实际使用中位 | 166.641 MB |
| Texture Pool 超预算 | 0 MB |

结论：Texture Pool 没有超预算，增加 Pool 大小不是当前内存问题的解决方案。UI 与其他不可流送资源位于主要驻留来源之中。

## 当前 UI 驻留结构

城镇 PIE 中检测到的 GameXXK UI 纹理：

| 分类 | 数量 | 当前内存 | 不可流送内存 |
|---|---:|---:|---:|
| 当前城镇 HUD 大背景 | 1 | 28.000 MB | 28.000 MB |
| 其他城镇面板和组件 | 44 | 45.316 MB | 45.316 MB |
| 未显示的世界地图 UI | 6 | 36.938 MB | 36.938 MB |
| 未显示的战斗 UI | 15 | 22.062 MB | 22.062 MB |
| 未显示的任务对话 UI | 3 | 18.750 MB | 18.750 MB |
| 未显示的背包 UI | 13 | 3.062 MB | 3.062 MB |
| 未显示的任务 UI | 1 | 1.312 MB | 1.312 MB |
| 其他 GameXXK UI | 6 | 9.500 MB | 9.500 MB |

当前 HUD 大背景：

- 资源：`T_TownPsd_Background_Hud`
- 尺寸：4096 x 1720
- 格式：`PF_B8G8R8A8`
- LOD Group：`TEXTUREGROUP_UI`
- 当前内存：28 MB
- Streaming：NO
- Mip 数量：1

其他大尺寸城镇背景包括：

- `T_TownPsd_Background_Character`：9.5 MB
- `T_TownPsd_Background_Companion`：9.5 MB
- `T_TownPsd_Background_Backpack`：7.125 MB
- `T_TownPsd_Background_Task`：6.562 MB

## 已确认的结构性原因

`AGameXXKMVPPlayerController::EnsurePlayerFlowWidgets()` 当前会在初始化阶段创建并加入 Viewport 的界面包括：

- Main Menu
- World Map
- Town Overlay
- Town HUD
- Route Map
- Battle Board
- Inventory
- Companion Roster
- Quest Dialog
- Route Encounter
- Route Merchant
- Relic Bar
- Task Panel

隐藏界面主要通过 `Collapsed` 控制显示，PlayerController 继续持有 Widget 强引用，因此对应对象和纹理仍可保持驻留。

代码证据：

`Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp:1105`

## 当前诊断结论

1. 当前城镇前台帧率稳定，HUD 优化不需要以修复即时掉帧为前提。
2. 整机内存压力真实存在，但 UE 不是唯一来源。
3. 项目侧最明确的 UI 内存浪费是所有流程界面被提前创建并保持强引用。
4. 4096 级无压缩、无流送城镇背景对当前实际显示尺寸存在明显过度采样。
5. 纹理池没有超预算，不应通过继续增大 Pool 解决。

## 延期事项

以下事项本阶段明确延期，不随城镇 HUD 图片和样式优化一起实施：

- Player Flow Widget 按需创建与销毁
- 隐藏界面的软引用或异步加载
- 关闭大型面板后的资源释放与受控 GC
- 城镇运行时背景降采样
- 大面积水墨背景压缩格式调整
- UI 纹理流送策略调整
- 世界场景 Draw Call、Lumen 或场景纹理专项优化

## 后续对比要求

城镇 HUD 图片、样式、PSD 拆分、UMG 接入和打包流程完成后，应在同一城镇位置重新执行：

1. 12 秒前台 CSV Profiler 采样。
2. `memreport -full`。
3. 城镇 HUD 截图与交互验收。
4. Development 打包启动与主流程验收。

重新测试时以本文件数据作为优化前基线；性能优化是否启动，由用户另行确认。
