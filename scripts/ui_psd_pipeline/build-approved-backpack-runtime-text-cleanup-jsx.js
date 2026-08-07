const fs = require('fs');
const path = require('path');

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

const args = parseArguments(process.argv.slice(2));
if (!fs.existsSync(args.psd) || !fs.statSync(args.psd).isFile()) {
  throw new Error(`Missing master PSD: ${args.psd}`);
}
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
const reportTemplate = {
  status: 'PASS',
  topLevelPageCount: 18,
  page: '13_主角背包_物品选中',
  hiddenLegacyGroup: '99_Legacy_SelectedItemRuntimeText',
  hiddenPlaceholderTexts: ['小布袋', '普通布袋，能装下一些小物件。', '分解', '使用'],
  hiddenPlaceholderTextCount: 4,
  exports: exportRecords,
};

function jsxString(value) {
  return JSON.stringify(String(value).replace(/\\/g, '/'));
}

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exportRecords)};
  var reportJson = ${JSON.stringify(JSON.stringify(reportTemplate))};
  var expectedTexts = ${JSON.stringify(reportTemplate.hiddenPlaceholderTexts)};
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) { return String(value || '').replace(/\\\\/g, '/').toLowerCase(); }
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
  function countExpectedTextLayers(container) {
    var count = 0;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      if (layer.kind != LayerKind.TEXT) continue;
      var contents = String(layer.textItem.contents);
      for (var textIndex = 0; textIndex < expectedTexts.length; textIndex++) {
        if (contents == expectedTexts[textIndex]) { count++; break; }
      }
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      count += countExpectedTextLayers(container.layerSets[groupIndex]);
    }
    return count;
  }
  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
  }
  function exportPage(record) {
    var exportDocument = doc.duplicate('GameXXK_RuntimeTextCleanup_' + record.name, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(record.origin[0], 'px'), UnitValue(record.origin[1], 'px'),
      UnitValue(record.origin[0] + 1920, 'px'), UnitValue(record.origin[1] + 1080, 'px')
    ]);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
    var pngOptions = new PNGSaveOptions();
    pngOptions.interlaced = false;
    exportDocument.saveAs(new File(record.path), pngOptions, true, Extension.LOWERCASE);
    exportDocument.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before placeholder cleanup');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');

    var page = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    var runtimeText = requireDirectLayerSet(page, '46_SelectedItemRuntimeText');
    if (countExpectedTextLayers(runtimeText) != 4) throw new Error('Expected four selected-item placeholder text layers');
    runtimeText.name = '99_Legacy_SelectedItemRuntimeText';
    runtimeText.visible = false;
    if (runtimeText.visible) throw new Error('Selected-item placeholder text group remained visible');

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();
    writeUtf8(reportPath, reportJson);
    if (openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES);
  } catch (error) {
    try {
      if (doc && initialHistoryState) { app.activeDocument = doc; doc.activeHistoryState = initialHistoryState; }
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

