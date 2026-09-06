# F10 开发测试工作台

首版用于开发包试玩、配装准备和AI数值复现。入口是F10，Esc关闭面板；面板关闭后临时试验仍可继续游玩。“返回原进度”结束试验并恢复进入前的状态。

F10只在游戏窗口获得焦点时响应；收起再打开保留页签、当前角色和输入。面板使用项目纸底、江湖体标题与墨迹滑块，数值独立于挂机HUD缩放。

## 人工操作

- **万物匣**：中文名称检索道具、装备和局内遗物，选择数量后获得。装备可指定等级和品质；遗物在进入局内后添加。
- **配装台**：在右侧选择主角、伙伴或NPC，设置角色等级；按六件同套、四加二、两加两加两生成装备，选择强化、词缀档位和宝石，再生成至仓库或直接穿戴。
- **试武场**：选择真实关卡、关内场次和种子，进入现有BattleBoard；同种子重开或返回战前配装状态。批测使用独立状态副本。
- **试验记录**：保存或载入整队/战斗快照，查看批测摘要和记录目录，执行高级JSON命令并复制结果。

右侧属性从实际装备快照读取。修改配装在战前进行；局内可以发放遗物和回复资源。所有修改失败时应保留修改前状态。

临时试验会抑制原玩家存档和训练恢复点写入。需要保留试验成果时使用命名Dev快照；它与正常玩家存档分开。关闭面板不会自动撤销正在试玩的配装。

## AI与本机脚本

`scripts/gamexxk_dev_client.py`使用与面板相同的运行时指令。编辑器可选`--via mcp`；开发包使用默认文件协议，并通过`--dev-dir`指定记录页打开的DevTools目录，不依赖Editor专属MCP插件。

```powershell
python scripts/gamexxk_dev_client.py --via mcp call help
python scripts/gamexxk_dev_client.py --via mcp call inspect
python scripts/gamexxk_dev_client.py --via mcp call heal
python scripts/gamexxk_dev_client.py --via mcp export Saved/my-build.json
python scripts/gamexxk_dev_client.py --via mcp simulate --scene Saved/my-build.json --stage Training.Hell.3-1 --encounter 7 --seed 20260906 --runs 100
python scripts/gamexxk_dev_client.py --via mcp compare --a Saved/build-a.json --b Saved/build-b.json --stage Training.Hell.3-1 --runs 100
python scripts/gamexxk_dev_client.py --via mcp call session.restore
```

参数较多时，使用`call <命令> --args-file <JSON文件>`，避免终端对JSON引号进行二次处理。配装参数示例：

```json
{
  "character": "Player",
  "sets": ["XuanJia", "XuanJia", "XuanJia", "XuanJia", "PoJun", "PoJun"],
  "level": 100,
  "character_level": 100,
  "quality": 6,
  "enhance": 10,
  "affix": "mid",
  "gem": "balanced",
  "equip": true
}
```

该参数对应`equipment.loadout`。六槽按武器、头部、衣服、腰带、鞋子、饰品排列。品质1～10对应普通、稀有、珍稀、传奇、不朽、至宝、超凡、天界、登神、宇宙。角色ID通过`inspect`获取。

`character_level`可选，提供时会在同一事务内设置角色等级并生成装备；省略时只使用`level`指定装备等级。

要比较两套配装，分别导出场景，使用相同关卡、场次、种子起点、样本数和回合上限运行。每份摘要明确区分胜利、战败、僵局和执行错误，并保留逐种子记录。首版策略是现有`Skilled`，其结果不是最优出牌或最优配装的证明。

快照与报告中的`binary_md5`标识实际运行的模块或游戏程序。执行错误不参与胜率与平均回合统计；客户端会把含执行错误或已取消的批次标为失败。游戏重启导致批次ID变化时也会明确报错。

## 文件协议

目录包含`inbox`、`outbox`、`snapshots`、`reports`。客户端先写临时文件，再将完整JSON原子改名到inbox；运行时将结果写到同名outbox文件。调用超时后可以复用相同request_id收取结果，无须重新发放物品。

```json
{
  "schema": 1,
  "request_id": "sample_001",
  "command": "item.give",
  "args": {"id": "Currency.Gold", "quantity": 1000}
}
```

常用命令：`catalog`、`inspect`、`session.begin`、`session.restore`、`item.give`、`equipment.create`、`equipment.loadout`、`character.level`、`party.select`、`cards.set`、`heal`、`battle.start`、`battle.restart`、`battle.return`、`battle.auto`、`snapshot.save`、`snapshot.load`、`snapshot.export`、`snapshot.import`、`snapshot.list`、`simulate.start`、`simulate.status`、`simulate.cancel`。

`simulate.start`可以携带导出的`scene`对象而不改变正在游玩的状态；`continue_current=true`并设置`runs=1`可从当前战斗继续推演。每个批次有独立ID、摘要、逐种子轨迹和来源场景。F10可通过`settings.key`修改为其他功能键。

## 验证边界

功能以最终验收记录为准。开发入口在Shipping中不创建子系统、不注册输入或文件处理；Development构建需单独验证。原有玩法、伤害与装备规则仍由项目现有运行时负责。
# 2026-09-07 扩展：整队配装与真实数值报告

首页“一键整备”默认统一100级、至宝/+10、同品质12孔4攻4防4生命，主角套装可选。伙伴按刀客破军、守卫玄甲、药师青囊、弓手/法师追风、阵师山河配置；NPC按第一关联职业配置，蚀骨六件放物理背包。匹配装备会复用，失败不会部分提交。

新增共享命令：

- `equipment.recommend_all`：`level`、`hero_set`。
- `benchmark.prepare`：`role`、`npc`、`hero_direction`、`npc_omit`，可传 `hero_cards/partner_cards/npc_cards`，以及 `enhance`、`gems` 做对照。返回独立场景，不修改玩家当前状态。
- `simulate.run`：`scene`、`stage`、`encounter`、`seed`、`max_rounds`，同步返回真实战斗结果、开局和完整逐步账本。请求成功与模拟成功分开，必须检查 `data.ok/outcome/error`。

报告工具为 `scripts/gamexxk_balance_report.py`，三表核对工具为 `scripts/audit_game_design_tables.py`。前者只收集/聚合运行时结果，不包含替代战斗公式；可使用 `--via files` 连接Development游戏，或保存并关闭交互编辑器后用默认命令行模式。示例：

```powershell
python scripts/gamexxk_balance_report.py manifest --suite matrix --extended --runs 5 --output Saved/Balance/my-matrix.json
python scripts/gamexxk_balance_report.py run --manifest Saved/Balance/my-matrix.json
python scripts/gamexxk_balance_report.py render --manifest Saved/Balance/my-matrix.json
```

本轮报告位于 `Deliverables/Reports/2026-09-07/试武数值与三表核对报告.html`。采用固定合法牌组和Skilled贪心策略，界面中的“60回合内胜率”包含僵局和达到测试上限的观察；运行错误另列。不要将它解读为最优牌组或普通玩家胜率。
