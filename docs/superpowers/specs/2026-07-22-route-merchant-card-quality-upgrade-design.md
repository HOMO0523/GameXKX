# GameXXK 路线商店、卡牌品质与局内升级设计

**日期：** 2026-07-22

**状态：** 已完成逐节讨论，并由用户在后续连续确认中批准、锁定

**适用范围：** 路线地图、路线商店、共享战斗牌组、卡牌/遗物目录、路线 HUD、存档迁移

**不在范围：** 城镇商店经济重做、永久金币兑换、任务 NPC 卡进入路线商店、永久卡牌品质成长

## 1. 目标与已批准结论

路线地图中的商店节点必须成为可完整进入、购买、刷新、离开并返回路线图的玩家流程。路线商店使用独立局内货币“行旅钱”，出售三张卡牌与三个遗物。卡牌具有普通、稀有、珍稀三档基础品质；获得两张相同 CardId、相同品质的牌时自动合成升级。所有升级只在本次路线中有效。

本设计锁定以下结论：

- 卡牌基础品质总数固定为普通 92、稀有 51、珍稀 31，共 174 张。
- 遗物基础品质总数固定为普通 15、稀有 10、珍稀 5，共 30 件。
- 商店卡牌固定为三格：有伙伴时为主角已解锁牌 1、当前伙伴个人 12 张牌库 1、路线牌 1；无伙伴时为主角 1、路线牌 2。
- 商店遗物为三个互不重复且本次路线尚未拥有的遗物；遗物不可叠加。
- 卡牌按品质定价 25/40/60，遗物按品质定价 70/100/140。
- 刷新一次重置六个商品格；费用为 20、30、40、50，之后保持 50。
- 商品只提供目录中的基础品质。卡牌允许重复出现以供升级；当前已达珍稀的 CardId 不再出现。
- 商店购买、战后卡牌奖励与其他局内卡牌来源共用同一自动合成入口。
- 商店节点退出必须恢复路线图输入；任何透明全屏根节点不得阻挡点击。

## 2. 术语与数据边界

现有 `EGameXXKCardRarity` 同时承担 `Permanent/Common/Rare/Boss` 来源语义，不能直接作为新的三档品质使用。实现必须保留该字段用于旧牌池和来源判断，并新增独立品质类型：

```cpp
UENUM(BlueprintType)
enum class EGameXXKCardQuality : uint8
{
    Invalid = 0 UMETA(Hidden),
    Common,
    Rare,
    Epic
};
```

中文显示固定为普通、稀有、珍稀。遗物可复用同一品质枚举，或建立语义完全一致的 `EGameXXKRelicQuality`；若分别建立，必须有同一价格与显示转换入口。

### 2.1 静态定义与运行实例

静态 `FGameXXKCardDefinition` 新增 `BaseQuality`。目录中的费用、效果和 CardId 仍只有一份，禁止为三个品质复制 522 条定义。

本次路线牌组必须从单纯 `TArray<FName> RouteCardIds` 迁移到有序运行配方。每个条目至少保存：

```text
EntryId
CardId
CurrentQuality
OwnerUnitId / OwnerKind
SourceKind
bTemporaryRouteCard
AcquisitionOrdinal
```

`EntryId` 是稳定条目标识，不能以 CardId 代替。每场战斗再由运行配方物化独立 `FGameXXKCardInstance`；战斗实例也必须携带 `CurrentQuality`，以便卡面、Tooltip、预览和结算使用相同数值。

### 2.2 基础牌与临时牌

- 主角 8 张、伙伴 5 张、任务 NPC 3 张是本次路线基础条目，不可被替换。
- 缺伙伴/NPC 时使用的旅途补位牌仍是临时条目。
- 商店购买未在基础配置中的主角/伙伴专属牌，作为该角色持有的临时路线条目加入。
- 路线牌、战后奖励牌和商店新增牌均属于最多 12 格的临时路线牌分组。
- 合成优先保留基础条目并消耗临时副本；两张均为临时条目时，结果仍是临时条目。

## 3. 卡牌自动合成与数值规则

### 3.1 合成算法

