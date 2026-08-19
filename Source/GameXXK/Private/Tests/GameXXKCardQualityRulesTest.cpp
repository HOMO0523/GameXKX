#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKRelicCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FExpectedQuality
	{
		const TCHAR* Id;
		EGameXXKCardQuality Quality;
	};

	const TArray<FExpectedQuality>& GetExpectedCardQualities()
	{
		static const TArray<FExpectedQuality> Expected = {
			{ TEXT("Hero.Generic.QingFengYiShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.HeYuZhan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.FengShenBu"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.SuiYanJi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.GuiYuanShu"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.HengJianShouShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.NingShenTuNa"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.GuanXi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.PoYunYiShan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.XingQiHuiHuan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.JianYiGuanHong"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Generic.GuiYuanFanZhao"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Blade.TongFengYinShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Blade.XueLuXiangCheng"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Blade.YingFengHuanBu"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Blade.TongPaoJuShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Guard.TieBiTongShou"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Guard.JieJiaHuanFeng"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Guard.LieZhenChengFeng"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Guard.XuanJiaZhenYue"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Healer.YiXueCuiFang"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Healer.HuiChunNiMai"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Healer.DuHuoTongLu"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Healer.BaiCaoJiZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Hunter.FengYanDingXian"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Hunter.LieYuLianShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Hunter.CuiDuChuanXin"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Hunter.HuiFengGuanRi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Mage.YanXuLiaoYuan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Mage.HanXuNingChuan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Mage.LeiXuYinTing"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Mage.GuiXuTongXuan"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Formation.GuanShiLuoZi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Formation.YiZhenHuiXiang"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Formation.LianYingBuShi"), EGameXXKCardQuality::Common },
			{ TEXT("Hero.Formation.LiuHeGuiYi"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.TusiChief.ShiMenShouShi"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.TusiChief.TuSiJunLing"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.TusiChief.MengZhaiShiYue"), EGameXXKCardQuality::Epic },
			{ TEXT("Npc.SongJinBao.ShangQianGuWu"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.SongJinBao.ErMuMiBao"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.SongJinBao.GuiKeLing"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.SongJinBao.YiNuoQianJin"), EGameXXKCardQuality::Epic },
			{ TEXT("Npc.YueBai.QingYanDianDeng"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.YueBai.CanJuanPiZhu"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.YueBai.YueBaiZhaoYe"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.YueBai.ShanHeCanTu"), EGameXXKCardQuality::Epic },
			{ TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.ZhouGuangZu.YanFenFengMai"), EGameXXKCardQuality::Epic },
			{ TEXT("Npc.JinGui.ShiJingErMu"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.JinGui.QiaoYanZhouXuan"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.JinGui.ZaYiChouBei"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.JinGui.HouXiangTuoShen"), EGameXXKCardQuality::Epic },
			{ TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), EGameXXKCardQuality::Rare },
			{ TEXT("Npc.QiongMeiEr.GuWuMiZong"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.QiongMeiEr.YinLingZhenXin"), EGameXXKCardQuality::Common },
			{ TEXT("Npc.QiongMeiEr.ShanGeHuanLing"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Blade.LieFengZhan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.FengHou"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.JiYuLianZhan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.JieShiHuiFeng"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.JingHongChuQiao"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.DuanYue"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.ZhuYing"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.YinXueDao"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.PoJun"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.BaoDaoShouYe"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Blade.ZhanYiFeiTeng"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.ZhanJin"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Blade.LangDuan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.HuiFengJiaShi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.LianXiGuiQiao"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.PoLangTuJin"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Blade.HengYunKaiFeng"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Blade.YiShiDuanJiang"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Guard.TieBi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.HuZhu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.ZhenDun"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.GuShou"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.FanZhenJia"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.ZhenYueLing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.YuanHuBu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.PiJiaXingJun"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.QinWangDunJi"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.TieBiRuShan"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.BiLeiFanGong"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.BuDongRuShan"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Guard.PanShiTuNa"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.YuanJunBiLei"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.DunZhenTuiJin"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Guard.TieSuoHengJiang"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Guard.SuiJiaHuiJi"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Guard.YiFuDangGuan"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Healer.CaoMuFuZhi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.QingXinSan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.YaoYin"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.BaiCaoDu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.LingZhiXuMing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.HuiChunLu"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.ZhiXueCao"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.XingQiZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.WenYangGao"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.FuGuSan"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.JinChuangXuMing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.YaoWangGuiYuan"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Healer.HuiQiXiang"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.LianQiaoJieDu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.YaoJiuWenShen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Healer.YaoNangFeiTou"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Healer.KuShenMaSan"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Healer.WuWeiTiaoHe"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Hunter.XunXiJian"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.FuBu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.ZhuiLie"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.YingYan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.LieWang"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.ChuanYang"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.LianZhuJian"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.FuZuShi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.YinZong"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.DuanMaiShi"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.ShouHun"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Hunter.BaiBuChuanYang"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Hunter.LueYingJian"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.LieHunBiao"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.PoJiaDing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.HuiHuanJian"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Hunter.FuYeXianJing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Hunter.YingLuo"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Sorcerer.LingHuoFu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.JuLing"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.LiHuoYin"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.YanQiang"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.BaoYanShu"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Sorcerer.SheLingHuo"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.FenMaiFu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.LingYanLianDan"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.HuLingMu"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.ChiXiaoFenXing"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.FenTianJue"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.Sorcerer.NingYanChengRen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.RanLingHuanYuan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.YanMuHuTi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.LieFu"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.Sorcerer.XingHuoHuiShou"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.Sorcerer.ChiYanFengJie"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.FormationMaster.GuanShi"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.DingZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.YinShuiHuiYuan"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.KunZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.LinYingMiZong"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.JieShanWeiZhang"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.HuiShengZhenSha"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.YiWeiZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.BaMenLunZhuan"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.ZhenShaZhen"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.FormationMaster.WanXiangGuiZhen"), EGameXXKCardQuality::Epic },
			{ TEXT("Profession.FormationMaster.ShanMenFengSuo"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.ShuiJingZheGuang"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.LinFengFuZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Profession.FormationMaster.ZhenQiGuWu"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.DiMaiJieLi"), EGameXXKCardQuality::Rare },
			{ TEXT("Profession.FormationMaster.SiXiangLianHuan"), EGameXXKCardQuality::Epic },
			{ TEXT("Route.General.PoJiaTuCi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.ShouShiHuiYuan"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.QingShenQuShi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.TuNaJue"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.ZhiXueSan"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.FeiZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.YanDun"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.TieJiLi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.LinZhenMoRen"), EGameXXKCardQuality::Common },
			{ TEXT("Route.General.HeJiLing"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.DuanYaLuoShi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.LinYingFuXi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.DuKouHuiLiu"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.ZhaiHuoYuanShou"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.DongHuoZhaoMing"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.JieShiTuXi"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.XingJunBuZhen"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.DiMaiHuiXiang"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.LinShiZhaYing"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Terrain.XianLuTuWei"), EGameXXKCardQuality::Common },
			{ TEXT("Route.Rare.GuJuanCanZhang"), EGameXXKCardQuality::Rare },
			{ TEXT("Route.Rare.TieYiYiJue"), EGameXXKCardQuality::Rare },
			{ TEXT("Route.Rare.LingQuanYiYin"), EGameXXKCardQuality::Rare },
			{ TEXT("Route.Rare.JueJingFanJi"), EGameXXKCardQuality::Rare },
			{ TEXT("Route.Rare.TongXinHeBi"), EGameXXKCardQuality::Rare },
			{ TEXT("Route.Boss.XiongPiPiJia"), EGameXXKCardQuality::Epic },
			{ TEXT("Route.Boss.HanDiYiShi"), EGameXXKCardQuality::Epic },
			{ TEXT("Route.Boss.HuPoZhenDan"), EGameXXKCardQuality::Epic },
			{ TEXT("Route.Boss.DuKouLieFeng"), EGameXXKCardQuality::Epic },
			{ TEXT("Route.Boss.FuHuDuanJiang"), EGameXXKCardQuality::Epic }
		};
		return Expected;
	}

	const TArray<FExpectedQuality>& GetExpectedRelicQualities()
	{
		static const TArray<FExpectedQuality> Expected = {
			{ TEXT("Relic.AncientCoin"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.JadeBell"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.BambooTally"), EGameXXKCardQuality::Epic },
			{ TEXT("Relic.TigerSeal"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.MedicineGourd"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.InkTalisman"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.CloudMirror"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.StoneBead"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.CraneFeather"), EGameXXKCardQuality::Epic },
			{ TEXT("Relic.IronKnot"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.TeaBrick"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.Compass"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.RedCord"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.BronzeNeedle"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.RainCape"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.ChessStone"), EGameXXKCardQuality::Epic },
			{ TEXT("Relic.DrumCharm"), EGameXXKCardQuality::Epic },
			{ TEXT("Relic.LotusSeed"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.SwordGuard"), EGameXXKCardQuality::Rare },
			{ TEXT("Relic.OldMap"), EGameXXKCardQuality::Epic },
			{ TEXT("Relic.PineCone"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.RiverPearl"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.CandleStub"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.FoxMask"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.StoneLion"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.WineCup"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.HerbBasket"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.PaperCrane"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.BrokenArrow"), EGameXXKCardQuality::Common },
			{ TEXT("Relic.MoonDisc"), EGameXXKCardQuality::Common }
		};
		return Expected;
	}

	bool IsConcreteQuality(const EGameXXKCardQuality Quality)
	{
		return Quality == EGameXXKCardQuality::Common
			|| Quality == EGameXXKCardQuality::Rare
			|| Quality == EGameXXKCardQuality::Epic;
	}

	struct FEffectScalingExpectation
	{
		EGameXXKCardEffectType Type;
		int32 BaseMagnitude;
		int32 CommonMagnitude;
		int32 RareMagnitude;
		int32 EpicMagnitude;
	};

	const TArray<FEffectScalingExpectation>& GetEffectScalingExpectations()
	{
		static const TArray<FEffectScalingExpectation> Expectations = {
			{ EGameXXKCardEffectType::DamagePercentAttack, 7, 7, 14, 28 },
			{ EGameXXKCardEffectType::DamageFlat, 7, 7, 14, 28 },
			{ EGameXXKCardEffectType::LoseHealth, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::Heal, 7, 7, 14, 28 },
			{ EGameXXKCardEffectType::AddArmor, 7, 7, 14, 28 },
			{ EGameXXKCardEffectType::GainMana, 3, 3, 5, 7 },
			{ EGameXXKCardEffectType::GainEnergy, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::GainManaPerConsumedStatus, 3, 3, 5, 7 },
			{ EGameXXKCardEffectType::DrawCards, 2, 2, 3, 4 },
			{ EGameXXKCardEffectType::ApplyStatus, 2, 2, 3, 4 },
			{ EGameXXKCardEffectType::RemoveStatus, 2, 2, 3, 4 },
			{ EGameXXKCardEffectType::RemoveAnyDamageOverTime, 2, 2, 3, 4 },
			{ EGameXXKCardEffectType::Insight, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::DiscoverCards, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::ReorderCards, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::DiscardCards, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::IgnoreDefense, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::BonusDamagePercent, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, 7, 7, 14, 28 },
			{ EGameXXKCardEffectType::ApplyGuardLink, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::ApplyBattleModifier, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::ModifyHealingPercent, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::ModifyEnergyCost, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::RevealEnemyIntent, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::DoubleTerrainBonus, 7, 7, 7, 7 },
			{ EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks, 7, 7, 7, 7 }
		};
		return Expectations;
	}

	FGameXXKCardEffectCondition MakeScalingSentinelCondition()
	{
		FGameXXKCardEffectCondition Condition;
		Condition.Type = EGameXXKCardEffectConditionType::TargetHasStatus;
		Condition.Status = EGameXXKCardStatus::Burn;
		Condition.MinimumStatusStacks = 3;
		Condition.MinimumArmor = 11;
		Condition.HealthPercentThreshold = 42.5f;
		Condition.Terrain = EGameXXKCardTerrain::Forest;
		Condition.AlternateTerrain = EGameXXKCardTerrain::Cave;
		Condition.bConsumeStatus = true;
		Condition.MaxConsumedStatusStacks = 5;
		Condition.bScaleMagnitudeByConsumedStacks = true;
		Condition.bConsumeOwnerArmor = true;
		Condition.MaxConsumedArmor = 13;
		Condition.bNegate = true;
		return Condition;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardQualityRulesTest,
	"GameXXK.Data.CardQuality.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardQualityRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("quality Invalid value remains zero"), static_cast<uint8>(EGameXXKCardQuality::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("quality Common value remains one"), static_cast<uint8>(EGameXXKCardQuality::Common), static_cast<uint8>(1));
	TestEqual(TEXT("quality Rare value remains two"), static_cast<uint8>(EGameXXKCardQuality::Rare), static_cast<uint8>(2));
	TestEqual(TEXT("quality Epic value remains three"), static_cast<uint8>(EGameXXKCardQuality::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("legacy rarity Invalid value remains zero"), static_cast<uint8>(EGameXXKCardRarity::Invalid), static_cast<uint8>(0));
	TestEqual(TEXT("legacy rarity Permanent value remains one"), static_cast<uint8>(EGameXXKCardRarity::Permanent), static_cast<uint8>(1));
	TestEqual(TEXT("legacy rarity Common value remains two"), static_cast<uint8>(EGameXXKCardRarity::Common), static_cast<uint8>(2));
	TestEqual(TEXT("legacy rarity Rare value remains three"), static_cast<uint8>(EGameXXKCardRarity::Rare), static_cast<uint8>(3));
	TestEqual(TEXT("legacy rarity Boss value remains four"), static_cast<uint8>(EGameXXKCardRarity::Boss), static_cast<uint8>(4));

	TestEqual(TEXT("card definitions default to Common quality"), FGameXXKCardDefinition().BaseQuality, EGameXXKCardQuality::Common);
	TestEqual(TEXT("card instances default to Common quality"), FGameXXKCardInstance().CurrentQuality, EGameXXKCardQuality::Common);
	TestEqual(TEXT("relic definitions default to Common quality"), FGameXXKRelicDefinition().BaseQuality, EGameXXKCardQuality::Common);

	const UEnum* QualityEnum = StaticEnum<EGameXXKCardQuality>();
	TestNotNull(TEXT("card quality is reflected"), QualityEnum);
	if (QualityEnum)
	{
		const int32 InvalidIndex = QualityEnum->GetIndexByValue(static_cast<int64>(EGameXXKCardQuality::Invalid));
#if WITH_METADATA
		TestTrue(TEXT("Invalid quality stays hidden"), !QualityEnum->GetMetaData(TEXT("Hidden"), InvalidIndex).IsEmpty());
#else
		TestTrue(TEXT("Invalid quality metadata is unavailable in this target"), true);
#endif
		TestEqual(TEXT("Common has the approved Chinese display name"),
			QualityEnum->GetDisplayNameTextByValue(static_cast<int64>(EGameXXKCardQuality::Common)).ToString(), FString(TEXT("普通")));
		TestEqual(TEXT("Rare has the approved Chinese display name"),
			QualityEnum->GetDisplayNameTextByValue(static_cast<int64>(EGameXXKCardQuality::Rare)).ToString(), FString(TEXT("稀有")));
		TestEqual(TEXT("Epic has the approved Chinese display name"),
			QualityEnum->GetDisplayNameTextByValue(static_cast<int64>(EGameXXKCardQuality::Epic)).ToString(), FString(TEXT("珍稀")));
	}

	const TArray<FExpectedQuality>& ExpectedCardEntries = GetExpectedCardQualities();
	TestEqual(TEXT("independent card authority contains exactly 198 entries"), ExpectedCardEntries.Num(), 198);
	TMap<FName, EGameXXKCardQuality> ExpectedCards;
	int32 ExpectedCommonCards = 0;
	int32 ExpectedRareCards = 0;
	int32 ExpectedEpicCards = 0;
	for (const FExpectedQuality& Entry : ExpectedCardEntries)
	{
		const FName Id(Entry.Id);
		TestFalse(FString::Printf(TEXT("independent card authority ID is unique: %s"), Entry.Id), ExpectedCards.Contains(Id));
		if (!ExpectedCards.Contains(Id))
		{
			ExpectedCards.Add(Id, Entry.Quality);
		}
		switch (Entry.Quality)
		{
		case EGameXXKCardQuality::Common: ++ExpectedCommonCards; break;
		case EGameXXKCardQuality::Rare: ++ExpectedRareCards; break;
		case EGameXXKCardQuality::Epic: ++ExpectedEpicCards; break;
		default: AddError(FString::Printf(TEXT("independent card authority has invalid quality: %s"), Entry.Id)); break;
		}
	}
	TestEqual(TEXT("independent Common card count"), ExpectedCommonCards, 122);
	TestEqual(TEXT("independent Rare card count"), ExpectedRareCards, 47);
	TestEqual(TEXT("independent Epic card count"), ExpectedEpicCards, 29);

	const TArray<FGameXXKCardDefinition>& CardDefinitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("card catalog contains exactly 198 definitions"), CardDefinitions.Num(), 198);
	TSet<FName> ActualCardIds;
	int32 ActualCommonCards = 0;
	int32 ActualRareCards = 0;
	int32 ActualEpicCards = 0;
	for (const FGameXXKCardDefinition& Definition : CardDefinitions)
	{
		TestFalse(FString::Printf(TEXT("actual card catalog ID is unique: %s"), *Definition.Id.ToString()), ActualCardIds.Contains(Definition.Id));
		ActualCardIds.Add(Definition.Id);
		const EGameXXKCardQuality* ExpectedQuality = ExpectedCards.Find(Definition.Id);
		TestNotNull(FString::Printf(TEXT("actual card ID is present in independent authority: %s"), *Definition.Id.ToString()), ExpectedQuality);
		if (ExpectedQuality)
		{
			TestEqual(FString::Printf(TEXT("card catalog has independent approved quality: %s"), *Definition.Id.ToString()), Definition.BaseQuality, *ExpectedQuality);
			TestEqual(FString::Printf(TEXT("card rule has independent approved quality: %s"), *Definition.Id.ToString()),
				FGameXXKCardQualityRules::GetCardBaseQuality(Definition.Id), *ExpectedQuality);
		}
		TestTrue(FString::Printf(TEXT("card has concrete quality: %s"), *Definition.Id.ToString()), IsConcreteQuality(Definition.BaseQuality));
		switch (Definition.BaseQuality)
		{
		case EGameXXKCardQuality::Common: ++ActualCommonCards; break;
		case EGameXXKCardQuality::Rare: ++ActualRareCards; break;
		case EGameXXKCardQuality::Epic: ++ActualEpicCards; break;
		default: break;
		}
	}
	for (const TPair<FName, EGameXXKCardQuality>& Expected : ExpectedCards)
	{
		TestTrue(FString::Printf(TEXT("independent card authority ID is present in actual catalog: %s"), *Expected.Key.ToString()), ActualCardIds.Contains(Expected.Key));
	}
	TestEqual(TEXT("actual card catalog has no missing or extra unique IDs"), ActualCardIds.Num(), ExpectedCards.Num());
	TestEqual(TEXT("Common card count"), ActualCommonCards, 122);
	TestEqual(TEXT("Rare card count"), ActualRareCards, 47);
	TestEqual(TEXT("Epic card count"), ActualEpicCards, 29);

	FString CardValidationError;
	TestTrue(TEXT("production card quality invariants accept the real catalog"),
		FGameXXKCardQualityRules::ValidateCardCatalog(CardDefinitions, CardValidationError));
	TestTrue(TEXT("valid card quality catalog has no validation error"), CardValidationError.IsEmpty());
	TArray<FGameXXKCardDefinition> ReplacedCardCatalog = CardDefinitions;
	const int32 EpicCardIndex = ReplacedCardCatalog.IndexOfByPredicate([](const FGameXXKCardDefinition& Definition)
	{
		return Definition.Id == TEXT("Npc.TusiChief.MengZhaiShiYue");
	});
	TestTrue(TEXT("tampered card fixture finds an explicitly classified card"), EpicCardIndex != INDEX_NONE);
	if (EpicCardIndex != INDEX_NONE)
	{
		ReplacedCardCatalog[EpicCardIndex].Id = TEXT("Missing.Replacement.Card");
		ReplacedCardCatalog[EpicCardIndex].BaseQuality = EGameXXKCardQuality::Common;
		FString ReplacedCardError;
		TestFalse(TEXT("production card validator rejects a catalog that replaces a classified ID"),
			FGameXXKCardQualityRules::ValidateCardCatalog(ReplacedCardCatalog, ReplacedCardError));
		TestFalse(TEXT("rejected card catalog explains the invariant failure"), ReplacedCardError.IsEmpty());
	}

	const FName MissingCard(TEXT("Missing.Card"));
	TestNull(TEXT("unknown card sentinel is absent from catalog"), FGameXXKCardCatalog::FindCardDefinition(MissingCard));
	TestEqual(TEXT("unknown card IDs default to Common"),
		FGameXXKCardQualityRules::GetCardBaseQuality(MissingCard), EGameXXKCardQuality::Common);

	const TArray<FExpectedQuality>& ExpectedRelicEntries = GetExpectedRelicQualities();
	TestEqual(TEXT("independent relic authority contains exactly 30 entries"), ExpectedRelicEntries.Num(), 30);
	TMap<FName, EGameXXKCardQuality> ExpectedRelics;
	int32 ExpectedCommonRelics = 0;
	int32 ExpectedRareRelics = 0;
	int32 ExpectedEpicRelics = 0;
	for (const FExpectedQuality& Entry : ExpectedRelicEntries)
	{
		const FName Id(Entry.Id);
		TestFalse(FString::Printf(TEXT("independent relic authority ID is unique: %s"), Entry.Id), ExpectedRelics.Contains(Id));
		if (!ExpectedRelics.Contains(Id))
		{
			ExpectedRelics.Add(Id, Entry.Quality);
		}
		switch (Entry.Quality)
		{
		case EGameXXKCardQuality::Common: ++ExpectedCommonRelics; break;
		case EGameXXKCardQuality::Rare: ++ExpectedRareRelics; break;
		case EGameXXKCardQuality::Epic: ++ExpectedEpicRelics; break;
		default: AddError(FString::Printf(TEXT("independent relic authority has invalid quality: %s"), Entry.Id)); break;
		}
	}
	TestEqual(TEXT("independent Common relic count"), ExpectedCommonRelics, 15);
	TestEqual(TEXT("independent Rare relic count"), ExpectedRareRelics, 10);
	TestEqual(TEXT("independent Epic relic count"), ExpectedEpicRelics, 5);

	const TArray<FGameXXKRelicDefinition>& RelicDefinitions = FGameXXKRelicCatalog::GetAllDefinitions();
	TestEqual(TEXT("relic catalog contains exactly 30 definitions"), RelicDefinitions.Num(), 30);
	TSet<FName> ActualRelicIds;
	int32 ActualCommonRelics = 0;
	int32 ActualRareRelics = 0;
	int32 ActualEpicRelics = 0;
	for (const FGameXXKRelicDefinition& Definition : RelicDefinitions)
	{
		TestFalse(FString::Printf(TEXT("actual relic catalog ID is unique: %s"), *Definition.Id.ToString()), ActualRelicIds.Contains(Definition.Id));
		ActualRelicIds.Add(Definition.Id);
		const EGameXXKCardQuality* ExpectedQuality = ExpectedRelics.Find(Definition.Id);
		TestNotNull(FString::Printf(TEXT("actual relic ID is present in independent authority: %s"), *Definition.Id.ToString()), ExpectedQuality);
		if (ExpectedQuality)
		{
			TestEqual(FString::Printf(TEXT("relic catalog has independent approved quality: %s"), *Definition.Id.ToString()), Definition.BaseQuality, *ExpectedQuality);
			TestEqual(FString::Printf(TEXT("relic rule has independent approved quality: %s"), *Definition.Id.ToString()),
				FGameXXKCardQualityRules::GetRelicBaseQuality(Definition.Id), *ExpectedQuality);
		}
		TestTrue(FString::Printf(TEXT("relic has concrete quality: %s"), *Definition.Id.ToString()), IsConcreteQuality(Definition.BaseQuality));
		switch (Definition.BaseQuality)
		{
		case EGameXXKCardQuality::Common: ++ActualCommonRelics; break;
		case EGameXXKCardQuality::Rare: ++ActualRareRelics; break;
		case EGameXXKCardQuality::Epic: ++ActualEpicRelics; break;
		default: break;
		}
	}
	for (const TPair<FName, EGameXXKCardQuality>& Expected : ExpectedRelics)
	{
		TestTrue(FString::Printf(TEXT("independent relic authority ID is present in actual catalog: %s"), *Expected.Key.ToString()), ActualRelicIds.Contains(Expected.Key));
	}
	TestEqual(TEXT("actual relic catalog has no missing or extra unique IDs"), ActualRelicIds.Num(), ExpectedRelics.Num());
	TestEqual(TEXT("Common relic count"), ActualCommonRelics, 15);
	TestEqual(TEXT("Rare relic count"), ActualRareRelics, 10);
	TestEqual(TEXT("Epic relic count"), ActualEpicRelics, 5);

	FString RelicValidationError;
	TestTrue(TEXT("production relic quality invariants accept the real catalog"),
		FGameXXKCardQualityRules::ValidateRelicCatalog(RelicDefinitions, RelicValidationError));
	TestTrue(TEXT("valid relic quality catalog has no validation error"), RelicValidationError.IsEmpty());
	TArray<FGameXXKRelicDefinition> ReplacedRelicCatalog = RelicDefinitions;
	const int32 EpicRelicIndex = ReplacedRelicCatalog.IndexOfByPredicate([](const FGameXXKRelicDefinition& Definition)
	{
		return Definition.Id == TEXT("Relic.BambooTally");
	});
	TestTrue(TEXT("tampered relic fixture finds an explicitly classified relic"), EpicRelicIndex != INDEX_NONE);
	if (EpicRelicIndex != INDEX_NONE)
	{
		ReplacedRelicCatalog[EpicRelicIndex].Id = TEXT("Missing.Replacement.Relic");
		ReplacedRelicCatalog[EpicRelicIndex].BaseQuality = EGameXXKCardQuality::Common;
		FString ReplacedRelicError;
		TestFalse(TEXT("production relic validator rejects a catalog that replaces a classified ID"),
			FGameXXKCardQualityRules::ValidateRelicCatalog(ReplacedRelicCatalog, ReplacedRelicError));
		TestFalse(TEXT("rejected relic catalog explains the invariant failure"), ReplacedRelicError.IsEmpty());
	}

	const FName MissingRelic(TEXT("Missing.Relic"));
	TestNull(TEXT("unknown relic sentinel is absent from catalog"), FGameXXKRelicCatalog::FindDefinition(MissingRelic));
	TestEqual(TEXT("unknown relic IDs default to Common"),
		FGameXXKCardQualityRules::GetRelicBaseQuality(MissingRelic), EGameXXKCardQuality::Common);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardQualityScalingTest,
	"GameXXK.Data.CardQuality.Scaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardQualityScalingTest::RunTest(const FString& Parameters)
{
	const TArray<FEffectScalingExpectation>& Expectations = GetEffectScalingExpectations();
	TestEqual(TEXT("scaling table covers every declared concrete effect type"), Expectations.Num(), 28);
	TSet<EGameXXKCardEffectType> CoveredEffectTypes;
	for (const FEffectScalingExpectation& Expectation : Expectations)
	{
		TestFalse(TEXT("scaling table contains no duplicate effect type"), CoveredEffectTypes.Contains(Expectation.Type));
		CoveredEffectTypes.Add(Expectation.Type);
	}
	for (int32 EffectTypeValue = 1; EffectTypeValue <= 28; ++EffectTypeValue)
	{
		TestTrue(
			FString::Printf(TEXT("scaling table explicitly covers effect type %d"), EffectTypeValue),
			CoveredEffectTypes.Contains(static_cast<EGameXXKCardEffectType>(EffectTypeValue)));
	}

	FGameXXKCardDefinition BaseDefinition;
	BaseDefinition.Id = TEXT("Test.CardQuality.Scaling");
	BaseDefinition.DisplayName = FText::FromString(TEXT("品质缩放测试牌"));
	BaseDefinition.Owner = EGameXXKCardOwner::Route;
	BaseDefinition.Rarity = EGameXXKCardRarity::Boss;
	BaseDefinition.BaseQuality = EGameXXKCardQuality::Common;
	BaseDefinition.EnergyCost = 1;
	BaseDefinition.ManaCost = 4;
	BaseDefinition.TargetSpec.Mode = EGameXXKCardTargetMode::SingleEnemy;
	BaseDefinition.TargetSpec.RequiredStatusMinimumStacks = 6;
	BaseDefinition.TargetSpec.MinimumHealthPercent = 12.5f;
	BaseDefinition.TargetSpec.MaximumHealthPercent = 87.5f;

	const FGameXXKCardEffectCondition SentinelCondition = MakeScalingSentinelCondition();
	for (const FEffectScalingExpectation& Expectation : Expectations)
	{
		FGameXXKCardEffect Effect;
		Effect.Type = Expectation.Type;
		Effect.Target = EGameXXKCardEffectTarget::SelectedTarget;
		Effect.Magnitude = Expectation.BaseMagnitude;
		Effect.SecondaryMagnitude = 17;
		Effect.HitCount = 3;
		Effect.Status = EGameXXKCardStatus::Poison;
		Effect.Condition = SentinelCondition;
		BaseDefinition.Effects.Add(Effect);

		FGameXXKCardEffect ModifierEffect;
		ModifierEffect.Type = EGameXXKCardEffectType::ApplyBattleModifier;
		ModifierEffect.Target = EGameXXKCardEffectTarget::CardOwner;
		ModifierEffect.Magnitude = 29;
		ModifierEffect.SecondaryMagnitude = 19;
		ModifierEffect.HitCount = 4;
		ModifierEffect.Condition = SentinelCondition;
		ModifierEffect.Modifier.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
		ModifierEffect.Modifier.EffectType = Expectation.Type;
		ModifierEffect.Modifier.Target = EGameXXKCardEffectTarget::SelectedTarget;
		ModifierEffect.Modifier.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
		ModifierEffect.Modifier.Expiry = EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount;
		ModifierEffect.Modifier.Status = EGameXXKCardStatus::Bleed;
		ModifierEffect.Modifier.Magnitude = Expectation.BaseMagnitude;
		ModifierEffect.Modifier.RemainingTriggers = 2;
		ModifierEffect.Modifier.MinimumResult = 23;
		ModifierEffect.Modifier.bPersistent = true;
		ModifierEffect.Modifier.Condition = SentinelCondition;
		BaseDefinition.Effects.Add(ModifierEffect);
	}

	struct FQualityExpectation
	{
		EGameXXKCardQuality Quality;
		int32 FEffectScalingExpectation::* ExpectedMagnitude;
	};
	const FQualityExpectation QualityExpectations[] = {
		{ EGameXXKCardQuality::Common, &FEffectScalingExpectation::CommonMagnitude },
		{ EGameXXKCardQuality::Rare, &FEffectScalingExpectation::RareMagnitude },
		{ EGameXXKCardQuality::Epic, &FEffectScalingExpectation::EpicMagnitude }
	};

	auto TestConditionUnchanged = [this](const FString& Prefix, const FGameXXKCardEffectCondition& Actual)
	{
		const FGameXXKCardEffectCondition Expected = MakeScalingSentinelCondition();
		TestEqual(Prefix + TEXT(" type"), Actual.Type, Expected.Type);
		TestEqual(Prefix + TEXT(" status"), Actual.Status, Expected.Status);
		TestEqual(Prefix + TEXT(" minimum status stacks"), Actual.MinimumStatusStacks, Expected.MinimumStatusStacks);
		TestEqual(Prefix + TEXT(" minimum armor"), Actual.MinimumArmor, Expected.MinimumArmor);
		TestEqual(Prefix + TEXT(" health percentage threshold"), Actual.HealthPercentThreshold, Expected.HealthPercentThreshold);
		TestEqual(Prefix + TEXT(" terrain"), Actual.Terrain, Expected.Terrain);
		TestEqual(Prefix + TEXT(" alternate terrain"), Actual.AlternateTerrain, Expected.AlternateTerrain);
		TestEqual(Prefix + TEXT(" consume-status flag"), Actual.bConsumeStatus, Expected.bConsumeStatus);
		TestEqual(Prefix + TEXT(" maximum consumed status stacks"), Actual.MaxConsumedStatusStacks, Expected.MaxConsumedStatusStacks);
		TestEqual(Prefix + TEXT(" consumed-stack scaling flag"), Actual.bScaleMagnitudeByConsumedStacks, Expected.bScaleMagnitudeByConsumedStacks);
		TestEqual(Prefix + TEXT(" consume-armor flag"), Actual.bConsumeOwnerArmor, Expected.bConsumeOwnerArmor);
		TestEqual(Prefix + TEXT(" maximum consumed armor"), Actual.MaxConsumedArmor, Expected.MaxConsumedArmor);
		TestEqual(Prefix + TEXT(" negate flag"), Actual.bNegate, Expected.bNegate);
	};

	for (const FQualityExpectation& QualityExpectation : QualityExpectations)
	{
		const FGameXXKCardDefinition Effective = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			BaseDefinition,
			QualityExpectation.Quality);
		TestEqual(TEXT("effective definition retains every source effect"), Effective.Effects.Num(), BaseDefinition.Effects.Num());
		TestEqual(TEXT("energy cost is quality-invariant"), Effective.EnergyCost, BaseDefinition.EnergyCost);
		TestEqual(TEXT("mana cost is quality-invariant"), Effective.ManaCost, BaseDefinition.ManaCost);
		TestEqual(TEXT("base-quality metadata remains the catalog base quality"), Effective.BaseQuality, BaseDefinition.BaseQuality);
		TestEqual(TEXT("target status threshold is quality-invariant"), Effective.TargetSpec.RequiredStatusMinimumStacks, BaseDefinition.TargetSpec.RequiredStatusMinimumStacks);
		TestEqual(TEXT("target minimum health percentage is quality-invariant"), Effective.TargetSpec.MinimumHealthPercent, BaseDefinition.TargetSpec.MinimumHealthPercent);
		TestEqual(TEXT("target maximum health percentage is quality-invariant"), Effective.TargetSpec.MaximumHealthPercent, BaseDefinition.TargetSpec.MaximumHealthPercent);

		for (int32 CaseIndex = 0; CaseIndex < Expectations.Num(); ++CaseIndex)
		{
			const FEffectScalingExpectation& Expectation = Expectations[CaseIndex];
			const int32 ExpectedMagnitude = Expectation.*(QualityExpectation.ExpectedMagnitude);
			const FGameXXKCardEffect& DirectEffect = Effective.Effects[CaseIndex * 2];
			const FGameXXKCardEffect& ModifierEffect = Effective.Effects[CaseIndex * 2 + 1];
			const FString CaseLabel = FString::Printf(TEXT("effect %d at quality %d"), static_cast<int32>(Expectation.Type), static_cast<int32>(QualityExpectation.Quality));

			TestEqual(CaseLabel + TEXT(" direct magnitude"), DirectEffect.Magnitude, ExpectedMagnitude);
			TestEqual(CaseLabel + TEXT(" direct secondary magnitude"), DirectEffect.SecondaryMagnitude, 17);
			TestEqual(CaseLabel + TEXT(" direct hit count"), DirectEffect.HitCount, 3);
			TestConditionUnchanged(CaseLabel + TEXT(" direct condition"), DirectEffect.Condition);

			TestEqual(CaseLabel + TEXT(" outer modifier magnitude"), ModifierEffect.Magnitude, 29);
			TestEqual(CaseLabel + TEXT(" outer modifier secondary magnitude"), ModifierEffect.SecondaryMagnitude, 19);
			TestEqual(CaseLabel + TEXT(" outer modifier hit count"), ModifierEffect.HitCount, 4);
			TestConditionUnchanged(CaseLabel + TEXT(" outer modifier condition"), ModifierEffect.Condition);
			TestEqual(CaseLabel + TEXT(" nested modifier trigger"), ModifierEffect.Modifier.Trigger, EGameXXKCardBattleModifierTrigger::OnCardPlayed);
			TestEqual(CaseLabel + TEXT(" nested modifier effect type"), ModifierEffect.Modifier.EffectType, Expectation.Type);
			TestEqual(CaseLabel + TEXT(" nested modifier target"), ModifierEffect.Modifier.Target, EGameXXKCardEffectTarget::SelectedTarget);
			TestEqual(CaseLabel + TEXT(" nested modifier recipient scope"), ModifierEffect.Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::SharedDeck);
			TestEqual(CaseLabel + TEXT(" nested modifier status"), ModifierEffect.Modifier.Status, EGameXXKCardStatus::Bleed);
			TestEqual(CaseLabel + TEXT(" nested modifier magnitude"), ModifierEffect.Modifier.Magnitude, ExpectedMagnitude);
			TestEqual(CaseLabel + TEXT(" nested modifier remaining triggers"), ModifierEffect.Modifier.RemainingTriggers, 2);
			TestEqual(CaseLabel + TEXT(" nested modifier minimum result"), ModifierEffect.Modifier.MinimumResult, 23);
			TestEqual(CaseLabel + TEXT(" nested modifier expiry"), ModifierEffect.Modifier.Expiry, EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount);
			TestTrue(CaseLabel + TEXT(" nested modifier persistence"), ModifierEffect.Modifier.bPersistent);
			TestConditionUnchanged(CaseLabel + TEXT(" nested modifier condition"), ModifierEffect.Modifier.Condition);
		}
	}

	TestEqual(TEXT("building effective definitions does not mutate source energy cost"), BaseDefinition.EnergyCost, 1);
	TestEqual(TEXT("building effective definitions does not mutate source mana cost"), BaseDefinition.ManaCost, 4);
	TestEqual(TEXT("building effective definitions does not mutate source base quality"), BaseDefinition.BaseQuality, EGameXXKCardQuality::Common);
	for (int32 CaseIndex = 0; CaseIndex < Expectations.Num(); ++CaseIndex)
	{
		TestEqual(TEXT("building effective definitions does not mutate a source direct magnitude"),
			BaseDefinition.Effects[CaseIndex * 2].Magnitude,
			Expectations[CaseIndex].BaseMagnitude);
		TestEqual(TEXT("building effective definitions does not mutate a source nested modifier magnitude"),
			BaseDefinition.Effects[CaseIndex * 2 + 1].Modifier.Magnitude,
			Expectations[CaseIndex].BaseMagnitude);
	}

	FGameXXKCardDefinition InvalidQualityFallback = BaseDefinition;
	InvalidQualityFallback.BaseQuality = EGameXXKCardQuality::Rare;
	TestEqual(TEXT("invalid requested quality falls back to the definition base quality"),
		FGameXXKCardQualityRules::BuildEffectiveDefinition(InvalidQualityFallback, EGameXXKCardQuality::Invalid).Effects[0].Magnitude,
		14);
	InvalidQualityFallback.BaseQuality = EGameXXKCardQuality::Invalid;
	TestEqual(TEXT("two invalid qualities safely fall back to Common"),
		FGameXXKCardQualityRules::BuildEffectiveDefinition(InvalidQualityFallback, EGameXXKCardQuality::Invalid).Effects[0].Magnitude,
		7);

	FGameXXKCardDefinition OverflowDefinition;
	FGameXXKCardEffect OverflowDamage;
	OverflowDamage.Type = EGameXXKCardEffectType::DamageFlat;
	OverflowDamage.Magnitude = TNumericLimits<int32>::Max();
	OverflowDefinition.Effects.Add(OverflowDamage);
	FGameXXKCardEffect OverflowNestedDamage;
	OverflowNestedDamage.Type = EGameXXKCardEffectType::ApplyBattleModifier;
	OverflowNestedDamage.Modifier.EffectType = EGameXXKCardEffectType::DamageFlat;
	OverflowNestedDamage.Modifier.Magnitude = TNumericLimits<int32>::Min();
	OverflowDefinition.Effects.Add(OverflowNestedDamage);
	FGameXXKCardEffect OverflowDraw;
	OverflowDraw.Type = EGameXXKCardEffectType::DrawCards;
	OverflowDraw.Magnitude = TNumericLimits<int32>::Max();
	OverflowDefinition.Effects.Add(OverflowDraw);
	FGameXXKCardEffect OverflowNestedMana;
	OverflowNestedMana.Type = EGameXXKCardEffectType::ApplyBattleModifier;
	OverflowNestedMana.Modifier.EffectType = EGameXXKCardEffectType::GainMana;
	OverflowNestedMana.Modifier.Magnitude = TNumericLimits<int32>::Max();
	OverflowDefinition.Effects.Add(OverflowNestedMana);
	const FGameXXKCardDefinition ClampedDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
		OverflowDefinition,
		EGameXXKCardQuality::Epic);
	TestEqual(TEXT("x4 positive overflow clamps to int32 maximum"), ClampedDefinition.Effects[0].Magnitude, TNumericLimits<int32>::Max());
	TestEqual(TEXT("outer ApplyBattleModifier zero magnitude remains zero for nested damage"), ClampedDefinition.Effects[1].Magnitude, 0);
	TestEqual(TEXT("x4 negative overflow clamps to int32 minimum"), ClampedDefinition.Effects[1].Modifier.Magnitude, TNumericLimits<int32>::Min());
	TestEqual(TEXT("additive effect overflow clamps to int32 maximum"), ClampedDefinition.Effects[2].Magnitude, TNumericLimits<int32>::Max());
	TestEqual(TEXT("outer ApplyBattleModifier zero magnitude remains zero for nested mana"), ClampedDefinition.Effects[3].Magnitude, 0);
	TestEqual(TEXT("nested additive overflow clamps to int32 maximum"), ClampedDefinition.Effects[3].Modifier.Magnitude, TNumericLimits<int32>::Max());

	TestEqual(TEXT("Common card price"), FGameXXKCardQualityRules::GetCardPrice(EGameXXKCardQuality::Common), 25);
	TestEqual(TEXT("Rare card price"), FGameXXKCardQualityRules::GetCardPrice(EGameXXKCardQuality::Rare), 40);
	TestEqual(TEXT("Epic card price"), FGameXXKCardQualityRules::GetCardPrice(EGameXXKCardQuality::Epic), 60);
	TestEqual(TEXT("invalid card quality has no price"), FGameXXKCardQualityRules::GetCardPrice(EGameXXKCardQuality::Invalid), 0);
	TestEqual(TEXT("Common relic price"), FGameXXKCardQualityRules::GetRelicPrice(EGameXXKCardQuality::Common), 70);
	TestEqual(TEXT("Rare relic price"), FGameXXKCardQualityRules::GetRelicPrice(EGameXXKCardQuality::Rare), 100);
	TestEqual(TEXT("Epic relic price"), FGameXXKCardQualityRules::GetRelicPrice(EGameXXKCardQuality::Epic), 140);
	TestEqual(TEXT("invalid relic quality has no price"), FGameXXKCardQualityRules::GetRelicPrice(EGameXXKCardQuality::Invalid), 0);

	TestEqual(TEXT("Common display name"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Common).ToString(), FString(TEXT("普通")));
	TestEqual(TEXT("Rare display name"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Rare).ToString(), FString(TEXT("稀有")));
	TestEqual(TEXT("Epic display name"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Epic).ToString(), FString(TEXT("珍稀")));
	TestEqual(TEXT("invalid display name"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Invalid).ToString(), FString(TEXT("无效品质")));

	TestTrue(TEXT("Common display color matches the approved warm neutral"),
		FGameXXKCardQualityRules::GetDisplayColor(EGameXXKCardQuality::Common).Equals(FLinearColor(0.94f, 0.91f, 0.82f, 1.0f)));
	TestTrue(TEXT("Rare display color matches the approved blue"),
		FGameXXKCardQualityRules::GetDisplayColor(EGameXXKCardQuality::Rare).Equals(FLinearColor(0.30f, 0.58f, 0.86f, 1.0f)));
	TestTrue(TEXT("Epic display color matches the approved purple"),
		FGameXXKCardQualityRules::GetDisplayColor(EGameXXKCardQuality::Epic).Equals(FLinearColor(0.55f, 0.35f, 0.78f, 1.0f)));
	const FLinearColor InvalidColor = FGameXXKCardQualityRules::GetDisplayColor(EGameXXKCardQuality::Invalid);
	TestTrue(TEXT("invalid display color is a finite opaque neutral"),
		FMath::IsFinite(InvalidColor.R)
		&& FMath::IsFinite(InvalidColor.G)
		&& FMath::IsFinite(InvalidColor.B)
		&& FMath::IsNearlyEqual(InvalidColor.R, InvalidColor.G)
		&& FMath::IsNearlyEqual(InvalidColor.G, InvalidColor.B)
		&& FMath::IsNearlyEqual(InvalidColor.A, 1.0f));

	return true;
}

#endif
