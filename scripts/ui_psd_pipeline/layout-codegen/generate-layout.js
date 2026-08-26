#!/usr/bin/env node
// generate-layout.js — PSD 导出 JSON → UMG 布局常量头（构建期代码生成）
// 用法: node generate-layout.js --export-dir <时间戳目录> --page "07_商店交易" [--check]
// 输出: Source/GameXXK/Public/UI/Generated/GameXXKPageNNLayout.gen.h
"use strict";
const fs = require("fs");
const path = require("path");

const PROJECT = path.resolve(__dirname, "../../..");

function arg(name, def) {
  const i = process.argv.indexOf("--" + name);
  return i >= 0 && process.argv[i + 1] ? process.argv[i + 1] : def;
}
const EXPORT_DIR = arg("export-dir", "");
const PAGE = arg("page", "07_商店交易");
const CHECK_ONLY = process.argv.includes("--check");

if (!EXPORT_DIR) {
  console.error("usage: node generate-layout.js --export-dir <dir> --page <页名> [--check]");
  process.exit(1);
}

// ---------- 1. 输入 ----------
const exportPath = path.join(PROJECT, "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/CurrentMasterV1", EXPORT_DIR);
const json = JSON.parse(fs.readFileSync(path.join(exportPath, "master-v1-current-pages.json"), "utf8"));
const manifest = JSON.parse(fs.readFileSync(path.join(PROJECT, "SourceArt/UI/PSD/gamexxk-v4/ui-master/final-approved-runtime-assets-manifest.json"), "utf8"));
const configPath = path.join(__dirname, "config", PAGE.replace(/[^\d]/g, "") + ".config.json");
const config = fs.existsSync(configPath) ? JSON.parse(fs.readFileSync(configPath, "utf8")) : {};

const layers = json.relevantPageLayers[PAGE];
if (!layers) {
  console.error("page not found in export:", PAGE);
  process.exit(1);
}
const exportMeta = json.exports.find((e) => e.name === PAGE);
const origin = exportMeta ? exportMeta.origin : [layers[0].bounds[0], layers[0].bounds[1]];
const [ox, oy] = origin;

// ---------- 2. 工具 ----------
function normName(raw) {
  // 别名表优先（text_005 → TitleText 等语义名）
  const alias = (config.aliases || {})[String(raw)];
  if (alias) return alias;
  let n = String(raw);
  n = n.replace(/^\d+_/, ""); // 数字前缀
  n = n.replace(/\s*拷贝(\s*\d+)?$/, ""); // 拷贝后缀
  n = n.replace(/^text_/, "");
  n = n.replace(/^0\d+_/, "").replace(/^\d+/, "");
  const parts = n.split("_").filter(Boolean);
  return parts.map((p, i) => (i === 0 ? p.charAt(0).toUpperCase() + p.slice(1) : p.charAt(0).toUpperCase() + p.slice(1))).join("");
}
const pagePrefix = { "07_商店交易": "MetaShop", "02_城镇HUD": "TownHud", "03_主角背包": "Backpack", "18_主角背包_卡组页": "CardDeckPage" }[PAGE] || "Page";
const isLegacy = (name) => /^99_/.test(name) || /备份/.test(name);
const isStateGroup = (name) => /^\d+_State_/.test(name);
const isText = (l) => l.layerKind === "TEXT" || /^text_/.test(l.name);
const localRect = (b) => ({ x: b[0] - ox, y: b[1] - oy, w: b[2] - b[0], h: b[3] - b[1] });

// ---------- 3. 遍历（扁平化，跳过 legacy/垃圾层/组容器，只留 art 叶子层） ----------
const flat = [];
let index = 0;
for (const l of layers) {
  if (l.name === PAGE) continue; // 页面组本身
  if (isLegacy(l.name)) continue;
  if (l.kind === "group") continue; // 组=透明容器，不产出控件（子层已扁平在列表里）
  if (l.bounds[2] - l.bounds[0] === 0 && l.bounds[3] - l.bounds[1] === 0) continue;
  const ignore = (config.ignore || []).includes(l.name);
  if (ignore) continue;
  flat.push({ layer: l, index: index++ });
}
const total = flat.length;
const zOf = (i) => (total - 1 - i) * 10;