获得卡牌后先进行不改数据的完整模拟，然后一次性提交：

1. 按 CardId 与 CurrentQuality 查找相同牌。
2. 两张普通合成为一张稀有；两张稀有合成为一张珍稀。
3. 重复检查直到没有可合成对子；四张普通可连锁成为一张珍稀。
4. 珍稀封顶，不继续合成；已拥有珍稀后，该 CardId 从后续商店和奖励候选排除。
5. 合成结果继承被保留条目的稳定归属；优先级为基础条目高于临时条目、较早 AcquisitionOrdinal 高于较晚条目。
6. 运行配方和活跃战斗快照不得在同一时刻分别修改。战斗中的奖励在战斗结算门之后提交到运行配方，下一场重新物化。

### 3.2 数值计算

品质只改变明确批准的可缩放数值。计算器必须数据驱动，禁止按卡名写分支。

| 数值类型 | 普通 | 稀有 | 珍稀 |
| --- | ---: | ---: | ---: |
| 直接攻击、固定伤害、护甲、治疗 | ×1 | ×2 | ×4 |
| 抽牌 | 原值 | 原值 +1 | 原值 +2 |
| 恢复内力 | 原值 | 原值 +2 | 原值 +4 |
| 施加或移除状态层数 | 原值 | 原值 +1 | 原值 +2 |
| 气力费用、内力费用 | 不变 | 不变 | 不变 |
| 持续时间、触发次数、条件阈值、百分比条件 | 不变 | 不变 | 不变 |

若一张卡同时包含多个同类效果，每个可缩放效果分别应用规则。消费型状态效果只增强最终施加/移除层数，不扩大消费上限；百分比条件、生命阈值和地势判断不变。卡面、Tooltip、目标预览、战斗日志和最终结算必须调用同一最终值解析器。

### 3.3 卡面品质表现

- 所有 174 张卡的卡面与所有悬停 Tooltip 均显示品质文字。
- 普通使用白色，稀有使用蓝色，珍稀使用紫色。
- 颜色只作用于品质标签或小印记，不覆盖主角/职业/NPC 信息带颜色。
- 图鉴显示 `BaseQuality`；本次牌库、手牌、奖励、商店和升级动画显示 `CurrentQuality`。
- 自动合成动画为同名牌向中心聚合、短促水墨闪动、品质文字白→蓝或蓝→紫；允许跳过，不阻塞规则提交或存档。

## 4. 行旅钱与路线经济

### 4.1 货币边界

新增路线局内货币字段，例如 `RouteTravelMoney`。路线运行期间它与永久 `PlayerGold` 完全分离，不能直接混用；终局兑换仅由三章路线结算规则执行：通关按 10:1、失败或放弃按 20:1 转为永久金币：

- 新路线初始化为 60。
- 普通战斗首次结算 +20。
- 精英战斗首次结算 +35。
- Boss 首次结算 +50。
- 部分正向事件可给予 20–40；具体事件内容显式配置，不进行隐藏随机加钱。
- 第三章 Boss 结算并完成整条路线、路线失败或主动放弃时，先用唯一结算凭据完成兑换，再清零；第一章和第二章 Boss 结算后继续保留。
- 城镇商店继续只读取 `PlayerGold`；路线商店只读取行旅钱。

每个可发钱节点必须有已结算记录，保证重新加载地图、读档和重复刷新 UI 不会重复发放。现有遗物 `Relic.WineCup` 的 `GainGold` 必须改为路线货币效果（建议新增 `GainRouteTravelMoney`），不得向永久金币注入局内收益。

### 4.2 左上角路线 HUD

路线地图、商店及需要显示路线概览的覆盖层左上角常驻：

1. 行旅钱图标与当前值。
2. 当前节点 / 总节点。
3. 当前临时路线牌 / 12。

刷新费用只显示在刷新按钮旁；商品价格在商品下；遗物栏继续位于右上角并每行六格自动换行。战斗中若左上信息与意图区冲突，可降低后两项透明度，但不得移动到角色血条区域。

## 5. 商店库存与价格

