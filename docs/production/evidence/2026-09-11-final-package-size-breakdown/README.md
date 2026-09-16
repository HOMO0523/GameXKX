# 2026-09-11 final 试玩包 · 体积构成证据

本目录是 `docs/production/2026-09-11-final-package-size-breakdown.md` 的配套数据。

放在 `evidence/` 下是刻意的：`scripts/harness_state_validator.py` 会把 `docs/production/` 下
**除 `evidence` 外的任何子目录**当作生产单元，要求其中存在 `00-raw-input.md` …
`07-review.md` 八个文件。数据目录不是生产单元，因此必须放在这里。

## 被记录的产物

| 项 | 值 |
|---|---|
| 包 | `Packaged\GameXXK-2D-Shipping-F10-20260911-final.zip` |
| SHA-256 | `ECE55024824AD6C88C70DB230271B25650F23C0CE0137DEB0572615145915D82` |
| 大小 | ZIP 382.24 MiB / 400.8 MB，解压 509.46 MiB，37 个文件 |
| 基线 | 645.15 MiB → 382.24 MiB（−262.91 MiB，−41%） |
| 源容器清单 | `Saved\Diagnostics\ucas-final.txt`（`UnrealPak -List` 导出） |

## 文件

| 文件 | 行数 | 内容 |
|---|---:|---|
| `01_包体文件构成.csv` | 38 | 37 个文件的解压/压缩后字节与占比 |
| `02_内容目录_二级.csv` | 183 | `.ucas` 按二级目录 |
| `03_内容目录_三级.csv` | 324 | `.ucas` 按三级目录 |
| `04_资产类别构成.csv` | 13 | `.uasset` 按内容类别汇总 |
| `05_包内资产全清单.csv` | 8512 | 包内每个资产的完整清单 |
| `06_卡牌资产明细.csv` | 50 | 全部卡牌相关资产 |
| `07_装备获取节奏_地狱讨伐箱.csv` | 11 | 各品质首次获得/稳态间隔/累计 9 合 1 点击 |

全部为 **UTF-8 带 BOM**，Excel 直接打开不乱码。

## 重新生成

```powershell
# 1) 从容器导出清单（换包后必做）
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealPak.exe' `
  'Packaged\<包目录>\Windows\GameXXK\Content\Paks\GameXXK-Windows.utoc' -List |
  Out-File 'Saved\Diagnostics\ucas-final.txt' -Encoding utf8

# 2) 重出 CSV
python Saved\Balance\20260911-size\_export_csv.py
```

脚本内的常量（包路径、清单路径、输出目录）在文件顶部，换包时一并修改。