// 数组聚合：product_card_N / product_icon_N
function arrayOf(flat, key) {
  const re = new RegExp(key + "_(\\d+)$");
  const items = flat
    .map((f) => ({ m: f.layer.name.match(re), f }))
    .filter((x) => x.m)
    .sort((a, b) => Number(a.m[1]) - Number(b.m[1]));
  return items;
}

// ---------- 4. 生成 ----------
const L = [];
L.push(`// AUTO-GENERATED — DO NOT EDIT`);
L.push(`// source: ${EXPORT_DIR}/master-v1-current-pages.json psdSha256=${json.psdSha256}`);
L.push(`// page: ${PAGE} origin=(${ox},${oy})`);
L.push(`#pragma once`);
L.push(`#include "CoreMinimal.h"`);
L.push(`namespace GameXXKLayout::${pagePrefix} {`);

// 4a. 静态控件常量（text / 非数组控件）
const emitted = [];
for (const { layer: l, index: i } of flat) {
  if (/_(card|icon)_\d+$/.test(l.name)) continue; // 数组层单独处理
  const r = localRect(l.bounds);
  const nm = normName(l.name);
  const suffix = isText(l) ? "Text" : "Widget";
  const id = pagePrefix + nm + "_" + suffix;
  if (emitted.includes(id)) continue;
  emitted.push(id);
  L.push(`  inline constexpr FVector2D ${id}_Position{${r.x}.0f, ${r.y}.0f};`);
  L.push(`  inline constexpr FVector2D ${id}_Size{${r.w}.0f, ${r.h}.0f};`);
  L.push(`  inline constexpr int32 ${id}_ZOrder = ${zOf(i)};`);
}

// 4b. 商品卡/图标锚点数组
const cards = arrayOf(flat, "product_card");
const icons = arrayOf(flat, "product_icon");
if (cards.length) {
  L.push(`  struct FProductCardAnchor { FVector2D Position; FVector2D Size; FVector2D IconPosition; FVector2D IconSize; };`);
  L.push(`  inline constexpr FProductCardAnchor ProductCardAnchors[${cards.length}] = {`);
  for (let k = 0; k < cards.length; k++) {
    const cr = localRect(cards[k].f.layer.bounds);
    const ir = icons[k] ? localRect(icons[k].f.layer.bounds) : { x: 0, y: 0, w: 0, h: 0 };
    L.push(`    {FVector2D{${cr.x}.0f, ${cr.y}.0f}, FVector2D{${cr.w}.0f, ${cr.h}.0f}, FVector2D{${ir.x}.0f, ${ir.y}.0f}, FVector2D{${ir.w}.0f, ${ir.h}.0f}},`);
  }
  L.push(`  };`);
}

L.push(`}`);
L.push(``);

const outFile = path.join(PROJECT, "Source/GameXXK/Public/UI/Generated", `GameXXKPage${PAGE.replace(/[^\d]/g, "")}Layout.gen.h`);
const content = L.join("\n");

if (CHECK_ONLY) {
  if (!fs.existsSync(outFile)) {
    console.error("[check] generated file missing:", outFile);
    process.exit(1);
  }
  const old = fs.readFileSync(outFile, "utf8");
  if (old !== content) {
    console.error("[check] generated file out of date — rerun without --check");
    process.exit(1);
  }
  console.log("[check] OK — generated file matches export");
} else {
  fs.mkdirSync(path.dirname(outFile), { recursive: true });
  fs.writeFileSync(outFile, content);
  console.log("generated:", outFile);
  console.log("consts:", emitted.length, "cards:", cards.length);
}