### 5.1 稳定库存状态

每个商店节点保存一份 `FGameXXKRouteMerchantState`，至少包含：

```text
SourceNodeId
StockSeed
RefreshCount
CardOffers[3]
RelicOffers[3]
bResolved
```

每个商品保存商品 ID、基础品质、价格和已售状态。重新打开商店、切换地图或读档必须恢复同一库存，不能免费重掷。

### 5.2 商品生成

有当前出战伙伴时，三张卡固定为：

1. 当前存档已解锁的主角专属牌一张。
2. 当前出战伙伴 `PersonalCardIds[12]` 中一张。
3. 路线牌一张。

没有当前伙伴时为主角牌一张、路线牌两张。任务 NPC 牌不进入商店。当前已经达到珍稀的 CardId、无效定义和同批重复候选均排除。某分类无合法牌时优先用路线牌补位；所有合法牌耗尽时该格显示“本次路线已收集完”。

三个遗物候选彼此不同，并排除本次路线已经拥有的遗物。遗物不可叠加；现有 `bStackable` 只作为旧存档迁移输入，不再允许重复获得。剩余合法遗物不足三件时显示剩余项，其余格显示“已收集完”。

### 5.3 价格与刷新

| 商品 | 普通 | 稀有 | 珍稀 |
| --- | ---: | ---: | ---: |
| 卡牌 | 25 | 40 | 60 |
| 遗物 | 70 | 100 | 140 |

刷新一次同时重置六个槽并补满已售格。第 1/2/3/4 次刷新分别为 20/30/40/50，后续保持 50。刷新必须先校验余额、生成完整合法库存、保存，再扣款；任一步失败不改变库存或余额。

## 6. 购买、替换与事务安全

商品本体悬停显示完整 Tooltip。商品下方常驻显示行旅钱图标、价格和购买按钮；余额不足时按钮置灰，悬停说明还差多少。点击购买按钮打开小型纸张确认区，显示当前余额、购买后余额、卡牌升级预览或遗物效果。

购买卡牌前模拟完整连锁：

- 合成后不增加临时牌数时，无需替换。
- 会增加临时牌且当前少于 12 格时直接加入。
- 会增加临时牌且已满 12 格时进入现有路线牌替换选择。
- 只能替换临时路线牌；取消返回商店，不扣钱。

事务提交顺序固定为：

```text
验证库存 → 验证货币 → 模拟合成 → 必要时选择替换
→ 写入卡牌/遗物 → 扣行旅钱 → 标记已售 → 保存
```

失败必须整体回滚。购买成功后留在商店，槽位覆盖“已售”印章。战后奖励同样调用自动合成入口；奖励仍只提供目录基础品质。

## 7. 商店 HUD 与素材契约

商店节点显示纯 HUD 商店页，不依赖空白 3D 场景填充。以 1920×1080 为基准，居中安全区整体缩放，不使用每帧投影：

- 左侧约 23% 为行商立绘、名称和“只收本次行旅钱”说明。
- 右侧约 77% 为两排商品；上排三张卡使用 PSD 第一排竖卡比例，下排三个遗物使用高填充方形图标。
- 每格同时显示图、名称、品质、价格和购买按钮。
- “刷新商品”与当前费用、“离开商店”位于底部操作区。
- 购买确认面板不遮挡整页；替换选择复用现有路线奖励替换组件。

优先复用项目已有 PSD 按钮、卡框、背包槽、卡牌图和 30 个遗物图标。需要新制作的资产只有：

- 行旅钱图标：透明底、圆形、高填充、低饱和水墨铜色，带简化行囊结或山路刻纹，不烘焙文字或数字。
- 商店专用纸板/标题（仅当现有 PSD 切图无法拼合时生成）。
- 行商透明立绘（仅在审计确认无可用角色原画后生成）。

生成资产必须严格匹配项目 PSD：低饱和、纸张暖白、墨线边框、主体清楚；禁止用通用灰色 UMG 面板或把整页文字烘焙进图片。

## 8. 进入、退出与输入生命周期

