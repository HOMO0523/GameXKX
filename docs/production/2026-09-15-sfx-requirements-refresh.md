# 音效需求网页与文件包重新生成

状态：完成。用户确认更新当前21类音效需求网页和文件包。本次沿用已经核验的视频、音频和单项时间轴，未启动编辑器或变更游戏接入。

- 生成时间：2026-09-15 11:03（Asia/Shanghai）。
- 21个独立HTML、21个单项ZIP、需求总表、目录总览、完整21类ZIP及新增6类ZIP已同步生成。
- 315个原始视频、音频、时间轴及来源文件逐字节保持；生成前的HTML和目录文件备份在 `Saved/Codex/SfxBriefRefresh-20260915-110338/previous-generated/`。
- 188个内嵌素材的SHA检查、所有23份ZIP的CRC及成员内容一致性检查通过。
- 21类网页全部通过离线加载、秒/帧定位、试听、24-bit WAV导入/试配、成品导出回读及窄屏布局检查；脚本错误和外部资源请求均为0。
- 总表22行（含表头）、本地HTTP预览内容与生成文件一致。声音范围仍是21类需求、29条参考音，游戏接入状态保持原来的14类。

生成入口：`python scripts/refresh_sfx_requirement_delivery.py`。该脚本先备份现有生成文件，并保留已存入需求HTML的成品和评审信息，再重建目录和压缩包。

本次生成与验证记录：

- `Saved/Codex/SfxBriefRefresh-20260915-110338/generation.json`
- `Saved/Codex/SfxBriefRefresh-20260915-110338/browser-review/checks.json`
- `Saved/Codex/SfxBriefRefresh-20260915-110338/handoff.json`

预览：`http://127.0.0.1:18815/index.html?revision=20260915110338`。
