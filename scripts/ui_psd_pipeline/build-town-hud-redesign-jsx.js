const fs = require('fs');
const path = require('path');

function parseArguments(values) {
  const required = [
    'psd',
    'background',
    'shell-components',
    'currency-strip',
    'ingot',
    'output',
    'report',
    'before-export',
    'after-export',
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

function requireDirectory(directoryPath, label) {
  if (!fs.existsSync(directoryPath) || !fs.statSync(directoryPath).isDirectory()) {
    throw new Error(`Missing ${label}: ${directoryPath}`);
  }
}

function jsxString(value) {
  return JSON.stringify(String(value).replace(/\\/g, '/'));
}

const args = parseArguments(process.argv.slice(2));
requireFile(args.psd, 'master PSD');
requireFile(args.background, 'clean town background');
requireDirectory(args['shell-components'], 'shell-component directory');
requireFile(args['currency-strip'], 'compact currency strip');
requireFile(args.ingot, 'ingot icon');

const shellFiles = {
  identity: path.join(args['shell-components'], 'identity_panel.png'),
  backpack: path.join(args['shell-components'], 'nav_disc_backpack.png'),
  companion: path.join(args['shell-components'], 'nav_disc_companion.png'),
  codex: path.join(args['shell-components'], 'nav_disc_codex.png'),
  task: path.join(args['shell-components'], 'nav_disc_task.png'),
  route: path.join(args['shell-components'], 'nav_disc_route.png'),
};
for (const [name, filePath] of Object.entries(shellFiles)) {
  requireFile(filePath, `${name} shell component`);
}

for (const destination of [args.output, args.report, args['before-export'], args['after-export']]) {
  fs.mkdirSync(path.dirname(destination), { recursive: true });
}
for (const destination of [args.report, args['before-export'], args['after-export']]) {
  if (fs.existsSync(destination)) throw new Error(`Destination already exists: ${destination}`);
}

const reportJson = JSON.stringify({
  status: 'PASS',
  page: '02_城镇HUD',
  pageCount: 18,
  canvas: [1920, 1080],
  createdGroups: ['10_TownScene', '20_ShellPaper', '21_HeroAndNavigation', '30_OutOfRunCurrency', '70_RuntimeText'],
  legacyGroups: [
    '99_Legacy_00_FamilyCorrection_PreTownHUDV2',
    '99_Legacy_30_Context_PreTownHUDV2',
    '99_Legacy_70_RuntimeText_PreTownHUDV2',
  ],
  navigationLayers: ['nav_backpack', 'nav_companion', 'nav_codex', 'nav_task', 'nav_route'],
  currencyStripBox: [1570, 28, 320, 86],
  persistentPromptVisible: false,
  nonTargetSignatureMatch: true,
  shopPeerSignatureMatch: true,
  background: args.background.replace(/\\/g, '/'),
  currencyStrip: args['currency-strip'].replace(/\\/g, '/'),
  ingotIcon: args.ingot.replace(/\\/g, '/'),
  beforeExport: args['before-export'].replace(/\\/g, '/'),
  afterExport: args['after-export'].replace(/\\/g, '/'),
});

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var backgroundPath = ${jsxString(args.background)};
  var identityPanelPath = ${jsxString(shellFiles.identity)};
  var navBackpackDiscPath = ${jsxString(shellFiles.backpack)};
  var navCompanionDiscPath = ${jsxString(shellFiles.companion)};
  var navCodexDiscPath = ${jsxString(shellFiles.codex)};
  var navTaskDiscPath = ${jsxString(shellFiles.task)};
  var navRouteDiscPath = ${jsxString(shellFiles.route)};
  var currencyStripPath = ${jsxString(args['currency-strip'])};
  var ingotPath = ${jsxString(args.ingot)};
  var reportPath = ${jsxString(args.report)};
  var beforeExportPath = ${jsxString(args['before-export'])};
  var afterExportPath = ${jsxString(args['after-export'])};
  var pageName = '02_城镇HUD';
  var shopPageName = '07_商店交易';
  var originX = 4080;
  var originY = 0;
  var shopOriginX = 4080;
  var shopOriginY = 1200;
  var pageWidth = 1920;
  var pageHeight = 1080;
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) {
    return String(value || '').replace(/\\\\/g, '/').toLowerCase();
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

  function importRaster(filePath, targetGroup, name, left, top, width, height) {
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
      duplicated.visible = true;
      return duplicated;
    } finally {
      if (sourceDocument) {
        try { sourceDocument.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
      }
    }
  }

  function renameAndHide(page, originalName, legacyName) {
    if (findDirectLayerSet(page, legacyName)) {
      throw new Error('Legacy destination already exists: ' + legacyName);
    }
    var group = findDirectLayerSet(page, originalName);
    if (!group) throw new Error('Missing required page group: ' + originalName);
    group.name = legacyName;
    group.visible = false;
    return group;
  }

  function layerTreeSignature(container) {
    var records = [];
    for (var index = 0; index < container.layers.length; index++) {
      var layer = container.layers[index];
      var record = layer.typename + ':' + layer.name + ':' + String(layer.visible);
      if (layer.typename == 'LayerSet') record += '{' + layerTreeSignature(layer) + '}';
      records.push(record);
    }
    return records.join('|');
  }

  function nonTargetSignature(document, excludedPageName) {
    var records = [];
    for (var index = 0; index < document.layerSets.length; index++) {
      var group = document.layerSets[index];
      if (group.name != excludedPageName) records.push(group.name + '=' + layerTreeSignature(group));
    }
    return records.join('||');
  }

  function directGroupSignature(group) {
    return group.name + '=' + layerTreeSignature(group);
  }

  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
  }

  function exportPage(exportPath, exportName) {
    var exportDocument = doc.duplicate(exportName, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(originX, 'px'),
      UnitValue(originY, 'px'),
      UnitValue(originX + pageWidth, 'px'),
      UnitValue(originY + pageHeight, 'px')
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

  function shouldCopyShopLayer(layer) {
    var imageNames = [
      'hero_portrait',
      'nav_backpack',
      'nav_companion',
      'nav_codex',
      'nav_task',
      'nav_route'
    ];
    var actualName = String(layer.name);
    for (var nameIndex = 0; nameIndex < imageNames.length; nameIndex++) {
      var semanticName = imageNames[nameIndex];
      if (actualName == semanticName) return true;
      var suffix = '_' + semanticName;
      if (actualName.length > suffix.length && actualName.substring(actualName.length - suffix.length) == suffix) {
        return true;
      }
    }
    if (layer.kind != LayerKind.TEXT) return false;
    var text = String(layer.textItem.contents);
    return text == '主角  Lv. 1' || text == '行旅者' || text == '战力 33';
  }

  function copyShopIdentityAndNavigation(shopGlobal, targetGroup) {
    var copiedNames = [];
    var deltaX = originX - shopOriginX;
    var deltaY = originY - shopOriginY;
    for (var index = shopGlobal.artLayers.length - 1; index >= 0; index--) {
      var source = shopGlobal.artLayers[index];
      if (!shouldCopyShopLayer(source)) continue;
      var copied = source.duplicate();
      copied.move(targetGroup, ElementPlacement.INSIDE);
      copied.translate(UnitValue(deltaX, 'px'), UnitValue(deltaY, 'px'));
      copied.visible = true;
      copiedNames.push(copied.name);
    }
    if (targetGroup.artLayers.length != 9) {
      throw new Error('Expected nine approved shop identity/navigation layers, got ' + targetGroup.artLayers.length);
    }
    return copiedNames;
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

    var page = findDirectLayerSet(doc, pageName);
    if (!page) throw new Error('Missing target page: ' + pageName);
    if (!page.visible) throw new Error('Target page must be visible for deterministic before export');
    var shopPage = findDirectLayerSet(doc, shopPageName);
    if (!shopPage) throw new Error('Missing shop reference page: ' + shopPageName);
    var shopGlobal = findDirectLayerSet(shopPage, '20_GlobalShell');
    if (!shopGlobal) throw new Error('Missing shop reference group: 20_GlobalShell');

    var newGroupNames = ['10_TownScene', '20_ShellPaper', '21_HeroAndNavigation', '30_OutOfRunCurrency'];
    for (var preflightIndex = 0; preflightIndex < newGroupNames.length; preflightIndex++) {
      if (findDirectLayerSet(page, newGroupNames[preflightIndex])) {
        throw new Error('Target page already contains ' + newGroupNames[preflightIndex]);
      }
    }
    if (!findDirectLayerSet(page, '00_FamilyCorrection')) throw new Error('Missing 00_FamilyCorrection');
    if (!findDirectLayerSet(page, '30_Context')) throw new Error('Missing 30_Context');
    if (!findDirectLayerSet(page, '70_RuntimeText')) throw new Error('Missing 70_RuntimeText');

    var pageCountBefore = doc.layerSets.length;
    if (pageCountBefore != 18) throw new Error('Expected eighteen top-level pages before mutation');
    var otherPagesBefore = nonTargetSignature(doc, pageName);
    var shopBefore = directGroupSignature(shopPage);
    exportPage(beforeExportPath, 'GameXXK_TownHUD_Before_Export');

    renameAndHide(page, '00_FamilyCorrection', '99_Legacy_00_FamilyCorrection_PreTownHUDV2');
    renameAndHide(page, '30_Context', '99_Legacy_30_Context_PreTownHUDV2');
    renameAndHide(page, '70_RuntimeText', '99_Legacy_70_RuntimeText_PreTownHUDV2');

    var sceneGroup = page.layerSets.add();
    sceneGroup.name = '10_TownScene';
    importRaster(backgroundPath, sceneGroup, '00_CleanTownGameplayView', originX, originY, pageWidth, pageHeight);

    var shellGroup = page.layerSets.add();
    shellGroup.name = '20_ShellPaper';
    importRaster(identityPanelPath, shellGroup, '01_HeroIdentityPaper', originX + 24, originY + 14, 541, 185);
    importRaster(navBackpackDiscPath, shellGroup, '02_NavDisc_Backpack', originX + 27, originY + 210, 153, 154);
    importRaster(navCompanionDiscPath, shellGroup, '03_NavDisc_Companion', originX + 29, originY + 359, 149, 152);
    importRaster(navCodexDiscPath, shellGroup, '04_NavDisc_Codex', originX + 30, originY + 504, 149, 162);
    importRaster(navTaskDiscPath, shellGroup, '05_NavDisc_Quest', originX + 23, originY + 651, 164, 165);
    importRaster(navRouteDiscPath, shellGroup, '06_NavDisc_Route', originX + 28, originY + 800, 155, 157);
    importRaster(currencyStripPath, shellGroup, '07_CompactCurrencyPaper_320', originX + 1570, originY + 28, 320, 86);

    var heroNavGroup = page.layerSets.add();
    heroNavGroup.name = '21_HeroAndNavigation';
    copyShopIdentityAndNavigation(shopGlobal, heroNavGroup);

    var currencyGroup = page.layerSets.add();
    currencyGroup.name = '30_OutOfRunCurrency';
    importRaster(ingotPath, currencyGroup, '01_IngotIcon', originX + 1672, originY + 50, 42, 42);

    var runtimeGroup = page.layerSets.add();
    runtimeGroup.name = '70_RuntimeText';
    addText(runtimeGroup, '02_IngotValue', '500', originX + 1728, originY + 82, 27, '#2B2822', true);

    sceneGroup.visible = true;
    shellGroup.visible = true;
    heroNavGroup.visible = true;
    currencyGroup.visible = true;
    runtimeGroup.visible = true;
    page.visible = true;

    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');
    if (otherPagesBefore != nonTargetSignature(doc, pageName)) throw new Error('A non-target page changed');
    if (shopBefore != directGroupSignature(findDirectLayerSet(doc, shopPageName))) throw new Error('Shop reference changed');
    if (findDirectLayerSet(page, '00_FamilyCorrection')) throw new Error('Old family correction group is still active by name');
    if (findDirectLayerSet(page, '30_Context')) throw new Error('Persistent context group is still active');
    if (heroNavGroup.artLayers.length != 9) throw new Error('Hero/navigation layer count changed after copy');
    if (shellGroup.artLayers.length != 7) throw new Error('Expected seven shell-paper layers');
    if (currencyGroup.artLayers.length != 1) throw new Error('Expected one ingot icon layer');
    if (runtimeGroup.artLayers.length != 1) throw new Error('Expected one editable currency value');

    exportPage(afterExportPath, 'GameXXK_TownHUD_After_Export');
    doc.save();
    writeUtf8(reportPath, ${JSON.stringify(reportJson)});

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