进入商店节点时保存来源节点与库存种子，加载商店屏幕后由控制器创建 HUD、完成最终状态刷新，再设置鼠标可见和 UI 输入。禁止 `NativeConstruct()` 在控制器打开后再次无条件折叠面板。

商店根节点使用 `SelfHitTestInvisible`，只有纸板、商品与按钮命中。离开商店时依次清除确认、替换和 Tooltip 状态，标记节点完成，保存，返回路线图并恢复节点点击。透明全屏层不得残留。跳转失败时保留当前状态和可再次点击的返回按钮，不能卡死。

## 9. 存档迁移

- 新字段均使用 `SaveGame`，并新增路线经济/卡牌实例迁移版本。
- 旧 `RouteCardIds` 按原顺序迁移为基础品质的临时运行条目，保留最多 12 格。
- 主角、伙伴和 NPC 基础配置按目录基础品质物化；旧战斗实例默认使用对应基础品质。
- 旧遗物重复栈折叠为一件，只保留一次效果。
- 正在进行的旧路线若没有行旅钱版本，一次性初始化为 60 并记录版本，后续读档不重复。
- 无效 CardId、伙伴实例或遗物 ID 安全跳过并写可读日志，不阻塞地图或扣资源。
- 第三章 Boss 结算并完成整条路线、路线失败或主动放弃时清除行旅钱、商店库存、CurrentQuality 升级、临时牌和路线遗物；第一章和第二章 Boss 后继续保留；永久金币与永久解锁不变。

## 10. 验收与测试

### 10.1 参数化自动化测试

- 174 张卡全部有合法 `BaseQuality`，计数严格为 92/51/31；各角色池满足批准配额。
- 30 个遗物全部有合法品质，计数严格为 15/10/5。
- 卡面与 Tooltip 品质文字、颜色和最终数值一致。
- 两张普通、两张稀有、四张普通连锁、珍稀封顶、基础条目优先保留均正确。
- 攻防治疗倍率与抽牌/内力/状态增量使用同一解析器。
- 行旅钱初始化、节点单次发放、路线清零和城镇金币隔离正确。
- 商店分类配额、当前伙伴个人牌池、遗物去重、稳定种子、价格和刷新费用正确。
- 余额不足、满 12 格、取消替换、空候选、无效定义均不改变状态。
- 旧存档迁移不丢合法牌，不重复发钱，不重复遗物效果。
- 商店根层不阻挡商品、离开按钮或返回后的路线节点。

测试使用参数化目录遍历和关键边界用例，不为 174 张牌编写重复测试。C++ 验证必须保存、关闭编辑器、冷 UBT 编译并运行目标自动化集合；禁止 Live Coding/Hot Reload。

### 10.2 PIE 流程

1. 开始路线，左上显示行旅钱 60、节点进度与路线牌 0/12。
2. 普通战胜利增加 20；重新加载不重复发放。
3. 商店节点打开即显示完整 HUD，不出现空场景。
4. 有伙伴时检查主角/伙伴个人牌/路线牌各一；无伙伴时检查 1+2。
5. 检查三个未拥有遗物、价格、Tooltip、购买按钮和余额不足置灰。
6. 连续购买同名牌，验证普通→稀有→珍稀和数值预览。
7. 填满 12 个临时槽，验证替换、取消和原子扣款。
8. 刷新商品，验证 20/30/40/50、六格同时补货及读档稳定。
9. 购买遗物，验证右上角栏和下一场战斗效果。
10. 离开商店返回路线图，确认鼠标与节点点击正常。
11. 完成或放弃路线，确认行旅钱、升级品质、临时牌和遗物清除，永久金币不变。

## 附录 A：174 张卡牌基础品质

“商店资格”表示满足运行条件后可进入路线商店；任务 NPC 卡均不可购买。伙伴卡只有当前出战伙伴个人 `PersonalCardIds[12]` 中的卡才有资格。

