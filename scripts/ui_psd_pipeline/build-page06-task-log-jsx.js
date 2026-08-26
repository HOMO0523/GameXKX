import fs from 'node:fs';
import path from 'node:path';

function parseArguments(values) {
  const required = ['psd', 'output', 'report', 'export-dir'];
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

// No backslash characters anywhere in this file: every JSX escape is built
// from String.fromCharCode so the JSON/heredoc layers cannot corrupt it.
function jsxString(value) {
  return JSON.stringify(String(value).split(String.fromCharCode(92)).join('/'));
}

const args = parseArguments(process.argv.slice(2));
requireFile(args.psd, 'master PSD');
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
fs.mkdirSync(args['export-dir'], { recursive: true });

const exportRecords = [
  { name: '06_任务日志', origin: [2040, 1200] },
].map((page) => ({
  ...page,
  path: path.join(args['export-dir'], `${page.name}.png`).split(String.fromCharCode(92)).join('/'),
}));

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exportRecords)};
  var pageOrigin = [2040, 1200];
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;
  var movedToLegacy = [];

  function boundsOf(layer) {
    var b = layer.bounds;
    return [Number(b[0].as('px')), Number(b[1].as('px')), Number(b[2].as('px')), Number(b[3].as('px'))];
  }
  function roundBounds(bounds) {
    return [Math.round(bounds[0]), Math.round(bounds[1]), Math.round(bounds[2]), Math.round(bounds[3])];
  }
  function localBounds(layer) {
    var b = roundBounds(boundsOf(layer));
    return [b[0] - pageOrigin[0], b[1] - pageOrigin[1], b[2] - pageOrigin[0], b[3] - pageOrigin[1]];
  }
  function findDirectLayerSet(container, name) {
    for (var i = 0; i < container.layerSets.length; i++) {
      if (container.layerSets[i].name == name) return container.layerSets[i];
    }
    return null;
  }
  function requireDirectLayerSet(container, name) {
    var group = findDirectLayerSet(container, name);
    if (!group) throw new Error('Missing required group ' + name + ' in ' + container.name);
    return group;
  }
  function findDirectArtLayer(container, name) {
    for (var i = 0; i < container.artLayers.length; i++) {
      if (container.artLayers[i].name == name) return container.artLayers[i];
    }
    return null;
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
    if (findDirectArtLayer(group, name)) throw new Error('Text already exists: ' + name);
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
  function repurposeText(layer, name, contents, x, baselineY, size, color, bold) {
    layer.name = name;
    var item = layer.textItem;
    item.contents = contents;
    item.position = [UnitValue(x, 'px'), UnitValue(baselineY, 'px')];
    item.size = UnitValue(size, 'pt');
    item.font = 'MicrosoftYaHei';
    item.color = colorFromHex(color);
    item.justification = Justification.LEFT;
    try { item.fauxBold = !!bold; } catch (ignored) {}
    return layer;
  }
  function duplicateAndFit(source, targetGroup, name, targetW, targetH, absLeft, absTop) {
    var copy = source.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    var b = boundsOf(copy);
    var sw = b[2] - b[0];
    var sh = b[3] - b[1];
    if (sw <= 0 || sh <= 0) throw new Error('Zero-size duplicate source: ' + source.name);
    copy.resize((targetW / sw) * 100, (targetH / sh) * 100, AnchorPosition.TOPLEFT);
    var b2 = boundsOf(copy);
    copy.translate(UnitValue(absLeft - b2[0], 'px'), UnitValue(absTop - b2[1], 'px'));
    copy.move(targetGroup, ElementPlacement.PLACEATEND);
    copy.name = name;
    copy.visible = true;
    return copy;
  }
  function moveToLegacy(layer, legacyGroup) {
    layer.move(legacyGroup, ElementPlacement.PLACEATEND);
    movedToLegacy.push(layer.name);
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
  function exportPage(record) {
    var exportDocument = doc.duplicate('GameXXK_Page06TaskLog_' + record.name, true);
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
  function within(bounds, box, label) {
    if (bounds[0] < box[0] || bounds[2] > box[2] || bounds[1] < box[1] || bounds[3] > box[3]) {
      throw new Error(label + ' outside approved area: ' + bounds.join(',') + ' vs ' + box.join(','));
    }
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    for (var openIndex = 0; openIndex < app.documents.length; openIndex++) {
      var candidate = app.documents[openIndex];
      try {
        var candidatePath = String(candidate.fullName.fsName).split(String.fromCharCode(92)).join('/').toLowerCase();
        var targetCompare = String(targetFile.fsName).split(String.fromCharCode(92)).join('/').toLowerCase();
        if (candidatePath == targetCompare) { doc = candidate; break; }
      } catch (ignored) {}
    }
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before page-06 task log pass');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 12) throw new Error('Expected twelve top-level pages, found ' + doc.layerSets.length);

    var page06 = requireDirectLayerSet(doc, '06_任务日志');
    var runtimeText = requireDirectLayerSet(page06, '70_RuntimeText');
    var actions = requireDirectLayerSet(page06, '40_Actions');
    var questDetail = requireDirectLayerSet(page06, '31_QuestDetail');
    var questList = requireDirectLayerSet(page06, '30_QuestList');
    var compactCurrency = requireDirectLayerSet(page06, '01_CompactOutOfRunCurrency');

    if (findDirectLayerSet(page06, '20_FilterTabs')) throw new Error('Page-06 filter tabs already exist');
    if (findDirectLayerSet(page06, '02_WindowControls')) throw new Error('Page-06 window controls already exist');
    if (findDirectLayerSet(page06, '99_Legacy_06_OldContent')) throw new Error('Page-06 legacy group already exists');

    var legacy = page06.layerSets.add();
    legacy.name = '99_Legacy_06_OldContent';
    legacy.visible = false;

    var oldTextNames = ['text_001', 'text_002', 'text_003', 'text_005', 'text_006', 'text_007',
      'text_008', 'text_009', 'text_010', 'text_011', 'text_012', 'text_013'];
    for (var textIndex = 0; textIndex < oldTextNames.length; textIndex++) {
      var oldText = findDirectArtLayer(runtimeText, oldTextNames[textIndex]);
      if (oldText) moveToLegacy(oldText, legacy);
    }
    var oldButtonNames = ['016_button_primary', '017_button_normal', '018_button_normal', '019_button_normal'];
    for (var buttonIndex = 0; buttonIndex < oldButtonNames.length; buttonIndex++) {
      var oldButton = findDirectArtLayer(questList, oldButtonNames[buttonIndex]);
      if (oldButton) moveToLegacy(oldButton, legacy);
    }
    var titleLayer = findDirectArtLayer(runtimeText, 'text_004');
    if (!titleLayer) throw new Error('Missing page-06 title text layer');
    repurposeText(titleLayer, '01_Title', '任务日志', pageOrigin[0] + 383, pageOrigin[1] + 255, 42, '#2B2822', true);
    var trackLabelLayer = findDirectArtLayer(runtimeText, 'text_014');
    if (!trackLabelLayer) throw new Error('Missing page-06 track label text layer');
    moveToLegacy(trackLabelLayer, legacy);

    var sourcePages = {
      page03: requireDirectLayerSet(doc, '03_主角背包'),
      page07: requireDirectLayerSet(doc, '07_商店交易'),
      page13: requireDirectLayerSet(doc, '13_主角背包_物品选中'),
      page18: requireDirectLayerSet(doc, '18_主角背包_卡组页')
    };
    var tabSource = findDirectArtLayer(requireDirectLayerSet(sourcePages.page03, '20_Tabs'), '003_tab_1');
    if (!tabSource) throw new Error('Missing tab source in page 03');
    var inkSource = findDirectArtLayer(requireDirectLayerSet(sourcePages.page07, '40_ProductGrid'), '032_selected_selection_ink');
    if (!inkSource) throw new Error('Missing selection ink source in page 07');
    var tooltipSource = findDirectArtLayer(requireDirectLayerSet(sourcePages.page13, '44_SelectedItemTooltip'), '01_TooltipPaper_CurrentParchment');
    if (!tooltipSource) throw new Error('Missing tooltip paper source in page 13');
    var closeSource = findDirectArtLayer(requireDirectLayerSet(sourcePages.page18, '02_WindowControls'), '01_CloseButton');
    if (!closeSource) throw new Error('Missing close button source in page 18');
    var trackButtonSource = findDirectArtLayer(actions, '021_button_primary');
    if (!trackButtonSource) throw new Error('Missing track button source in page-06 actions');

    var windowControls = page06.layerSets.add();
    windowControls.name = '02_WindowControls';
    windowControls.visible = true;
    windowControls.move(compactCurrency, ElementPlacement.PLACEBEFORE);
    duplicateAndFit(closeSource, windowControls, '01_CloseButton', 74, 74, pageOrigin[0] + 1652, pageOrigin[1] + 201);

    var filterTabs = page06.layerSets.add();
    filterTabs.name = '20_FilterTabs';
    filterTabs.visible = true;
    filterTabs.move(runtimeText, ElementPlacement.PLACEBEFORE);
    var tabSpecs = [
      { name: '01_TabAllBg', x: 390, primary: true },
      { name: '02_TabMainBg', x: 473, primary: false },
      { name: '03_TabSideBg', x: 556, primary: false },
      { name: '04_TabDoneBg', x: 639, primary: false }
    ];
    for (var tabIndex = 0; tabIndex < tabSpecs.length; tabIndex++) {
      var spec = tabSpecs[tabIndex];
      if (spec.primary) {
        duplicateAndFit(trackButtonSource, filterTabs, spec.name, 82, 60, pageOrigin[0] + spec.x, pageOrigin[1] + 290);
      } else {
        duplicateAndFit(tabSource, filterTabs, spec.name, 82, 60, pageOrigin[0] + spec.x, pageOrigin[1] + 290);
      }
    }
    addText(filterTabs, '05_FilterAll', '全部', pageOrigin[0] + 409, pageOrigin[1] + 331, 22, '#2B2822', true);
    addText(filterTabs, '06_FilterMain', '主线', pageOrigin[0] + 492, pageOrigin[1] + 331, 22, '#5B5143', false);
    addText(filterTabs, '07_FilterSide', '支线', pageOrigin[0] + 575, pageOrigin[1] + 331, 22, '#5B5143', false);
    addText(filterTabs, '08_FilterDone', '已完成', pageOrigin[0] + 647, pageOrigin[1] + 331, 22, '#5B5143', false);

    duplicateAndFit(inkSource, questList, '01_SelectionInk', 330, 54, pageOrigin[0] + 390, pageOrigin[1] + 350);
    var rowSpecs = [
      { name: '02_Row01', text: '掌柜的请求', color: '#2B2822' },
      { name: '03_Row02', text: '山路异响', color: '#2B2822' },
      { name: '04_Row03', text: '青山镇来客', color: '#2B2822' },
      { name: '05_Row04', text: '采药人的委托', color: '#2B2822' },
      { name: '06_Row05', text: '猎户的报酬', color: '#2B2822' },
      { name: '07_Row06', text: '归途迷路客', color: '#2B2822' },
      { name: '08_Row07', text: '山匪夜袭', color: '#8A8072' }
    ];
    var rowTopY = 350;
    var rowPitch = 71.43;
    var rowTexts = [];
    for (var rowIndex = 0; rowIndex < rowSpecs.length; rowIndex++) {
      var rowSpec = rowSpecs[rowIndex];
      var rowY = rowTopY + rowPitch * rowIndex;
      var layer = addText(questList, rowSpec.name, rowSpec.text, pageOrigin[0] + 400, pageOrigin[1] + rowY + 36, 23, rowSpec.color, false);
      rowTexts.push(layer);
    }

    duplicateAndFit(tooltipSource, questDetail, '01_TooltipPaper', 520, 240, pageOrigin[0] + 850, pageOrigin[1] + 400);
    addText(questDetail, '02_DetailTitle', '掌柜的请求', pageOrigin[0] + 880, pageOrigin[1] + 438, 30, '#2B2822', true);
    addText(questDetail, '03_DetailObjective', '任务目标', pageOrigin[0] + 880, pageOrigin[1] + 480, 20, '#2B2822', true);
    addText(questDetail, '04_DetailDesc', '前往镇外山路，调查最近出现的怪物。', pageOrigin[0] + 880, pageOrigin[1] + 518, 20, '#5B5143', false);
    addText(questDetail, '05_DetailProgress', '当前进度：0 / 1', pageOrigin[0] + 880, pageOrigin[1] + 560, 20, '#5B5143', false);
    addText(questDetail, '06_DetailReward', '任务奖励：铜钱 200 · 青玉 10', pageOrigin[0] + 880, pageOrigin[1] + 600, 20, '#5B5143', false);
    var emptyHint = addText(questDetail, '99_EmptyHint', '暂无任务', pageOrigin[0] + 1070, pageOrigin[1] + 530, 20, '#5B5143', false);
    emptyHint.visible = false;

    duplicateAndFit(trackButtonSource, actions, '01_TrackButton', 220, 72, pageOrigin[0] + 1375, pageOrigin[1] + 850);
    moveToLegacy(trackButtonSource, legacy);

    var page12 = requireDirectLayerSet(doc, '12_系统菜单');
    var menuGroup = requireDirectLayerSet(page12, '30_Menu');
    var closeButtonSource = findDirectArtLayer(menuGroup, '006_button_normal');
    if (!closeButtonSource) throw new Error('Missing button_normal source in page 12 menu');
    duplicateAndFit(closeButtonSource, actions, '02_CloseButton', 220, 72, pageOrigin[0] + 1375, pageOrigin[1] + 930);

    addText(actions, '03_TrackLabel', '追踪任务', pageOrigin[0] + 1439, pageOrigin[1] + 898, 23, '#2B2822', false);
    addText(actions, '04_CloseLabel', '关闭', pageOrigin[0] + 1462, pageOrigin[1] + 978, 23, '#2B2822', false);

    var titleBounds = localBounds(titleLayer);
    var inkBounds = localBounds(findDirectArtLayer(questList, '01_SelectionInk'));
    var paperBounds = localBounds(findDirectArtLayer(questDetail, '01_TooltipPaper'));
    var trackBounds = localBounds(findDirectArtLayer(actions, '01_TrackButton'));
    var closeBounds = localBounds(findDirectArtLayer(actions, '02_CloseButton'));
    var closeInkBounds = localBounds(findDirectArtLayer(windowControls, '01_CloseButton'));
    if (movedToLegacy.length != 18) throw new Error('Expected 18 legacy moves, moved ' + movedToLegacy.length);
    within(titleBounds, [375, 210, 585, 265], 'Title');
    within(inkBounds, [388, 348, 722, 406], 'Selection ink');
    within(paperBounds, [848, 398, 1372, 642], 'Tooltip paper');
    within(trackBounds, [1370, 845, 1600, 927], 'Track button');
    within(closeBounds, [1370, 925, 1600, 1007], 'Close button');
    within(closeInkBounds, [1647, 196, 1731, 280], 'Close ink');
    for (var filterTextIndex = 0; filterTextIndex < filterTabs.artLayers.length; filterTextIndex++) {
      within(localBounds(filterTabs.artLayers[filterTextIndex]), [388, 288, 722, 352], 'Filter text ' + filterTabs.artLayers[filterTextIndex].name);
    }
    for (var rowCheckIndex = 0; rowCheckIndex < rowTexts.length; rowCheckIndex++) {
      within(localBounds(rowTexts[rowCheckIndex]), [388, 348, 722, 852], 'Row text ' + rowTexts[rowCheckIndex].name);
    }
    for (var detailCheckIndex = 0; detailCheckIndex < questDetail.artLayers.length; detailCheckIndex++) {
      if (questDetail.artLayers[detailCheckIndex].visible) {
        within(localBounds(questDetail.artLayers[detailCheckIndex]), [848, 398, 1372, 642], 'Detail text ' + questDetail.artLayers[detailCheckIndex].name);
      }
    }
    if (!legacy.visible) {
      for (var legacyCheckIndex = 0; legacyCheckIndex < legacy.artLayers.length; legacyCheckIndex++) {
        if (legacy.artLayers[legacyCheckIndex].visible) throw new Error('Legacy layer is still visible: ' + legacy.artLayers[legacyCheckIndex].name);
      }
    }

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();

    var report = {
      status: 'PASS',
      topLevelPageCount: doc.layerSets.length,
      targetPage: page06.name,
      legacyMoved: movedToLegacy,
      titleBounds: localBounds(titleLayer),
      inkBounds: inkBounds,
      paperBounds: paperBounds,
      trackButtonBounds: trackBounds,
      closeButtonBounds: closeBounds,
      closeInkBounds: closeInkBounds,
      rowCount: rowSpecs.length,
      textLayers: ['01_Title', '05_FilterAll', '06_FilterMain', '07_FilterSide', '08_FilterDone',
        '02_Row01', '03_Row02', '04_Row03', '05_Row04', '06_Row05', '07_Row06', '08_Row07',
        '02_DetailTitle', '03_DetailObjective', '04_DetailDesc', '05_DetailProgress', '06_DetailReward',
        '03_TrackLabel', '04_CloseLabel'],
      emptyHintHidden: !findDirectArtLayer(questDetail, '99_EmptyHint').visible,
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
