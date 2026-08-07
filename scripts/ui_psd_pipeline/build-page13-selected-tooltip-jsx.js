import fs from 'node:fs';
import path from 'node:path';

function parseArguments(values) {
  const required = ['psd', 'paper', 'slot', 'output', 'report', 'export-dir'];
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
requireFile(args.paper, 'approved paper component');
requireFile(args.slot, 'approved detail slot component');
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
fs.mkdirSync(args['export-dir'], { recursive: true });

const exportRecords = [
  { name: '03_主角背包', origin: [6120, 0] },
  { name: '13_主角背包_物品选中', origin: [6120, 2400] },
].map((page) => ({
  ...page,
  path: path.join(args['export-dir'], `${page.name}.png`).replace(/\\/g, '/'),
}));

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var paperPath = ${jsxString(args.paper)};
  var slotPath = ${jsxString(args.slot)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exportRecords)};
  var baseOrigin = [6120, 0];
  var targetOrigin = [6120, 2400];
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) {
    return String(value || '').replace(/\\\\/g, '/').toLowerCase();
  }

  function containsText(value, fragment) {
    return String(value).indexOf(fragment) >= 0;
  }

  function findOpenDocument(file) {
    var expected = normalizedPath(file.fsName);
    for (var index = 0; index < app.documents.length; index++) {
      var candidate = app.documents[index];
      try { if (normalizedPath(candidate.fullName.fsName) == expected) return candidate; } catch (ignored) {}
    }
    return null;
  }

  function findDirectLayerSet(container, name) {
    for (var index = 0; index < container.layerSets.length; index++) {
      if (container.layerSets[index].name == name) return container.layerSets[index];
    }
    return null;
  }

  function requireDirectLayerSet(container, name) {
    var group = findDirectLayerSet(container, name);
    if (!group) throw new Error('Missing required group ' + name + ' in ' + container.name);
    return group;
  }

  function findDirectArtLayer(container, name) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (container.artLayers[index].name == name) return container.artLayers[index];
    }
    return null;
  }

  function findDirectArtLayerContaining(container, fragment) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (containsText(container.artLayers[index].name, fragment)) return container.artLayers[index];
    }
    return null;
  }

  function pixels(value) {
    try { return Number(value.as('px')); } catch (ignored) { return Number(value); }
  }

  function boundsOf(layer) {
    var bounds = layer.bounds;
    return [pixels(bounds[0]), pixels(bounds[1]), pixels(bounds[2]), pixels(bounds[3])];
  }

  function roundBounds(bounds) {
    var result = [];
    for (var index = 0; index < bounds.length; index++) result.push(Math.round(bounds[index]));
    return result;
  }

  function localBounds(layer, origin) {
    var bounds = roundBounds(boundsOf(layer));
    return [bounds[0] - origin[0], bounds[1] - origin[1], bounds[2] - origin[0], bounds[3] - origin[1]];
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
    if (findDirectArtLayer(group, name)) throw new Error('Tooltip text already exists: ' + name);
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

  function importRaster(filePath, targetGroup, name, left, top, width, height) {
    var sourceFile = new File(filePath);
    if (!sourceFile.exists) throw new Error('Missing raster source: ' + filePath);
    var sourceDocument = null;
    try {
      sourceDocument = app.open(sourceFile);
      sourceDocument.resizeImage(UnitValue(width, 'px'), UnitValue(height, 'px'), null, ResampleMethod.BICUBICSHARPER);
      var duplicated = sourceDocument.activeLayer.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
      sourceDocument.close(SaveOptions.DONOTSAVECHANGES);
      sourceDocument = null;
      app.activeDocument = doc;
      duplicated.name = name;
      var bounds = boundsOf(duplicated);
      duplicated.translate(UnitValue(left - bounds[0], 'px'), UnitValue(top - bounds[1], 'px'));
      duplicated.move(targetGroup, ElementPlacement.INSIDE);
      duplicated.visible = true;
      return duplicated;
    } finally {
      if (sourceDocument) {
        try { sourceDocument.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
      }
    }
  }

  function duplicateIconToSlot(sourceIcon, targetGroup, targetSlot) {
    var copied = sourceIcon.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    copied.move(targetGroup, ElementPlacement.INSIDE);
    copied.name = '03_SelectedItemIcon_XuanJiaHead';
    copied.visible = true;
    var slotBounds = boundsOf(targetSlot);
    var iconBounds = boundsOf(copied);
    var slotCenterX = (slotBounds[0] + slotBounds[2]) / 2;
    var slotCenterY = (slotBounds[1] + slotBounds[3]) / 2;
    var iconCenterX = (iconBounds[0] + iconBounds[2]) / 2;
    var iconCenterY = (iconBounds[1] + iconBounds[3]) / 2;
    copied.translate(UnitValue(slotCenterX - iconCenterX, 'px'), UnitValue(slotCenterY - iconCenterY, 'px'));
    return copied;
  }

  function exportPage(record) {
    var exportDocument = doc.duplicate('GameXXK_Page13Tooltip_' + record.name, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(record.origin[0], 'px'),
      UnitValue(record.origin[1], 'px'),
      UnitValue(record.origin[0] + 1920, 'px'),
      UnitValue(record.origin[1] + 1080, 'px')
    ]);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
    var pngOptions = new PNGSaveOptions();
    pngOptions.interlaced = false;
    exportDocument.saveAs(new File(record.path), pngOptions, true, Extension.LOWERCASE);
    exportDocument.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
  }

  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
  }

  function escapeJsonString(value) {
    var slash = String.fromCharCode(92);
    var quote = String.fromCharCode(34);
    return String(value)
      .split(slash).join(slash + slash)
      .split(quote).join(slash + quote)
      .split(String.fromCharCode(13)).join(slash + 'r')
      .split(String.fromCharCode(10)).join(slash + 'n')
      .split(String.fromCharCode(9)).join(slash + 't');
  }

  function stringifyJson(value) {
    if (value === null) return 'null';
    var valueType = typeof value;
    if (valueType == 'string') return '"' + escapeJsonString(value) + '"';
    if (valueType == 'number') return isFinite(value) ? String(value) : 'null';
    if (valueType == 'boolean') return value ? 'true' : 'false';
    if (value instanceof Array) {
      var parts = [];
      for (var arrayIndex = 0; arrayIndex < value.length; arrayIndex++) parts.push(stringifyJson(value[arrayIndex]));
      return '[' + parts.join(',') + ']';
    }
    var objectParts = [];
    for (var key in value) {
      if (value.hasOwnProperty(key)) objectParts.push('"' + escapeJsonString(key) + '":' + stringifyJson(value[key]));
    }
    return '{' + objectParts.join(',') + '}';
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before page-13 tooltip pass');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');

    var basePage = requireDirectLayerSet(doc, '03_主角背包');
    var targetPage = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    var baseSelection = requireDirectLayerSet(basePage, '39_SelectedSlotInk');
    var targetSelection = requireDirectLayerSet(targetPage, '39_SelectedSlotInk');
    var targetSelectionInk = findDirectArtLayer(targetSelection, '01_SharedShopSelectionInk');
    if (baseSelection.visible || !targetSelection.visible || !targetSelectionInk || !targetSelectionInk.visible) throw new Error('Backpack selected-state visibility contract is broken');
    if (findDirectLayerSet(targetPage, '44_SelectedItemTooltip')) throw new Error('Page-13 tooltip already exists');

    var inventoryItems = requireDirectLayerSet(targetPage, '41_InventoryItems');
    var selectedSourceIcon = findDirectArtLayerContaining(inventoryItems, 'inventory_item_1');
    if (!selectedSourceIcon) throw new Error('Missing first selected inventory item icon');

    var tooltip = targetPage.layerSets.add();
    tooltip.name = '44_SelectedItemTooltip';
    tooltip.visible = true;
    var paper = importRaster(paperPath, tooltip, '01_TooltipPaper_CurrentParchment', targetOrigin[0] + 1110, targetOrigin[1] + 420, 420, 250);
    var slot = importRaster(slotPath, tooltip, '02_TooltipItemSlot', targetOrigin[0] + 1134, targetOrigin[1] + 454, 96, 102);
    var icon = duplicateIconToSlot(selectedSourceIcon, tooltip, slot);
    var texts = [
      addText(tooltip, '10_ItemName', '玄甲头冠', targetOrigin[0] + 1250, targetOrigin[1] + 475, 24, '#2B2822', true),
      addText(tooltip, '11_SetAndSlot', '头冠 · 玄甲套装', targetOrigin[0] + 1250, targetOrigin[1] + 508, 17, '#5B5143', false),
      addText(tooltip, '12_RarityAndLevel', '普通 · Lv. 1', targetOrigin[0] + 1250, targetOrigin[1] + 537, 17, '#5B5143', false),
      addText(tooltip, '13_BaseStats', '气血 +8    强化 +0', targetOrigin[0] + 1138, targetOrigin[1] + 586, 18, '#2B2822', true),
      addText(tooltip, '14_Description', '玄甲套装的头冠，沉稳厚实。', targetOrigin[0] + 1138, targetOrigin[1] + 619, 16, '#5B5143', false),
      addText(tooltip, '15_DecomposeYield', '分解获得：洗炼砂 5', targetOrigin[0] + 1138, targetOrigin[1] + 649, 16, '#5B5143', false)
    ];

    var paperBounds = localBounds(paper, targetOrigin);
    var slotBounds = localBounds(slot, targetOrigin);
    var iconBounds = localBounds(icon, targetOrigin);
    if (paperBounds[0] < 1090 || paperBounds[2] > 1550 || paperBounds[1] < 400 || paperBounds[3] > 700) {
      throw new Error('Tooltip paper is outside approved overlay area: ' + paperBounds.join(','));
    }
    if (paperBounds[1] < 414) throw new Error('Tooltip overlaps selected first-row item');
    if (paperBounds[2] >= 1600) throw new Error('Tooltip overlaps scrollbar safe area');
    if (slotBounds[0] < paperBounds[0] || slotBounds[2] > paperBounds[2] || slotBounds[1] < paperBounds[1] || slotBounds[3] > paperBounds[3]) {
      throw new Error('Tooltip item slot is outside the paper');
    }
    if (iconBounds[0] < slotBounds[0] || iconBounds[2] > slotBounds[2] || iconBounds[1] < slotBounds[1] || iconBounds[3] > slotBounds[3]) {
      throw new Error('Selected tooltip icon is outside its detail slot');
    }
    for (var textIndex = 0; textIndex < texts.length; textIndex++) {
      var textBounds = localBounds(texts[textIndex], targetOrigin);
      if (textBounds[0] < paperBounds[0] || textBounds[2] > paperBounds[2] || textBounds[1] < paperBounds[1] || textBounds[3] > paperBounds[3]) {
        throw new Error('Tooltip text is outside the paper: ' + texts[textIndex].name + ' ' + textBounds.join(','));
      }
    }

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();

    var report = {
      status: 'PASS',
      topLevelPageCount: doc.layerSets.length,
      targetPage: targetPage.name,
      tooltipGroup: tooltip.name,
      composition: ['current parchment paper', 'approved detail item slot', 'selected live item icon', 'runtime text fields'],
      paperBounds: paperBounds,
      slotBounds: slotBounds,
      iconBounds: iconBounds,
      selectedStateVisibleOnlyOnPage13: !baseSelection.visible && targetSelection.visible,
      textLayers: [
        '10_ItemName', '11_SetAndSlot', '12_RarityAndLevel', '13_BaseStats', '14_Description', '15_DecomposeYield'
      ],
      exports: exportRecords
    };
    writeUtf8(reportPath, stringifyJson(report));
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