### A.1 主角（12：普通 6 / 稀有 4 / 珍稀 2）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Hero.QingFengYiShi` | 青锋一式 | 普通 | 已解锁后 |
| `Hero.HeYuZhan` | 鹤羽斩 | 普通 | 已解锁后 |
| `Hero.FengShenBu` | 风身步 | 普通 | 已解锁后 |
| `Hero.GuiYuanShu` | 归元术 | 普通 | 已解锁后 |
| `Hero.HengJianShouShi` | 横剑守势 | 普通 | 已解锁后 |
| `Hero.NingShenTuNa` | 凝神吐纳 | 普通 | 已解锁后 |
| `Hero.SuiYanJi` | 碎岩击 | 稀有 | 已解锁后 |
| `Hero.GuanXi` | 观隙 | 稀有 | 已解锁后 |
| `Hero.PoYunYiShan` | 破云一闪 | 稀有 | 已解锁后 |
| `Hero.HuiFengZhuiJian` | 回风追剑 | 稀有 | 已解锁后 |
| `Hero.JianYiGuanHong` | 剑意贯虹 | 珍稀 | 已解锁后 |
| `Hero.GuiYuanFanZhao` | 归元返照 | 珍稀 | 已解锁后 |

### A.2 任务 NPC（24：普通 12 / 稀有 6 / 珍稀 6）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Npc.TusiChief.ShiMenShouShi` | 石门守势 | 普通 | 否 |
| `Npc.TusiChief.TuSiJunLing` | 土司军令 | 普通 | 否 |
| `Npc.TusiChief.ZhaiZhuHaoLing` | 寨主号令 | 稀有 | 否 |
| `Npc.TusiChief.MengZhaiShiYue` | 盟寨誓约 | 珍稀 | 否 |
| `Npc.SongJinBao.ShangQianGuWu` | 赏钱鼓舞 | 普通 | 否 |
| `Npc.SongJinBao.GuiKeLing` | 贵客令 | 普通 | 否 |
| `Npc.SongJinBao.ErMuMiBao` | 耳目密报 | 稀有 | 否 |
| `Npc.SongJinBao.YiNuoQianJin` | 一诺千金 | 珍稀 | 否 |
| `Npc.YueBai.QingYanDianDeng` | 青焰点灯 | 普通 | 否 |
| `Npc.YueBai.YueBaiZhaoYe` | 月白照夜 | 普通 | 否 |
| `Npc.YueBai.CanJuanPiZhu` | 残卷批注 | 稀有 | 否 |
| `Npc.YueBai.ShanHeCanTu` | 山河残图 | 珍稀 | 否 |
| `Npc.ZhouGuangZu.YiCaoBianShi` | 异草辨识 | 普通 | 否 |
| `Npc.ZhouGuangZu.HuangShanFuZhi` | 黄山敷治 | 普通 | 否 |
| `Npc.ZhouGuangZu.DiZhiMoTu` | 地志摹图 | 稀有 | 否 |
| `Npc.ZhouGuangZu.YanFenFengMai` | 岩粉封脉 | 珍稀 | 否 |
| `Npc.JinGui.QiaoYanZhouXuan` | 巧言周旋 | 普通 | 否 |
| `Npc.JinGui.ZaYiChouBei` | 杂役筹备 | 普通 | 否 |
| `Npc.JinGui.ShiJingErMu` | 市井耳目 | 稀有 | 否 |
| `Npc.JinGui.HouXiangTuoShen` | 后巷脱身 | 珍稀 | 否 |
| `Npc.QiongMeiEr.GuWuMiZong` | 蛊雾迷踪 | 普通 | 否 |
| `Npc.QiongMeiEr.YinLingZhenXin` | 银铃镇心 | 普通 | 否 |
| `Npc.QiongMeiEr.TengQiaoFeiDu` | 藤桥飞渡 | 稀有 | 否 |
| `Npc.QiongMeiEr.ShanGeHuanLing` | 山歌唤灵 | 珍稀 | 否 |

