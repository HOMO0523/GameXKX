const fs = require('fs');
const path = require('path');

function parseArguments(values) {
  const required = [
    'psd',
    'currency-strip',
    'ingot',
    'selected-slot',
    'output',
    'report',
    'export-dir',
  ];
  const result = {};
  for (const name of required) {
    const flag = `--${name}`;
    const index = values.indexOf(flag);
    if (index === -1 || !values[index + 1] || values[index + 1].startsWith('--')) {
      throw new Error(`${flag} requires a value`);
    }
    result[name] = path.resolve(values[index + 1]);
  }
  return result;
}

function requireFile(filePath, label) {
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    throw new Error(`Missing ${label}: ${filePath}`);
  }
}

function jsxString(value) {
  return JSON.stringify(String(value).replace(/\\/g, '/'));
}

const args = parseArguments(process.argv.slice(2));
requireFile(args.psd, 'master PSD');
requireFile(args['currency-strip'], 'compact currency strip');
requireFile(args.ingot, 'ingot icon');
requireFile(args['selected-slot'], 'selected inventory slot');

for (const destination of [args.output, args.report]) {
  fs.mkdirSync(path.dirname(destination), { recursive: true });
}
fs.mkdirSync(args['export-dir'], { recursive: true });

const targets = [
  { name: '03_主角背包', origin: [6120, 0] },
  { name: '04_伙伴编队', origin: [8160, 0] },
  { name: '05_图鉴', origin: [0, 1200] },
  { name: '06_任务日志', origin: [2040, 1200] },
  { name: '07_商店交易', origin: [4080, 1200] },
  { name: '13_主角背包_物品选中', origin: [6120, 2400] },
  { name: '14_伙伴编队_角色选中', origin: [8160, 2400] },
  { name: '15_图鉴_怪物选中', origin: [0, 3600] },
];

const exportRecords = targets.map((target) => ({
  name: target.name,
  path: path.join(args['export-dir'], `${target.name}.png`).replace(/\\/g, '/'),
}));

