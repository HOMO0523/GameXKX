# GameXXK 正文字体：荆南圆体（KeinannMaruPOP）

2026-09-12 用户决定：**除标题以外的文本**使用荆南圆体；标题继续用现有江湖古风体。用这款替换同日的芝士奶盖乌龙宋（用户反馈"有点细"）。

- 源文件：`fonts/KeinannMaruPOP.ttf`（原始文件名 `けいなんポップ体.ttf`，8,459,780 字节，sha256 `30c3536a…9427`），未做任何修改或子集化。
- 授权：**SIL Open Font License 1.1**（`licenses/KeinannMaruPOP-Readme.pdf` 内引用 `http://scripts.sil.org/OFL`；猫啃网说明页声明可免费商用）。OFL 允许嵌入并随软件再分发。
- 出身：由 KNBobohei（Kingnam Type Foundry / NightFurySL2001，`github.com/maoken-fonts/KNBobohei`）衍生；保留字体名不得用于修改版。
- 字重：只有 Regular 一个字重；本项目不调用其它字面（加重沿用描边）。
- 覆盖：项目全部中英文语料 1799 个唯一字符**全覆盖，零缺字**（对比：思源回退仍保留配置作为保险）。
- **未采用** `けいなん丸ポップ体JP.ttf`：日文子集缺 542 个项目中文字（专业、丛、东、丝…），不可用于简中界面。
- 导入：`Content/Python/gamexxk_import_body_font.py`（经 UE MCP 执行），目标 `/Game/GameXXK/UI/Fonts/Body/FF_Body_KeinannMaruPOP_Font`。

字体预览、覆盖率与授权依据同时记录在 `font-manifest.json`。