### A.3 刀客（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.Blade.LieFengZhan` | 裂风斩 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.FengHou` | 封喉 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.JiYuLianZhan` | 疾雨连斩 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.JieShiHuiFeng` | 借势回锋 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.YiShangHuanShi` | 以伤换势 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.ZhuYing` | 逐影 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.LangDuan` | 浪断 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.HuiFengJiaShi` | 回锋架势 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.PoLangTuJin` | 破浪突进 | 普通 | 当前伙伴个人牌库 |
| `Profession.Blade.DuanYue` | 断岳 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.YinXueDao` | 饮血刀 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.PoJun` | 破军 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.ZhanYiFeiTeng` | 战意沸腾 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.XiaoJiaLianJi` | 削甲连击 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.DaoYiShouShu` | 刀意收束 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Blade.CanYueSanDie` | 残月三叠 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Blade.ZhanJin` | 斩尽 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Blade.YiShiDuanJiang` | 一式断江 | 珍稀 | 当前伙伴个人牌库 |

### A.4 护卫（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.Guard.TieBi` | 铁壁 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.HuZhu` | 护主 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.ZhenDun` | 震盾 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.GuShou` | 固守 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.YuanHuBu` | 援护步 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.PiJiaXingJun` | 披甲行军 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.PanShiTuNa` | 磐石吐纳 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.YuanJunBiLei` | 援军壁垒 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.DunZhenTuiJin` | 盾阵推进 | 普通 | 当前伙伴个人牌库 |
| `Profession.Guard.FanZhenJia` | 反震甲 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.ZhenYueLing` | 镇岳令 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.QinWangDunJi` | 擒王盾击 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.TieBiRuShan` | 铁壁如山 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.BiLeiFanGong` | 壁垒反攻 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.SuiJiaHuiJi` | 碎甲回击 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Guard.BuDongRuShan` | 不动如山 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Guard.TieSuoHengJiang` | 铁锁横江 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Guard.YiFuDangGuan` | 一夫当关 | 珍稀 | 当前伙伴个人牌库 |

### A.5 医者（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.Healer.CaoMuFuZhi` | 草木敷治 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.QingXinSan` | 清心散 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.YaoYin` | 药引 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.BaiCaoDu` | 百草毒 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.ZhiXueCao` | 止血草 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.XingQiZhen` | 行气针 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.HuiQiXiang` | 回气香 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.LianQiaoJieDu` | 连翘解毒 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.YaoJiuWenShen` | 药酒温身 | 普通 | 当前伙伴个人牌库 |
| `Profession.Healer.LingZhiXuMing` | 灵芝续命 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.HuiChunLu` | 回春露 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.WenYangGao` | 温养膏 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.FuGuSan` | 腐骨散 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.JinChuangXuMing` | 金疮续命 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.KuShenMaSan` | 苦参麻散 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Healer.YaoWangGuiYuan` | 药王归元 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Healer.YaoNangFeiTou` | 药囊飞投 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Healer.WuWeiTiaoHe` | 五味调和 | 珍稀 | 当前伙伴个人牌库 |

### A.6 猎手（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.Hunter.XunXiJian` | 寻隙箭 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.FuBu` | 伏步 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.ZhuiLie` | 追猎 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.YingYan` | 鹰眼 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.LieWang` | 猎网 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.FuZuShi` | 缚足矢 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.LueYingJian` | 掠影箭 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.LieHunBiao` | 猎魂标 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.HuiHuanJian` | 回环箭 | 普通 | 当前伙伴个人牌库 |
| `Profession.Hunter.ChuanYang` | 穿杨 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.LianZhuJian` | 连珠箭 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.YinZong` | 隐踪 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.DuanMaiShi` | 断脉矢 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.PoJiaDing` | 破甲钉 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.FuYeXianJing` | 腐叶陷阱 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Hunter.ShouHun` | 狩魂 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Hunter.BaiBuChuanYang` | 百步穿杨 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Hunter.YingLuo` | 鹰落 | 珍稀 | 当前伙伴个人牌库 |

