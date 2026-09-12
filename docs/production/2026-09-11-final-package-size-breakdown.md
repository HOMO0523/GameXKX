# GameXXK 试玩包体积构成（2026-09-11 final）

成品：`Packaged\GameXXK-2D-Shipping-F10-20260911-final.zip`
SHA-256 `ECE55024824AD6C88C70DB230271B25650F23C0CE0137DEB0572615145915D82`
ZIP 382.24 MiB / 400.8 MB，解压 509.46 MiB，37 个文件。

初始基线 645.15 MiB -> 382.24 MiB（-262.91 MiB, -41%）。本轮已移除 NNEDenoiser（98.4 MB
模型 + DirectML/onnxruntime）、94 项代码路径验证的死资源（73.1 MB）、Lumen/光追/Nanite/VSM
等 2D 用不到的着色器（66.3 MB），并去掉 vc_redist 安装器与 launcher stub，改为静态 CRT 的
`GameXXK.exe` 作唯一入口 + 应用目录运行库。

生成：`python Saved/Balance/20260911-size/_final_tables.py`

### 表 1：382.24 MiB 下载包的逐文件构成（37 个文件）

| # | 文件 | 解压 MiB | 压缩后 MiB | 占下载 |
|---:|---|---:|---:|---:|
| 1 | `Windows/GameXXK/Content/Paks/GameXXK-Windows.ucas` | 281.98 | 273.00 | 71.42% |
| 2 | `Windows/GameXXK/Binaries/Win64/GameXXKDev-Win64-Shipping.exe` | 163.56 | 65.25 | 17.07% |
| 3 | `Windows/GameXXK/Content/Paks/GameXXK-Windows.pak` | 33.62 | 32.32 | 8.46% |
| 4 | `Windows/Engine/Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/GFSDK_Aftermath_Lib.x64.dll` | 5.53 | 2.05 | 0.54% |
| 5 | `Windows/GameXXK/Binaries/Win64/D3D12/x64/d3d12SDKLayers.dll` | 4.71 | 1.88 | 0.49% |
| 6 | `Windows/GameXXK/Binaries/Win64/D3D12/x64/D3D12Core.dll` | 4.61 | 1.76 | 0.46% |
| 7 | `Windows/GameXXK/Content/Paks/global.ucas` | 3.31 | 1.30 | 0.34% |
| 8 | `Windows/Engine/Binaries/ThirdParty/MsQuic/v220/win64/msquic.dll` | 2.43 | 1.19 | 0.31% |
| 9 | `Windows/Engine/Binaries/ThirdParty/DbgHelp/dbghelp.dll` | 2.13 | 0.94 | 0.25% |
| 10 | `Windows/GameXXK/Content/Paks/GameXXK-Windows.utoc` | 1.05 | 0.50 | 0.13% |
| 11 | `Windows/Engine/Binaries/ThirdParty/Windows/XAudio2_9/x64/xaudio2_9redist.dll` | 0.82 | 0.41 | 0.11% |
| 12 | `GameXXK.exe` | 0.30 | 0.24 | 0.06% |
| 13 | `Windows/Engine/Binaries/ThirdParty/Vorbis/Win64/VS2015/libvorbis_64.dll` | 1.64 | 0.24 | 0.06% |
| 14 | `Windows/GameXXK/Binaries/Win64/msvcp140.dll` | 0.54 | 0.18 | 0.05% |
| 15 | `Windows/GameXXK/Binaries/Win64/concrt140.dll` | 0.32 | 0.14 | 0.04% |
| 16 | `Windows/Engine/Content/Renderer/TessellationTable.bin` | 0.25 | 0.12 | 0.03% |
| 17 | `Windows/GameXXK/Binaries/Win64/vccorlib140.dll` | 0.34 | 0.11 | 0.03% |
| 18 | `Windows/GameXXK/Binaries/Win64/msvcp140_2.dll` | 0.28 | 0.11 | 0.03% |
| 19 | `Windows/GameXXK/Binaries/Win64/tbb12.dll` | 0.33 | 0.10 | 0.03% |
| 20 | `Windows/GameXXK/Binaries/Win64/vcruntime140.dll` | 0.13 | 0.06 | 0.02% |
| 21 | `Windows/Manifest_UFSFiles_Win64.txt` | 1.12 | 0.06 | 0.01% |
| 22 | `Windows/GameXXK/Binaries/Win64/tbbmalloc.dll` | 0.11 | 0.05 | 0.01% |
| 23 | `Windows/GameXXK/Binaries/Win64/msvcp140_atomic_wait.dll` | 0.06 | 0.03 | 0.01% |
| 24 | `Windows/GameXXK/Binaries/Win64/vcruntime140_1.dll` | 0.06 | 0.03 | 0.01% |
| 25 | `Windows/GameXXK/Binaries/Win64/vcruntime140_threads.dll` | 0.05 | 0.03 | 0.01% |
| 26 | `Windows/Engine/Binaries/ThirdParty/Vorbis/Win64/VS2015/libvorbisfile_64.dll` | 0.05 | 0.03 | 0.01% |
| 27 | `Windows/GameXXK/Binaries/Win64/msvcp140_1.dll` | 0.04 | 0.02 | 0.01% |
| 28 | `Windows/GameXXK/Binaries/Win64/msvcp140_codecvt_ids.dll` | 0.04 | 0.02 | 0.01% |
| 29 | `Windows/Engine/Binaries/ThirdParty/Ogg/Win64/VS2015/libogg_64.dll` | 0.06 | 0.02 | 0.01% |
| 30 | `DevTools/README.md` | 0.01 | 0.00 | 0.00% |
| 31 | `DevTools/gamexxk_dev_client.py` | 0.01 | 0.00 | 0.00% |
| 32 | `README.txt` | 0.00 | 0.00 | 0.00% |
| 33 | `Windows/Manifest_NonUFSFiles_Win64.txt` | 0.00 | 0.00 | 0.00% |
| 34 | `Windows/GameXXK/Content/Paks/global.utoc` | 0.00 | 0.00 | 0.00% |
| 35 | `Windows/NOTICES.txt` | 0.00 | 0.00 | 0.00% |
| 36 | `Windows/Engine/Content/Slate/Cursor/invisible.cur` | 0.00 | 0.00 | 0.00% |
| 37 | `Windows/Engine/Config/StagedBuild_GameXXK.ini` | 0.00 | 0.00 | 0.00% |
| | **合计** | **509.46** | **382.24** | **100%** |

