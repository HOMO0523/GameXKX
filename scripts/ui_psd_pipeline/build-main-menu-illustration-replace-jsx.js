const fs = require('fs');
const path = require('path');

function parseArguments(values) {
  const required = ['psd', 'image', 'output', 'report', 'export'];
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
requireFile(args.image, 'replacement illustration');
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
  illustrationGroup: '10_MenuIllustration',
  illustrationLayer: '00_TigerHero_MainMenu_Illustration',
  replacement: args.image.replace(/\\/g, '/'),
  nonTargetSignatureMatch: true,
  pagePeerSignatureMatch: true,
  export: args.export.replace(/\\/g, '/'),
});

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var replacementPath = ${jsxString(args.image)};
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

  function findDirectArtLayer(container, name) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (container.artLayers[index].name == name) return container.artLayers[index];
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

  function importRaster(filePath, targetGroup, name) {
    var sourceFile = new File(filePath);
    if (!sourceFile.exists) throw new Error('Missing raster source: ' + filePath);
    var sourceDocument = null;
    try {
      sourceDocument = app.open(sourceFile);
      sourceDocument.resizeImage(
        UnitValue(pageWidth, 'px'),
        UnitValue(pageHeight, 'px'),
        null,
        ResampleMethod.BICUBICSHARPER
      );
      var duplicated = sourceDocument.activeLayer.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
      sourceDocument.close(SaveOptions.DONOTSAVECHANGES);
      sourceDocument = null;
      app.activeDocument = doc;
      duplicated.name = name;
      var bounds = layerBounds(duplicated);
      duplicated.translate(UnitValue(originX - bounds[0], 'px'), UnitValue(originY - bounds[1], 'px'));
      duplicated.move(targetGroup, ElementPlacement.INSIDE);
      duplicated.visible = true;
      return duplicated;
    } finally {
      if (sourceDocument) {
        try { sourceDocument.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
      }
    }
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

  function pagePeerSignature(page, excludedGroupName) {
    var records = [];
    for (var index = 0; index < page.layers.length; index++) {
      var layer = page.layers[index];
      if (layer.name == excludedGroupName) continue;
      var record = layer.typename + ':' + layer.name + ':' + String(layer.visible);
      if (layer.typename == 'LayerSet') record += '{' + layerTreeSignature(layer) + '}';
      records.push(record);
    }
    return records.join('|');
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
    if (!doc.saved) throw new Error('Target PSD has unsaved changes.');
    initialHistoryState = doc.activeHistoryState;

    var page = findDirectLayerSet(doc, pageName);
    if (!page) throw new Error('Missing target page: ' + pageName);
    var illustrationGroup = findDirectLayerSet(page, '10_MenuIllustration');
    if (!illustrationGroup) throw new Error('Missing 10_MenuIllustration');
    if (illustrationGroup.layerSets.length != 0) throw new Error('Illustration group contains unexpected child groups');
    if (illustrationGroup.artLayers.length != 1) throw new Error('Expected one current illustration layer');
    var oldIllustration = findDirectArtLayer(illustrationGroup, '00_TigerHero_MainMenu_Illustration');
    if (!oldIllustration) throw new Error('Missing current illustration layer');

    var pageCountBefore = doc.layerSets.length;
    var otherPagesBefore = nonTargetSignature(doc, pageName);
    var pagePeersBefore = pagePeerSignature(page, '10_MenuIllustration');

    var replacement = importRaster(replacementPath, illustrationGroup, '01_TigerHero_MainMenu_Illustration_v9');
    oldIllustration.remove();
    replacement.name = '00_TigerHero_MainMenu_Illustration';
    if (illustrationGroup.artLayers.length != 1) throw new Error('Illustration replacement did not leave exactly one layer');

    var pageCountAfter = doc.layerSets.length;
    if (pageCountBefore != pageCountAfter || pageCountAfter != 18) throw new Error('Top-level page count changed');
    if (otherPagesBefore != nonTargetSignature(doc, pageName)) throw new Error('A non-target page changed');
    if (pagePeersBefore != pagePeerSignature(page, '10_MenuIllustration')) throw new Error('A peer group in page 01 changed');

    var exportDocument = doc.duplicate('GameXXK_MainMenu_v9_Export', true);
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