### A.7 术士（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.Sorcerer.LingHuoFu` | 灵火符 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.JuLing` | 聚灵 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.LiHuoYin` | 离火印 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.YanQiang` | 炎墙 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.FenMaiFu` | 焚脉符 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.NingYanChengRen` | 凝焰成刃 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.RanLingHuanYuan` | 燃灵换元 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.YanMuHuTi` | 焰幕护体 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.LieFu` | 裂符 | 普通 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.BaoYanShu` | 爆炎术 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.SheLingHuo` | 摄灵火 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.LingYanLianDan` | 灵焰连弹 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.HuLingMu` | 护灵幕 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.ChiXiaoFenXing` | 赤霄焚星 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.XingHuoHuiShou` | 星火回收 | 稀有 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.XingHuoLiaoYuan` | 星火燎原 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.FenTianJue` | 焚天诀 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.Sorcerer.ChiYanFengJie` | 赤焰封界 | 珍稀 | 当前伙伴个人牌库 |

### A.8 阵师（18：普通 9 / 稀有 6 / 珍稀 3）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Profession.FormationMaster.GuanShi` | 观势 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.DingZhen` | 定阵 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.YinShuiHuiYuan` | 引水回元 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.KunZhen` | 困阵 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.LinYingMiZong` | 林影迷踪 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.JieShanWeiZhang` | 借山为障 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.YiWeiZhen` | 易位阵 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.ShanMenFengSuo` | 山门封锁 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.LinFengFuZhen` | 林风拂阵 | 普通 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.CunZhaiYuanZhen` | 村寨援阵 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.HuiShengZhenSha` | 回声震杀 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.BaMenLunZhuan` | 八门轮转 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.ShuiJingZheGuang` | 水镜折光 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.ZhenQiGuWu` | 阵旗鼓舞 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.DiMaiJieLi` | 地脉借力 | 稀有 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.ZhenShaZhen` | 镇煞阵 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.WanXiangGuiZhen` | 万象归阵 | 珍稀 | 当前伙伴个人牌库 |
| `Profession.FormationMaster.SiXiangLianHuan` | 四象连环 | 珍稀 | 当前伙伴个人牌库 |

### A.9 路线牌（30：普通 20 / 稀有 5 / 珍稀 5）

| CardId | 中文名 | 基础品质 | 商店资格 |
| --- | --- | --- | --- |
| `Route.General.PoJiaTuCi` | 破甲突刺 | 普通 | 是 |
| `Route.General.ShouShiHuiYuan` | 守势回元 | 普通 | 是 |
| `Route.General.QingShenQuShi` | 轻身取势 | 普通 | 是 |
| `Route.General.TuNaJue` | 吐纳诀 | 普通 | 是 |
| `Route.General.ZhiXueSan` | 止血散 | 普通 | 是 |
| `Route.General.FeiZhen` | 飞针 | 普通 | 是 |
| `Route.General.YanDun` | 烟遁 | 普通 | 是 |
| `Route.General.TieJiLi` | 铁蒺藜 | 普通 | 是 |
| `Route.General.LinZhenMoRen` | 临阵磨刃 | 普通 | 是 |
| `Route.General.HeJiLing` | 合击令 | 普通 | 是 |
| `Route.Terrain.DuanYaLuoShi` | 断崖落石 | 普通 | 是 |
| `Route.Terrain.LinYingFuXi` | 林影伏袭 | 普通 | 是 |
| `Route.Terrain.DuKouHuiLiu` | 渡口回流 | 普通 | 是 |
| `Route.Terrain.ZhaiHuoYuanShou` | 寨火援手 | 普通 | 是 |
| `Route.Terrain.DongHuoZhaoMing` | 洞火照明 | 普通 | 是 |
| `Route.Terrain.JieShiTuXi` | 借势突袭 | 普通 | 是 |
| `Route.Terrain.XingJunBuZhen` | 行军布阵 | 普通 | 是 |
| `Route.Terrain.DiMaiHuiXiang` | 地脉回响 | 普通 | 是 |
| `Route.Terrain.LinShiZhaYing` | 临时扎营 | 普通 | 是 |
| `Route.Terrain.XianLuTuWei` | 险路突围 | 普通 | 是 |
| `Route.Rare.GuJuanCanZhang` | 古卷残章 | 稀有 | 是 |
| `Route.Rare.TieYiYiJue` | 铁衣遗诀 | 稀有 | 是 |
| `Route.Rare.LingQuanYiYin` | 灵泉一饮 | 稀有 | 是 |
| `Route.Rare.JueJingFanJi` | 绝境反击 | 稀有 | 是 |
| `Route.Rare.TongXinHeBi` | 同心合璧 | 稀有 | 是 |
| `Route.Boss.XiongPiPiJia` | 熊罴皮甲 | 珍稀 | 是 |
| `Route.Boss.HanDiYiShi` | 撼地遗势 | 珍稀 | 是 |
| `Route.Boss.HuPoZhenDan` | 虎魄镇胆 | 珍稀 | 是 |
| `Route.Boss.DuKouLieFeng` | 渡口猎风 | 珍稀 | 是 |
| `Route.Boss.FuHuDuanJiang` | 伏虎断江 | 珍稀 | 是 |