### 表 2：按角色归并

| 角色 | 文件数 | 解压 MiB | 压缩后 MiB | 占下载 |
|---|---:|---:|---:|---:|
| 游戏内容容器 `.ucas` | 1 | 281.98 | 273.00 | 71.42% |
| 主程序 EXE | 1 | 163.56 | 65.25 | 17.07% |
| 字体/引擎资源 `.pak` | 1 | 33.62 | 32.32 | 8.46% |
| 索引 `global.ucas` / `.utoc` | 3 | 4.36 | 1.81 | 0.47% |
| 第三方/引擎 DLL | 21 | 24.26 | 9.43 | 2.47% |
| 启动器 `GameXXK.exe` | 1 | 0.30 | 0.24 | 0.06% |
| 清单与文档 | 7 | 1.14 | 0.07 | 0.02% |
| 其他 | 2 | 0.25 | 0.12 | 0.03% |

### 表 3：`.ucas` 内容按二级目录

| 目录 | 文件数 | MB | 占 .ucas |
|---|---:|---:|---:|
| `GameXXK/UI` | 638 | 122.1 | 43.7% |
| `GameXXK/BattleAnimations` | 4250 | 70.7 | 25.3% |
| `<ShaderCode>` | 1200 | 47.4 | 17.0% |
| `GameXXK/Sprites` | 30 | 11.8 | 4.2% |
| `GameXXK/Characters` | 1761 | 8.7 | 3.1% |
| `Engine/Plugins` | 235 | 3.7 | 1.3% |
| `GameXXK/Cinematics` | 4 | 2.9 | 1.0% |
| `GameXXK/Narrative` | 15 | 2.4 | 0.9% |
| `EngineMaterials/FastBlueNoise_vec2_128x128x64.uasset` | 1 | 2.0 | 0.7% |
| `1Game/Texture` | 41 | 1.4 | 0.5% |
| `EngineMaterials/Substrate` | 4 | 1.3 | 0.5% |
| `EngineMaterials/FastBlueNoise_scalar_128x128x64.uasset` | 1 | 1.0 | 0.4% |
| `VREditor/TransformGizmo` | 17 | 0.5 | 0.2% |
| `EngineMaterials/DefaultDiffuse.ubulk` | 1 | 0.4 | 0.2% |
| `EngineMaterials/Hair` | 3 | 0.3 | 0.1% |
| `EngineMaterials/DefaultBloomKernel.uasset` | 1 | 0.3 | 0.1% |
| `<ContainerHeader>` | 1 | 0.3 | 0.1% |
| `ShaderArchive-Global-PCD3D_SM6-PCD3D_SM6.ushaderbytecode` | 1 | 0.1 | 0.1% |
| `GameXXK/Audio` | 44 | 0.1 | 0.0% |
| `EngineMaterials/EnergyConservation` | 6 | 0.1 | 0.0% |
| `EngineMaterials/FastBlueNoise_scalar_128x128x8.uasset` | 1 | 0.1 | 0.0% |
| `ShaderArchive-Global-PCD3D_SM5-PCD3D_SM5.ushaderbytecode` | 1 | 0.1 | 0.0% |
| `EngineMaterials/DefaultCalibrationGrayscale.uasset` | 1 | 0.1 | 0.0% |
| `EditorMeshes/Camera` | 6 | 0.1 | 0.0% |
| `EditorMaterials/Dataflow` | 5 | 0.1 | 0.0% |
| **合计** | **8512** | **279.3** | **100%** |

