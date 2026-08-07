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
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
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

const report = {
  status: 'PASS',
  pageCount: 18,
  targetPages: targets.map((target) => target.name),
  hiddenLegacyCurrencyLayerCount: 24,
  hiddenLegacyShopPriceIconCount: 10,
  visibleCompactCurrencyGroupCount: 8,
  visibleShopIngotPriceCount: 8,
  hiddenShopStateIngotPriceCount: 2,
  exports: exportRecords,
};

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var reportPath = ${jsxString(args.report)};
  var targetPages = ${JSON.stringify(targets)};
  var exportRecords = ${JSON.stringify(exportRecords)};
  var reportJson = ${JSON.stringify(JSON.stringify(report))};
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) {
    return String(value || '').replace(/\\\\/g, '/').toLowerCase();
  }

  function beginsWith(value, prefix) {
    return String(value).substring(0, prefix.length) == prefix;
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

  function writeUtf8(filePath, contents) {
    var file = new File(filePath);
    file.encoding = 'UTF8';
    file.open('w');
    file.write(contents);
    file.close();
  }

  function hideLegacyCurrencyLayers(container) {
    var count = 0;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      if (
        beginsWith(layer.name, '99_Legacy_LongCurrencyPaper') ||
        beginsWith(layer.name, '99_Legacy_TopBalanceCopperIcon') ||
        beginsWith(layer.name, '99_Legacy_TopBalanceValue')
      ) {
        layer.visible = false;
        if (layer.visible) throw new Error('Legacy currency layer remained visible: ' + layer.name);
        count++;
      }
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      count += hideLegacyCurrencyLayers(container.layerSets[groupIndex]);
    }
    return count;
  }

  function repairShopPriceVisibility(shopPage) {
    var hiddenCopperCount = 0;
    var visibleIngotCount = 0;
    var hiddenIngotCount = 0;
    function visit(container) {
      for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
        var layer = container.artLayers[artIndex];
        if (beginsWith(layer.name, '99_Legacy_CopperPrice_')) {
          layer.visible = false;
          if (layer.visible) throw new Error('Legacy shop copper price remained visible: ' + layer.name);
          hiddenCopperCount++;
        } else if (beginsWith(layer.name, 'IngotPrice_')) {
          if (layer.visible) visibleIngotCount++; else hiddenIngotCount++;
        }
      }
      for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
        visit(container.layerSets[groupIndex]);
      }
    }
    visit(shopPage);
    if (hiddenCopperCount != 10) throw new Error('Expected ten hidden shop copper price layers, got ' + hiddenCopperCount);
    if (visibleIngotCount != 8 || hiddenIngotCount != 2) {
      throw new Error('Unexpected shop ingot visibility: visible=' + visibleIngotCount + ', hidden=' + hiddenIngotCount);
    }
    return [hiddenCopperCount, visibleIngotCount, hiddenIngotCount];
  }

  function exportPage(pageRecord, exportPath) {
    var exportDocument = doc.duplicate('GameXXK_VisibilityRepair_' + pageRecord.name, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(pageRecord.origin[0], 'px'),
      UnitValue(pageRecord.origin[1], 'px'),
      UnitValue(pageRecord.origin[0] + 1920, 'px'),
      UnitValue(pageRecord.origin[1] + 1080, 'px')
    ]);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
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
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before visibility repair');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');

    var hiddenCurrencyCount = 0;
    for (var targetIndex = 0; targetIndex < targetPages.length; targetIndex++) {
      var page = requireDirectLayerSet(doc, targetPages[targetIndex].name);
      var compact = requireDirectLayerSet(page, '01_CompactOutOfRunCurrency');
      compact.visible = true;
      for (var compactIndex = 0; compactIndex < compact.artLayers.length; compactIndex++) compact.artLayers[compactIndex].visible = true;
      hiddenCurrencyCount += hideLegacyCurrencyLayers(page);
    }
    if (hiddenCurrencyCount != 24) throw new Error('Expected twenty-four legacy currency layers, got ' + hiddenCurrencyCount);
    repairShopPriceVisibility(requireDirectLayerSet(doc, '07_商店交易'));

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