## 附录 B：30 件遗物的品质与商店资格

分布锁定为普通 15、稀有 10、珍稀 5。遗物在单次路线中不重复获得；已持有的遗物从事件、奖励与商店候选池中排除。所有未持有遗物均有路线商店资格，价格分别为 70 / 100 / 140 行旅钱。

| RelicId | 中文名 | 品质 | 商店资格 |
| --- | --- | --- | --- |
| `Relic.AncientCoin` | 古铜方孔钱 | 普通 | 未持有时可售 |
| `Relic.JadeBell` | 青玉铃 | 普通 | 未持有时可售 |
| `Relic.MedicineGourd` | 青釉药葫 | 普通 | 未持有时可售 |
| `Relic.TeaBrick` | 陈香茶砖 | 普通 | 未持有时可售 |
| `Relic.RainCape` | 旧蓑衣 | 普通 | 未持有时可售 |
| `Relic.PineCone` | 雷击松果 | 普通 | 未持有时可售 |
| `Relic.RiverPearl` | 江心珠 | 普通 | 未持有时可售 |
| `Relic.CandleStub` | 长明烛心 | 普通 | 未持有时可售 |
| `Relic.FoxMask` | 旧狐面 | 普通 | 未持有时可售 |
| `Relic.StoneLion` | 袖珍石狮 | 普通 | 未持有时可售 |
| `Relic.WineCup` | 缺口酒盏 | 普通 | 未持有时可售 |
| `Relic.HerbBasket` | 百草小篓 | 普通 | 未持有时可售 |
| `Relic.PaperCrane` | 祈愿纸鹤 | 普通 | 未持有时可售 |
| `Relic.BrokenArrow` | 折锋箭簇 | 普通 | 未持有时可售 |
| `Relic.MoonDisc` | 月白玉璧 | 普通 | 未持有时可售 |
| `Relic.TigerSeal` | 虎纹兵符 | 稀有 | 未持有时可售 |
| `Relic.InkTalisman` | 镇煞墨符 | 稀有 | 未持有时可售 |
| `Relic.CloudMirror` | 云纹古镜 | 稀有 | 未持有时可售 |
| `Relic.StoneBead` | 山石念珠 | 稀有 | 未持有时可售 |
| `Relic.IronKnot` | 玄铁结 | 稀有 | 未持有时可售 |
| `Relic.Compass` | 寻路司南 | 稀有 | 未持有时可售 |
| `Relic.RedCord` | 同心红绳 | 稀有 | 未持有时可售 |
| `Relic.BronzeNeedle` | 定脉铜针 | 稀有 | 未持有时可售 |
| `Relic.LotusSeed` | 清心莲子 | 稀有 | 未持有时可售 |
| `Relic.SwordGuard` | 旧剑镡 | 稀有 | 未持有时可售 |
| `Relic.BambooTally` | 竹节令 | 珍稀 | 未持有时可售 |
| `Relic.CraneFeather` | 鹤羽 | 珍稀 | 未持有时可售 |
| `Relic.ChessStone` | 残局黑子 | 珍稀 | 未持有时可售 |
| `Relic.DrumCharm` | 震山鼓坠 | 珍稀 | 未持有时可售 |
| `Relic.OldMap` | 残山旧图 | 珍稀 | 未持有时可售 |