### 表 4：`UI` 子目录（122.1 MB）

| 子目录 | 文件数 | MB | 占 UI |
|---|---:|---:|---:|
| `StoryNodes` | 62 | 42.6 | 34.9% |
| `Battle` | 97 | 27.0 | 22.1% |
| `Town` | 93 | 12.7 | 10.4% |
| `Items` | 56 | 6.3 | 5.1% |
| `Relics` | 46 | 4.8 | 4.0% |
| `MasterV2` | 53 | 4.0 | 3.3% |
| `Equipment` | 36 | 3.9 | 3.2% |
| `QuestDialog` | 3 | 3.5 | 2.9% |
| `Maps` | 6 | 3.1 | 2.6% |
| `MainMenu` | 2 | 2.4 | 2.0% |
| `Inventory` | 18 | 2.2 | 1.8% |
| `Tasks` | 13 | 1.8 | 1.5% |
| `RouteMap` | 6 | 1.5 | 1.2% |
| `StoryPortraits` | 12 | 1.4 | 1.2% |
| `PartyDeck` | 19 | 1.0 | 0.8% |
| `Talents` | 11 | 0.8 | 0.6% |
| `Training` | 10 | 0.7 | 0.6% |
| `RouteCamp` | 1 | 0.6 | 0.5% |
| `StarterEquipment` | 6 | 0.6 | 0.5% |
| `ImageTruth` | 8 | 0.5 | 0.4% |
| `DesktopOverlay` | 3 | 0.2 | 0.2% |
| `Cards` | 1 | 0.2 | 0.1% |
| `Materials` | 68 | 0.1 | 0.1% |
| `MetaShop` | 1 | 0.1 | 0.0% |
| `Controls` | 1 | 0.0 | 0.0% |
| `Fonts` | 6 | 0.0 | 0.0% |
| **合计** | **638** | **122.1** | **100%** |

### 表 5：`.uasset` 按内容类别（7230 个 / 229.3 MB）

| 类别 | 个数 | MB | 占 uasset |
|---|---:|---:|---:|
| 战斗动画图集 | 98 | 66.42 | 29.0% |
| 剧情插画 | 62 | 42.60 | 18.6% |
| 其他 UI | 362 | 37.22 | 16.2% |
| 战斗 UI / 特效 | 76 | 26.22 | 11.4% |
| 角色 / 精灵图 | 1792 | 20.48 | 8.9% |
| 城镇 UI | 92 | 12.52 | 5.5% |
| 引擎自带资源 | 463 | 9.74 | 4.2% |
| 其他（蓝图 / 数据 / 元数据） | 4183 | 5.23 | 2.3% |
| 过场 | 4 | 2.89 | 1.3% |
| 剧情物品 | 14 | 2.39 | 1.0% |
| 卡牌卡面 / 卡框 | 50 | 2.10 | 0.9% |
| 剧情立绘 | 12 | 1.44 | 0.6% |
| 音频 | 22 | 0.01 | 0.0% |
| **合计** | **7230** | **229.3** | **100%** |