const reportTemplate = {
  status: 'PASS',
  pageCount: 18,
  targetPages: targets.map((target) => target.name),
  currencyStripBox: [1570, 28, 320, 86],
  shopPriceIconCount: 10,
  shopInsufficientMessage: '元宝不足，还需 50',
  masterBackpackPage: '03_主角背包',
  selectedBackpackPage: '13_主角背包_物品选中',
  standaloneBackpackImported: false,
  statePairBaseSignatureMatch: true,
  nonTargetSignatureMatch: true,
  exports: exportRecords,
};

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var currencyStripPath = ${jsxString(args['currency-strip'])};
  var ingotPath = ${jsxString(args.ingot)};
  var selectedSlotPath = ${jsxString(args['selected-slot'])};
  var reportPath = ${jsxString(args.report)};
  var targetPages = ${JSON.stringify(targets)};
  var exportRecords = ${JSON.stringify(exportRecords)};
  var reportJson = ${JSON.stringify(JSON.stringify(reportTemplate))};
  var pageWidth = 1920;
  var pageHeight = 1080;
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) {
    return String(value || '').replace(/\\\\/g, '/').toLowerCase();
  }

  function containsText(value, fragment) {
    return String(value).indexOf(fragment) >= 0;
  }

  function beginsWith(value, prefix) {
    return String(value).substring(0, prefix.length) == prefix;
  }

  function arrayContains(values, value) {
    for (var index = 0; index < values.length; index++) {
      if (values[index] == value) return true;
    }
    return false;
  }

  function findOpenDocument(file) {
    var expected = normalizedPath(file.fsName);
    for (var index = 0; index < app.documents.length; index++) {
      var candidate = app.documents[index];
      try {
        if (normalizedPath(candidate.fullName.fsName) == expected) return candidate;
      } catch (ignored) {}
    }
    return null;
  }

  function findDirectLayerSet(container, name) {
    for (var index = 0; index < container.layerSets.length; index++) {
      if (container.layerSets[index].name == name) return container.layerSets[index];
    }
    return null;
  }

  function findDirectArtLayer(container, name) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (container.artLayers[index].name == name) return container.artLayers[index];
    }
    return null;
  }

  function requireDirectLayerSet(container, name) {
    var group = findDirectLayerSet(container, name);
    if (!group) throw new Error('Missing required group ' + name + ' in ' + container.name);
    return group;
  }

  function findTarget(name) {
    for (var index = 0; index < targetPages.length; index++) {
      if (targetPages[index].name == name) return targetPages[index];
    }
    return null;
  }

  function px(value) {
    try { return value.as('px'); } catch (ignored) { return Number(value); }
  }

  function layerBounds(layer) {
    var bounds = layer.bounds;
    return [px(bounds[0]), px(bounds[1]), px(bounds[2]), px(bounds[3])];
  }

  function colorFromHex(value) {
    var clean = String(value).replace('#', '');
    var color = new SolidColor();
    color.rgb.red = parseInt(clean.substring(0, 2), 16);
    color.rgb.green = parseInt(clean.substring(2, 4), 16);
    color.rgb.blue = parseInt(clean.substring(4, 6), 16);
    return color;
  }

  function addText(group, name, contents, x, baselineY, size, color, bold) {
    var layer = group.artLayers.add();
    layer.name = name;
    layer.kind = LayerKind.TEXT;
    var item = layer.textItem;
    item.contents = contents;
    item.position = [UnitValue(x, 'px'), UnitValue(baselineY, 'px')];
    item.size = UnitValue(size, 'pt');
    item.font = 'MicrosoftYaHei';
    item.color = colorFromHex(color);
    item.justification = Justification.LEFT;
    try { item.fauxBold = !!bold; } catch (ignored) {}
    try { item.antiAliasMethod = AntiAlias.SMOOTH; } catch (ignored2) {}
    return layer;
  }

  function importRaster(filePath, targetGroup, name, left, top, width, height, visible) {
    var sourceFile = new File(filePath);
    if (!sourceFile.exists) throw new Error('Missing raster source: ' + filePath);
    var sourceDocument = null;
    try {
      sourceDocument = app.open(sourceFile);
      sourceDocument.resizeImage(
        UnitValue(width, 'px'),
        UnitValue(height, 'px'),
        null,
        ResampleMethod.BICUBICSHARPER
      );
      var duplicated = sourceDocument.activeLayer.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
      sourceDocument.close(SaveOptions.DONOTSAVECHANGES);
      sourceDocument = null;
      app.activeDocument = doc;
      duplicated.name = name;
      var bounds = layerBounds(duplicated);
      duplicated.translate(UnitValue(left - bounds[0], 'px'), UnitValue(top - bounds[1], 'px'));
      duplicated.move(targetGroup, ElementPlacement.INSIDE);
      duplicated.visible = visible !== false;
      return duplicated;
    } finally {
      if (sourceDocument) {
        try { sourceDocument.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
      }
    }
  }

  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
  }

  function treeSignature(container, originX, originY) {
    var records = [];
    for (var index = 0; index < container.layers.length; index++) {
      var layer = container.layers[index];
      var record = layer.typename + ':' + layer.name + ':' + String(layer.visible);
      if (layer.typename == 'LayerSet') {
        record += '{' + treeSignature(layer, originX, originY) + '}';
      } else {
        var bounds = layerBounds(layer);
        record += ':[' +
          Math.round(bounds[0] - originX) + ',' +
          Math.round(bounds[1] - originY) + ',' +
          Math.round(bounds[2] - originX) + ',' +
          Math.round(bounds[3] - originY) + ']';
        try {
          if (layer.kind == LayerKind.TEXT) record += ':text=' + String(layer.textItem.contents);
        } catch (ignored) {}
      }
      records.push(record);
    }
    return records.join('|');
  }

  function nonTargetSignature(document) {
    var targetNames = [];
    for (var targetIndex = 0; targetIndex < targetPages.length; targetIndex++) {
      targetNames.push(targetPages[targetIndex].name);
    }
    var records = [];
    for (var index = 0; index < document.layerSets.length; index++) {
      var group = document.layerSets[index];
      if (!arrayContains(targetNames, group.name)) {
        records.push(group.name + '=' + treeSignature(group, 0, 0));
      }
    }
    return records.join('||');
  }

  function renameAndHideDirectGroup(page, name, legacyName) {
    if (findDirectLayerSet(page, legacyName)) throw new Error('Legacy group already exists: ' + legacyName);
    var group = requireDirectLayerSet(page, name);
    group.name = legacyName;
    group.visible = false;
    return group;
  }

  function cloneArtLayer(source, targetGroup, deltaX, deltaY) {
    var copied = source.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    copied.move(targetGroup, ElementPlacement.INSIDE);
    copied.name = source.name;
    if (deltaX || deltaY) {
      copied.translate(UnitValue(deltaX, 'px'), UnitValue(deltaY, 'px'));
    }
    copied.visible = source.visible;
    return copied;
  }

  function cloneLayerSet(source, targetParent, targetName, deltaX, deltaY, forceVisible) {
    var target = targetParent.layerSets.add();
    target.name = targetName;
    for (var index = source.layers.length - 1; index >= 0; index--) {
      var sourceLayer = source.layers[index];
      if (sourceLayer.typename == 'LayerSet') {
        cloneLayerSet(sourceLayer, target, sourceLayer.name, deltaX, deltaY, null);
      } else {
        cloneArtLayer(sourceLayer, target, deltaX, deltaY);
      }
    }
    try { target.opacity = source.opacity; } catch (ignored) {}
    try { target.blendMode = source.blendMode; } catch (ignored2) {}
    target.visible = forceVisible === null ? source.visible : forceVisible;
    return target;
  }

  function copyGroupWithDelta(sourcePage, targetPage, name, deltaX, deltaY) {
    var source = requireDirectLayerSet(sourcePage, name);
    if (findDirectLayerSet(targetPage, name)) throw new Error('Target group already exists before copy: ' + name);
    return cloneLayerSet(source, targetPage, name, deltaX, deltaY, true);
  }

  function duplicateTextByContents(sourceGroup, targetGroup, contents, name) {
    for (var index = 0; index < sourceGroup.artLayers.length; index++) {
      var layer = sourceGroup.artLayers[index];
      try {
        if (layer.kind == LayerKind.TEXT && String(layer.textItem.contents) == contents) {
          var copied = layer.duplicate();
          copied.move(targetGroup, ElementPlacement.INSIDE);
          copied.name = name;
          copied.visible = true;
          return copied;
        }
      } catch (ignored) {}
    }
    throw new Error('Missing selected-state text: ' + contents);
  }

  function findArtLayerContaining(container, fragment) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (containsText(container.artLayers[index].name, fragment)) return container.artLayers[index];
    }
    return null;
  }

  function rebuildSelectedBackpackState() {
    var basePage = requireDirectLayerSet(doc, '03_主角背包');
    var statePage = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    var oldRuntime = requireDirectLayerSet(statePage, '70_RuntimeText');
    var oldDetail = requireDirectLayerSet(statePage, '45_Selection');
    var oldActions = requireDirectLayerSet(statePage, '40_Actions');

    var legacyNames = [
      ['00_FamilyCorrection', '99_Legacy_00_FamilyCorrection_PreMaster03Parity'],
      ['22_Tabs', '99_Legacy_22_Tabs_PreMaster03Parity'],
      ['30_Character', '99_Legacy_30_Character_PreMaster03Parity'],
      ['31_Equipment', '99_Legacy_31_Equipment_PreMaster03Parity'],
      ['35_Grid', '99_Legacy_35_Grid_PreMaster03Parity'],
      ['40_Actions', '99_Legacy_40_Actions_PreMaster03Parity'],
      ['45_Selection', '99_Legacy_45_Selection_PreMaster03Parity'],
      ['42_InventoryScrollbar', '99_Legacy_42_InventoryScrollbar_PreMaster03Parity'],
      ['70_RuntimeText', '99_Legacy_70_RuntimeText_PreMaster03Parity']
    ];
    for (var legacyIndex = 0; legacyIndex < legacyNames.length; legacyIndex++) {
      renameAndHideDirectGroup(statePage, legacyNames[legacyIndex][0], legacyNames[legacyIndex][1]);
    }

    var baseGroupsBottomToTop = [
      '00_FamilyCorrection',
      '20_Tabs',
      '30_Character',
      '31_EquipmentFrames',
      '32_EquipmentIcons',
      '40_InventorySlots',
      '41_InventoryItems',
      '42_InventoryScrollbar',
      '70_RuntimeText'
    ];
    for (var groupIndex = 0; groupIndex < baseGroupsBottomToTop.length; groupIndex++) {
      copyGroupWithDelta(basePage, statePage, baseGroupsBottomToTop[groupIndex], 0, 2400);
    }

    cloneLayerSet(oldActions, statePage, '40_SelectedItemActions', 0, 0, true);
    cloneLayerSet(oldDetail, statePage, '45_SelectedItemDetail', 0, 0, true);

    var selectedSlotGroup = statePage.layerSets.add();
    selectedSlotGroup.name = '44_SelectedInventorySlot';
    importRaster(selectedSlotPath, selectedSlotGroup, '01_SelectedSlotFrame', 7255, 2700, 110, 116, true);
    var copiedItems = requireDirectLayerSet(statePage, '41_InventoryItems');
    var bagItem = findArtLayerContaining(copiedItems, 'inventory_item_1');
    if (!bagItem) throw new Error('Missing page-03 first inventory item for selected state');
    var selectedBag = bagItem.duplicate();
    selectedBag.move(selectedSlotGroup, ElementPlacement.INSIDE);
    selectedBag.name = '02_SelectedItem_Bag';
    selectedBag.visible = true;

    var selectedText = statePage.layerSets.add();
    selectedText.name = '46_SelectedItemRuntimeText';
    duplicateTextByContents(oldRuntime, selectedText, '小布袋', '01_ItemName');
    duplicateTextByContents(oldRuntime, selectedText, '普通布袋，能装下一些小物件。', '02_ItemDescription');
    duplicateTextByContents(oldRuntime, selectedText, '分解', '03_DismantleLabel');
    duplicateTextByContents(oldRuntime, selectedText, '使用', '04_UseLabel');

    statePage.visible = true;
  }

  function walkVisibleGroups(container, visitor, ancestorsVisible) {
    var currentVisible = ancestorsVisible && container.visible !== false;
    if (!currentVisible) return;
    visitor(container);
    for (var index = 0; index < container.layerSets.length; index++) {
      walkVisibleGroups(container.layerSets[index], visitor, currentVisible);
    }
  }

  function hideActiveTopCurrency(page) {
    var paperCount = 0;
    var iconCount = 0;
    var valueCount = 0;
    var balance = null;
    walkVisibleGroups(page, function (group) {
      for (var artIndex = 0; artIndex < group.artLayers.length; artIndex++) {
        var layer = group.artLayers[artIndex];
        if (containsText(layer.name, '顶部铜钱条')) {
          layer.name = '99_Legacy_LongCurrencyPaper';
          layer.visible = false;
          paperCount++;
          continue;
        }
        if (beginsWith(group.name, '20_GlobalShell')) {
          if (containsText(layer.name, '_coin_')) {
            layer.name = '99_Legacy_TopBalanceCopperIcon';
            layer.visible = false;
            iconCount++;
            continue;
          }
          try {
            if (layer.kind == LayerKind.TEXT && String(layer.textItem.contents) == '500') {
              balance = String(layer.textItem.contents);
              layer.name = '99_Legacy_TopBalanceValue';
              layer.visible = false;
              valueCount++;
            }
          } catch (ignored) {}
        }
      }
    }, true);
    if (paperCount != 1 || iconCount != 1 || valueCount != 1 || balance === null) {
      throw new Error('Unexpected active currency sources in ' + page.name + ': paper=' + paperCount + ', icon=' + iconCount + ', value=' + valueCount);
    }
    return balance;
  }

  function addCompactCurrency(page, originX, originY) {
    if (findDirectLayerSet(page, '01_CompactOutOfRunCurrency')) {
      throw new Error('Compact currency group already exists in ' + page.name);
    }
    var balance = hideActiveTopCurrency(page);
    var group = page.layerSets.add();
    group.name = '01_CompactOutOfRunCurrency';
    importRaster(currencyStripPath, group, '01_CompactCurrencyPaper_320', originX + 1570, originY + 28, 320, 86, true);
    importRaster(ingotPath, group, '02_IngotIcon', originX + 1672, originY + 50, 42, 42, true);
    addText(group, '03_IngotValue', balance, originX + 1728, originY + 82, 27, '#2B2822', true);
    group.visible = true;
    if (group.artLayers.length != 3) throw new Error('Compact currency group must contain three layers in ' + page.name);
  }

  function collectShopPriceCoins(container, output, inGlobalShell) {
    var isGlobal = inGlobalShell || beginsWith(container.name, '20_GlobalShell');
    if (!isGlobal && !beginsWith(container.name, '99_Legacy')) {
      for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
        var layer = container.artLayers[artIndex];
        if (containsText(layer.name, '_coin_')) output.push({ layer: layer, parent: container });
      }
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      collectShopPriceCoins(container.layerSets[groupIndex], output, isGlobal);
    }
  }

  function collectNumericPriceText(container, output, inPriceArea) {
    var isPriceArea = inPriceArea || container.name == '40_ProductGrid' || beginsWith(container.name, '71_State_') || beginsWith(container.name, '72_State_') || beginsWith(container.name, '73_State_');
    if (isPriceArea) {
      for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
        var layer = container.artLayers[artIndex];
        try {
          if (layer.kind == LayerKind.TEXT) {
            var contents = String(layer.textItem.contents);
            if (/^[0-9][0-9,]*$/.test(contents)) output.push(contents);
          }
        } catch (ignored) {}
      }
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      collectNumericPriceText(container.layerSets[groupIndex], output, isPriceArea);
    }
  }

  function updateInsufficientMessage(container) {
    var count = 0;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      try {
        if (layer.kind == LayerKind.TEXT && String(layer.textItem.contents) == '铜钱不足，还需 50') {
          layer.textItem.contents = '元宝不足，还需 50';
          count++;
        }
      } catch (ignored) {}
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      count += updateInsufficientMessage(container.layerSets[groupIndex]);
    }
    return count;
  }

  function replaceShopPriceCoins(shopPage) {
    var priceTextBefore = [];
    collectNumericPriceText(shopPage, priceTextBefore, false);
    var records = [];
    collectShopPriceCoins(shopPage, records, false);
    if (records.length != 10) throw new Error('Expected ten shop price coin layers, got ' + records.length);
    for (var index = 0; index < records.length; index++) {
      var source = records[index].layer;
      var parent = records[index].parent;
      var bounds = layerBounds(source);
      var wasVisible = source.visible;
      source.name = '99_Legacy_CopperPrice_' + ('0' + (index + 1)).slice(-2);
      source.visible = false;
      importRaster(
        ingotPath,
        parent,
        'IngotPrice_' + ('0' + (index + 1)).slice(-2),
        bounds[0], bounds[1], bounds[2] - bounds[0], bounds[3] - bounds[1], wasVisible
      );
    }
    if (updateInsufficientMessage(shopPage) != 1) throw new Error('Expected one insufficient-funds message');
    var priceTextAfter = [];
    collectNumericPriceText(shopPage, priceTextAfter, false);
    if (priceTextBefore.join('|') != priceTextAfter.join('|')) throw new Error('Shop numeric price text changed');
    return records.length;
  }

  function baseStateSignature(page, originX, originY) {
    var names = [
      '00_FamilyCorrection',
      '01_CompactOutOfRunCurrency',
      '20_Tabs',
      '30_Character',
      '31_EquipmentFrames',
      '32_EquipmentIcons',
      '40_InventorySlots',
      '41_InventoryItems',
      '42_InventoryScrollbar',
      '70_RuntimeText'
    ];
    var records = [];
    for (var index = 0; index < names.length; index++) {
      var group = requireDirectLayerSet(page, names[index]);
      records.push(names[index] + '=' + treeSignature(group, originX, originY));
    }
    return records.join('||');
  }

  function exportPage(pageRecord, exportPath) {
    var exportDocument = doc.duplicate('GameXXK_Confirmed_' + pageRecord.name, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(pageRecord.origin[0], 'px'),
      UnitValue(pageRecord.origin[1], 'px'),
      UnitValue(pageRecord.origin[0] + pageWidth, 'px'),
      UnitValue(pageRecord.origin[1] + pageHeight, 'px')
    ]);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) {
      exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
    }
    var pngOptions = new PNGSaveOptions();
    pngOptions.interlaced = false;
    exportDocument.saveAs(new File(exportPath), pngOptions, true, Extension.LOWERCASE);
    exportDocument.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) {
      doc = app.open(targetFile);
      openedByScript = true;
    }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes. Save or revert them before running this script.');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages before mutation');

    for (var preflightIndex = 0; preflightIndex < targetPages.length; preflightIndex++) {
      var preflightPage = requireDirectLayerSet(doc, targetPages[preflightIndex].name);
      if (findDirectLayerSet(preflightPage, '01_CompactOutOfRunCurrency')) {
        throw new Error('Target page already integrated: ' + preflightPage.name);
      }
    }
    var nonTargetsBefore = nonTargetSignature(doc);

    rebuildSelectedBackpackState();
    for (var currencyIndex = 0; currencyIndex < targetPages.length; currencyIndex++) {
      var target = targetPages[currencyIndex];
      addCompactCurrency(requireDirectLayerSet(doc, target.name), target.origin[0], target.origin[1]);
    }
    var shopCount = replaceShopPriceCoins(requireDirectLayerSet(doc, '07_商店交易'));
    if (shopCount != 10) throw new Error('Shop price icon count changed after replacement');

    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages after mutation');
    if (nonTargetsBefore != nonTargetSignature(doc)) throw new Error('A non-target page changed');
    var basePage = requireDirectLayerSet(doc, '03_主角背包');
    var statePage = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    if (baseStateSignature(basePage, 6120, 0) != baseStateSignature(statePage, 6120, 2400)) {
      throw new Error('Page 03 and page 13 base signatures differ');
    }
    if (!findDirectLayerSet(statePage, '44_SelectedInventorySlot')) throw new Error('Page 13 is missing selected slot treatment');
    if (!findDirectLayerSet(statePage, '45_SelectedItemDetail')) throw new Error('Page 13 is missing selected item detail');
    if (!findDirectLayerSet(statePage, '46_SelectedItemRuntimeText')) throw new Error('Page 13 is missing selected item text');

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) {
      exportPage(targetPages[exportIndex], exportRecords[exportIndex].path);
    }
    doc.save();
    writeUtf8(reportPath, reportJson);
    if (openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES);
  } catch (error) {
    try {
      if (doc && initialHistoryState) {
        app.activeDocument = doc;
        doc.activeHistoryState = initialHistoryState;
      }
      if (doc && openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES);
    } catch (rollbackError) {}
    try { writeUtf8(reportPath + '.error.txt', String(error)); } catch (writeError) {}
    throw error;
  } finally {
    app.preferences.rulerUnits = oldUnits;
    app.displayDialogs = oldDialogs;
  }
})();`;

fs.writeFileSync(args.output, jsx, 'utf8');
