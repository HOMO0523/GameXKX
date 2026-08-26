#!/usr/bin/env node
// golden.test.js — 冻结已迁移页关键常量，防止生成文件与 PSD 漂移
// 用法: node test/golden.test.js   （退出码 0=通过）
"use strict";
const fs = require("fs");
const path = require("path");

const PROJECT = path.resolve(__dirname, "../../../../");
const GEN = (p) => path.join(PROJECT, "Source/GameXXK/Public/UI/Generated", p);

let failures = 0;
function expect(file, needle, hint) {
  const content = fs.readFileSync(GEN(file), "utf8");
  if (!content.includes(needle)) {
    console.error(`FAIL ${file}: missing "${needle}" (${hint})`);
    failures++;
  } else {
    console.log(`ok   ${needle.slice(0, 70)}`);
  }
}

console.log("=== Page07 golden (frozen from PSD export 20260813-143457) ===");
const f = "GameXXKPage07Layout.gen.h";
expect(f, "MetaShopTitleText_Text_Position{390.0f, 211.0f}", "标题位置 == 现有代码");
expect(f, "MetaShopTitleText_Text_Size{86.0f, 43.0f}", "标题尺寸");
expect(f, "FVector2D{410.0f, 300.0f}", "卡1位置");
expect(f, "FVector2D{1070.0f, 300.0f}", "卡4位置");
expect(f, "FVector2D{960.0f, 610.0f}", "卡7位置");
expect(f, "FVector2D{170.0f, 170.0f}", "卡尺寸");
expect(f, "ProductCardAnchors[7]", "7 卡锚点数组");
expect(f, "MetaShopDetailName_Text_Position{1405.0f, 579.0f}", "详情名位置");
expect(f, "psdSha256=a4b2bd5b4ee0065a3da10bea774269ecf22ed2c971353c024fe9687b3f190e75", "源 PSD 指纹");

if (failures) {
  console.error(`\n${failures} golden failure(s) — 生成文件与冻结基线不符，检查 PSD/导出或有意变更后更新测试`);
  process.exit(1);
}
console.log("\nAll golden checks passed.");
