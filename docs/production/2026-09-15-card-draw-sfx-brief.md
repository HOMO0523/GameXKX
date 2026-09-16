# 补充第15类：抽牌音效需求

用户指出总览缺少抽牌音效。本次新增`15_抽牌_v001`，更新总览、总表、目录清单和`音效需求_15类_完整包.zip`。既有14类目录与编号保持。当前共15类需求、23条去重参考WAV、112秒演示。

## 范围与规则

当前`EGameXXKSfxCue`包含出牌`CardPlay`，不包含独立抽牌触发；手牌已具有`HandMotions`/`GameXXKCardVisualEffects::Deal`入场动画。此次补的是需求与参考配音，未修改C++、导入游戏音频或启动UE。当前游戏接入库仍是原14类22条素材。

抽牌定义为新卡进入手牌，区别于出牌提交。采用轻短翻纸/滑入方向，建议0.08–0.20秒；单张一次，同批多张合并一次，控制密集抽牌的响度。起手、回合补牌和技能抽牌复用；重排、悬停、界面刷新及未实际入手的操作不触发。后续接入应依据真实入手事件，不能仅凭控件重建重播。

## 视频与参考

画面复用`EssentialSfxRequirements-20260914/defeat02-raw.mp4`中两次真实回合补牌。原始帧397与762首次出现新卡，已查看前后帧核对；分别从337、702帧开始截取150帧。最终视频5秒、60fps、300帧，入手落点为F0060/1.000秒与F0210/3.500秒。

声轨仅为抽牌参考配音，其他战斗声音静音。页面、视频和表格均明确“新增/待接入”，不把它称作游戏原有效果。

参考`SFX_CardDraw_ref_v01.wav`来自[Kenney RPG Audio](https://kenney.nl/assets/rpg-audio)的`bookFlip1.ogg`。发布页列明CC0，下载压缩包SHA256与既有来源记录一致。以浮点解码后截取0.16秒、调整增益和淡入淡出，输出48kHz单声道16-bit WAV。原始OGG、授权文本和处理记录随包提供；可维护来源另存于`SourceArt/Audio/References/CardDraw/`，未混入已接入音频清单。

## 验证

- 视频300帧，参考配音/无声版编码视频流相同。
- 成片声轨与参考PCM混音相关度0.9997；入手帧标注与实际画面一致。
- 新页通过离线播放、定位帧、试听、24-bit导入、对应两处落点试配、导出并单独重新打开检查。
- 总览15项及相对链接有效，23条参考和完整包283个文件的哈希/CRC核对通过。
- 交付HTML没有测试候选或备注；新音效尚未进入游戏，最终音色仍待人工听审。

证据：`Saved/Codex/CardDrawRequirement-20260915/`；浏览器记录为`Saved/Codex/EssentialSfxRequirements-20260914/browser-review/checks-CardDraw.json`。

生成入口：`scripts/build_card_draw_requirement.py`，随后运行`build_essential_sfx_html.py --cue CardDraw`与`build_sfx_requirement_index.py`。总览统计已改为按实际类别数量生成。
