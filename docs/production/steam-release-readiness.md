---
status: active
owner: codex
updated_at: 2026-09-12T21:20:00+08:00
source_commit: 200c42b
---

# GameXXK Steam 发行准备

核对日期：2026-09-12。状态：准备中，尚未登录 Steamworks、上传或提交审核。
关联任务：先加固本地存档，再接 Steam 测试与云存档；现有 382.24 MiB 试玩打包路线保留。

## 当前事实与待确认信息

- 工作目录为根项目，分支 main；不创建 worktree。
- 现有 Windows ShippingF10 ZIP 为 400,812,836 字节，解压 534,212,456 字节；CRC 通过，无玩家存档。
- 这是 2026-09-11 成品，不代表当前 main 所有最新内容；正式测试须重新构建并记录源码版本及文件哈希。
- 项目没有完整 Steam SDK/账号/成就接入证据，也没有 Steam 后台云同步、审核通过的证据。
- Steamworks 注册状态、发行主体、AppID：待用户确认。不得把密码、证件、税号或银行资料写进仓库。
- 商店名称：霞客行；英文名称：CloudFarer（用户已确认）。发行定价、上线日期、是否先发布 Demo：待后续产品决策。

## 逐步工作表

| 顺序 | 工作 | 用户处理 | 项目侧交付 | 完成标准 |
|---|---|---|---|---|
| 1 | 发行主体与 Steamworks | 确定个人/公司，登录注册，实名、协议、税务、银行资料、付款 | 解释页面字段与所需材料，不代填不确定的法律/财务事实 | 账户资料完成，取得本游戏 AppID |
| 2 | 发行范围 | 确认名称、语言、平台、售价方向、Demo/正式版计划 | 整理真实可玩内容和功能清单 | 商店承诺与版本相符 |
| 3 | 素材与内容调查 | 核对自有/购买素材权益，确认内容披露 | 来源授权清单、AI 使用说明草案、分级内容盘点 | 已发布内容可追溯，调查如实填写 |
| 4 | 商店页面 | 审阅文案与图片，决定公开时间 | 中英文简介、详细介绍、胶囊图、截图、宣传视频和系统需求草案 | 后台检查项完整，商店审核通过后公开 Coming Soon |
| 5 | 本地存档加固 | 保留自己的正常试玩进度 | 旧档兼容、主档/检查点选择、周期保存、故障恢复测试 | 冷编译及独立存档行为检查通过；玩家档案未被测试覆盖 |
| 6 | Steam 测试包 | 提供非敏感 AppID/DepotID；本人登录发布工具 | SteamPipe 配置、明确启动入口、运行库、测试分支、构建清单 | Steam 安装/启动/退出/更新通过 |
| 7 | Steam 功能 | 确认首发是否包含云存档、成就、手柄支持 | 账号档案隔离、云存档白名单、必要 SDK 功能、兼容测试 | 两机交替、离线再联网、切账号、窗口焦点与覆盖界面验证 |
| 8 | 审核与发布 | 确认售价、日期，提交/发布前检查最终版本 | 审核说明、已知问题、近最终构建、发布检查单 | 商店及构建审核通过，等待期满足，再执行明确授权的发布 |

## 用户需要备齐的资料

- 与签约主体一致的个人/公司法定信息、联系方式和后台要求的身份材料。
- 与主体名称匹配的收款银行资料；税务问卷要求的真实信息。具体字段以本人所在地与后台提示为准。
- 每个产品 100 美元或当地等值 Steam Direct 费用的付款方式。
- 商店公开名称、开发者/发行商展示名、支持联系邮箱、支持语言、产品说明。
- 所有实际随包分发的美术、音效、音乐、字体、插件及其它第三方素材的来源/购买/授权记录。

上述账户与费用要求来源：[Steamworks Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)。这份清单不包含用户的实际敏感资料。

## 商店素材清单

