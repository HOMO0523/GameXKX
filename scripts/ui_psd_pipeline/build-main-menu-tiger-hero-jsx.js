const fs = require('fs');
const path = require('path');

function parseArguments(values) {
  const required = ['psd', 'image', 'brush-primary', 'brush-normal', 'output', 'report', 'export'];
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
requireFile(args.image, 'main-menu illustration');
requireFile(args['brush-primary'], 'primary brush component');
requireFile(args['brush-normal'], 'normal brush component');

for (const destination of [args.output, args.report, args.export]) {
  fs.mkdirSync(path.dirname(destination), { recursive: true });
}
if (fs.existsSync(args.report)) throw new Error(`Report already exists: ${args.report}`);
if (fs.existsSync(args.export)) throw new Error(`Export already exists: ${args.export}`);

const reportJson = JSON.stringify({
  status: 'PASS',
  page: '01_主菜单',
  pageCount: 18,
  canvas: [1920, 1080],
  title: '霞客行',
  buttonLabels: ['开始游戏', '加载存档', '设置游戏', '退出'],
  createdGroups: ['10_MenuIllustration', '20_HeroIdentityCorrection', '30_Title', '40_MenuButtons', '70_RuntimeText'],
  legacyGroups: [
    '99_Legacy_00_FamilyCorrection_PreTigerHero',
    '99_Legacy_30_Menu_PreTigerHero',
    '99_Legacy_70_RuntimeText_PreTigerHero',
  ],
  nonTargetSignatureMatch: true,
  illustration: args.image.replace(/\\/g, '/'),
  brushComponents: [
    args['brush-primary'].replace(/\\/g, '/'),
    args['brush-normal'].replace(/\\/g, '/'),
  ],
  export: args.export.replace(/\\/g, '/'),
});

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var illustrationPath = ${jsxString(args.image)};
  var brushPrimaryPath = ${jsxString(args['brush-primary'])};
  var brushNormalPath = ${jsxString(args['brush-normal'])};
  var reportPath = ${jsxString(args.report)};
  var exportPath = ${jsxString(args.export)};
  var pageName = '01_主菜单';
  var originX = 2040;
  var originY = 0;
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

  function addText(group, name, contents, x, baselineY, size, color, justification, bold) {
    var layer = group.artLayers.add();
    layer.name = name;
    layer.kind = LayerKind.TEXT;
    var item = layer.textItem;
    item.contents = contents;
    item.position = [UnitValue(x, 'px'), UnitValue(baselineY, 'px')];
    item.size = UnitValue(size, 'pt');
    item.font = 'MicrosoftYaHei';
    item.color = colorFromHex(color);
    item.justification = justification == 'center' ? Justification.CENTER : Justification.LEFT;
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

  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
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
    var newGroupNames = ['10_MenuIllustration', '20_HeroIdentityCorrection', '30_Title', '40_MenuButtons'];
    for (var preflightIndex = 0; preflightIndex < newGroupNames.length; preflightIndex++) {
      if (findDirectLayerSet(page, newGroupNames[preflightIndex])) {
        throw new Error('Target page already contains ' + newGroupNames[preflightIndex]);
      }
    }
    if (!findDirectLayerSet(page, '00_FamilyCorrection')) throw new Error('Missing 00_FamilyCorrection');
    if (!findDirectLayerSet(page, '30_Menu')) throw new Error('Missing 30_Menu');
    if (!findDirectLayerSet(page, '70_RuntimeText')) throw new Error('Missing 70_RuntimeText');

    var pageCountBefore = doc.layerSets.length;
    var otherPagesBefore = nonTargetSignature(doc, pageName);

    renameAndHide(page, '00_FamilyCorrection', '99_Legacy_00_FamilyCorrection_PreTigerHero');
    renameAndHide(page, '30_Menu', '99_Legacy_30_Menu_PreTigerHero');
    renameAndHide(page, '70_RuntimeText', '99_Legacy_70_RuntimeText_PreTigerHero');

    var illustrationGroup = page.layerSets.add();
    illustrationGroup.name = '10_MenuIllustration';
    importRaster(
      illustrationPath,
      illustrationGroup,
      '00_TigerHero_MainMenu_Illustration',
      originX,
      originY,
      pageWidth,
      pageHeight
    );

    var correctionGroup = page.layerSets.add();
    correctionGroup.name = '20_HeroIdentityCorrection';
    correctionGroup.visible = false;

    var titleGroup = page.layerSets.add();
    titleGroup.name = '30_Title';
    addText(titleGroup, '00_Title_霞客行', '霞客行', originX + 92, originY + 132, 74, '#213936', 'left', true);

    var buttonGroup = page.layerSets.add();
    buttonGroup.name = '40_MenuButtons';
    var labels = ['开始游戏', '加载存档', '设置游戏', '退出'];
    var buttonY = [318, 414, 510, 606];
    for (var buttonIndex = 0; buttonIndex < labels.length; buttonIndex++) {
      var button = importRaster(
        buttonIndex == 0 ? brushPrimaryPath : brushNormalPath,
        buttonGroup,
        (buttonIndex < 9 ? '0' : '') + String(buttonIndex + 1) + '_Brush_' + labels[buttonIndex],
        originX + 82,
        originY + buttonY[buttonIndex],
        350,
        78
      );
      button.opacity = buttonIndex == 0 ? 100 : 90;
    }

    var runtimeGroup = page.layerSets.add();
    runtimeGroup.name = '70_RuntimeText';
    for (var textIndex = 0; textIndex < labels.length; textIndex++) {
      addText(
        runtimeGroup,
        (textIndex < 9 ? '0' : '') + String(textIndex + 1) + '_MenuLabel_' + labels[textIndex],
        labels[textIndex],
        originX + 257,
        originY + buttonY[textIndex] + 53,
        29,
        '#F3E8D1',
        'center',
        textIndex == 0
      );
    }

    illustrationGroup.visible = true;
    titleGroup.visible = true;
    buttonGroup.visible = true;
    runtimeGroup.visible = true;
    page.visible = true;

    var pageCountAfter = doc.layerSets.length;
    var otherPagesAfter = nonTargetSignature(doc, pageName);
    if (pageCountBefore != pageCountAfter) throw new Error('Top-level page count changed');
    if (pageCountAfter != 18) throw new Error('Expected eighteen top-level pages');
    if (otherPagesBefore != otherPagesAfter) throw new Error('A non-target page changed');
    if (findDirectLayerSet(page, '00_FamilyCorrection')) throw new Error('Old family correction group is still active by name');
    if (!findDirectLayerSet(page, '10_MenuIllustration')) throw new Error('Missing illustration group after assembly');
    if (!findDirectLayerSet(page, '30_Title')) throw new Error('Missing title group after assembly');
    if (!findDirectLayerSet(page, '40_MenuButtons')) throw new Error('Missing button group after assembly');
    if (!findDirectLayerSet(page, '70_RuntimeText')) throw new Error('Missing runtime text group after assembly');
    if (runtimeGroup.artLayers.length != 4) throw new Error('Expected four editable menu labels');

    var exportDocument = doc.duplicate('GameXXK_MainMenu_TigerHero_Export', true);
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
})();
`;

fs.writeFileSync(args.output, jsx, 'utf8');
console.log(args.output);