| 素材 | 当前官方尺寸/要求 | 项目准备建议 |
|---|---|---|
| Header Capsule | 920 × 430 | 游戏主视觉＋清晰标题 |
| Small Capsule | 462 × 174 | 缩小后仍可读的标题 |
| Main Capsule | 1232 × 706 | 主视觉适配，不简单拉伸 |
| Vertical Capsule | 748 × 896 | 竖版构图 |
| Library 素材与客户端图标 | 按对应 Steamworks 最新模板 | 单独导出，不混用商店图尺寸 |
| 游戏截图 | 实际游戏画面 | 建议选桌面挂机、出牌战斗、装备、成长、剧情等不同场景；建议 6–8 张是项目建议，不是官方最低数量声明 |
| 宣传视频 | 按后台支持格式 | 建议 45–90 秒，优先展示实际玩法；时长为项目建议 |

尺寸来源：[Store Graphical Assets](https://partner.steamgames.com/doc/store/assets/standard)。胶囊图避免添加折扣、评分或其它宣传文字；截图使用实际游戏，不能用概念图冒充。[审核要求](https://partner.steamgames.com/doc/store/review_process)

## 该项目需要特别处理的技术项

- 区分测试 ShippingF10 与首发正式版。完整 F10 不自动等于正式版必须移除，但其开放范围、成就与正常存档写入边界必须明确。
- 桌面透明窗口依赖 Windows/DX12/DWM；保留 DX11 并不等于已实测全部低端显卡。暂不宣称 Linux/macOS/Steam Deck、手柄或云存档支持。
- 确认启动 EXE、工作目录、参数和退出进程跟踪；优先减少小启动器中转，实测 Steam Overlay 与透明窗口的鼠标焦点。
- 上传展开文件夹；Oodle/IoStore 可保留，但用两个相邻版本测增量补丁，不把 ZIP 大小当 Steam 下载量。
- 按需要选择公共运行库；用干净 Windows 镜像测试，不依赖开发机已有环境。
- 云同步只覆盖明确的玩家存档，不同步整个 Saved、F10 快照、日志、缓存、临时写入文件或机器相关窗口配置。
- 先按 Steam 用户隔离本地档案，定义主档和恢复点的新旧选择，再配置云同步；检查离线冲突和跨账号行为。

依据：[SteamPipe](https://partner.steamgames.com/doc/sdk/uploading)、[公共运行库](https://partner.steamgames.com/doc/features/common_redist)、[Steam Cloud](https://partner.steamgames.com/doc/features/cloud)、[Steam Overlay](https://partner.steamgames.com/doc/features/overlay)。

## 时间安排与内容调查

- 首批产品发行通常需付款后至少 30 天；公开 Coming Soon 至少两周。两段可以重叠，并不必然相加为 44 天。
- 商店和构建各自审核，官方通常为 3–5 个工作日，建议各预留至少 7 个工作日；审核通过不等于自动正式发售。
- 先准备商店页可与代码开发并行。正式构建应接近最终版本，并兑现所勾选的功能。
- 内容调查需如实覆盖实际内容及适用的 AI 使用：开发期生成并随游戏交付的美术、声音、剧情、本地化等，与运行时实时生成分别说明。不能仅凭“用过 AI 编程工具”就武断认定全部内容生成，也不能遗漏实际生成资产。

依据：[Onboarding](https://partner.steamgames.com/doc/gettingstarted/onboarding)、[Review Process](https://partner.steamgames.com/doc/store/review_process)、[Content Survey](https://partner.steamgames.com/doc/gettingstarted/contentsurvey)。

## 当前下一步

等待用户提供注册阶段和个人/公司选择（不需要敏感信息）。项目侧继续本地存档审计与加固。尚未完成账号开通、商店材料制作、Steam 上传或发布。

## 已准备的素材

2026-09-12：`Deliverables/Steam/CloudFarer` 已整理11张宣传/库/图标资产与6张运行中游戏界面渲染图、两张用途总览、双语文案、像素尺寸与哈希清单。见同目录上传说明。尚未上传Steam；存档技术验收见 `2026-09-12-save-hardening-acceptance.md`。
