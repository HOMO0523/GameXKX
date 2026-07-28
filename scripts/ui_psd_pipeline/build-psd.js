const fs = require('fs');
const path = require('path');

function readPackageRoot(argumentsList) {
  const rootIndex = argumentsList.indexOf('--root');
  if (rootIndex === -1) {
    return path.resolve(__dirname, '..', '..', 'SourceArt', 'UI', 'PSD', 'town-v2');
  }
  const rootValue = argumentsList[rootIndex + 1];
  if (!rootValue || rootValue.startsWith('--')) {
    throw new Error('--root requires a package directory');
  }
  return path.resolve(rootValue);
}

const root = readPackageRoot(process.argv.slice(2));
const manifestPath = path.join(root, 'manifest.json');
const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
const svgDir = path.join(root, 'svg_text');
fs.mkdirSync(svgDir, { recursive: true });

function xmlEscape(value) {
  return value
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&apos;');
}

function safeFilename(value, index) {
  const cleaned = value.replace(/[\\/:*?"<>|]/g, '_').slice(0, 72);
  return `${String(index + 1).padStart(3, '0')}_${cleaned}.svg`;
}

for (const [index, layer] of manifest.textLayers.entries()) {
  for (const key of ['name', 'text', 'x', 'y', 'fontSize', 'font', 'color']) {
    if (layer[key] === undefined || layer[key] === null) {
      throw new Error(`Missing text layer field ${key} at index ${index}`);
    }
  }
  const textAnchor = layer.justify === 'center' ? 'middle' : (layer.justify === 'right' ? 'end' : 'start');
  const svg = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    `<svg xmlns="http://www.w3.org/2000/svg" width="${manifest.document.width}" height="${manifest.document.height}" viewBox="0 0 ${manifest.document.width} ${manifest.document.height}">`,
    `  <text id="${xmlEscape(layer.name)}" x="${layer.x * (manifest.document.scale || 1)}" y="${(layer.y + layer.fontSize) * (manifest.document.scale || 1)}" text-anchor="${textAnchor}" font-family="${xmlEscape(layer.font)}" font-size="${layer.fontSize * (manifest.document.scale || 1)}" fill="${layer.color}">${xmlEscape(layer.text)}</text>`,
    '</svg>',
    ''
  ].join('\n');
  fs.writeFileSync(path.join(svgDir, safeFilename(layer.name, index)), svg, 'utf8');
}

function jsxString(value) {
  return JSON.stringify(String(value));
}

const imageLayers = JSON.stringify(manifest.imageLayers);
const textLayers = JSON.stringify(manifest.textLayers);
const document = JSON.stringify(manifest.document);
const previewPath = path.join(root, 'final_preview.png').replace(/\\/g, '/');

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  app.preferences.rulerUnits = Units.PIXELS;
  app.displayDialogs = DialogModes.NO;
  var imageLayers = ${imageLayers};
  var textLayers = ${textLayers};
  var spec = ${document};

  function colorFromHex(hex) {
    var c = new SolidColor();
    c.rgb.red = parseInt(hex.substr(1, 2), 16);
    c.rgb.green = parseInt(hex.substr(3, 2), 16);
    c.rgb.blue = parseInt(hex.substr(5, 2), 16);
    return c;
  }

  function setFont(textItem, preferred) {
    var candidates = [preferred, 'STKaiti', 'KaiTi', 'MicrosoftYaHei', 'SimSun', 'ArialMT'];
    for (var i = 0; i < candidates.length; i++) {
      try {
        textItem.font = candidates[i];
        return candidates[i];
      } catch (e) {}
    }
    return '';
  }

  function importImage(doc, item) {
    var sourceFile = new File(item.path);
    if (!sourceFile.exists) throw new Error('Missing image: ' + item.path);
    var source = app.open(sourceFile);
    var sourceLayer = source.activeLayer;
    var duplicated = sourceLayer.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    source.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
    duplicated.name = item.name;
    var bounds = duplicated.bounds;
    var currentLeft = bounds[0].as('px');
    var currentTop = bounds[1].as('px');
    duplicated.translate(UnitValue(item.x - currentLeft, 'px'), UnitValue(item.y - currentTop, 'px'));
    return duplicated;
  }

  for (var cleanupIndex = app.documents.length - 1; cleanupIndex >= 0; cleanupIndex--) {
    var cleanupDoc = app.documents[cleanupIndex];
    if (cleanupDoc.name == spec.name || cleanupDoc.name == 'background.png') {
      cleanupDoc.close(SaveOptions.DONOTSAVECHANGES);
    }
  }

  var doc = app.documents.add(
    spec.width,
    spec.height,
    spec.resolution,
    spec.name,
    NewDocumentMode.RGB,
    DocumentFill.TRANSPARENT,
    1,
    BitsPerChannelType.EIGHT
  );

  for (var i = 0; i < imageLayers.length; i++) {
    importImage(doc, imageLayers[i]);
  }

  var createdText = [];
  var documentScale = spec.scale || 1;
  for (var j = 0; j < textLayers.length; j++) {
    var item = textLayers[j];
    var layer = doc.artLayers.add();
    layer.kind = LayerKind.TEXT;
    layer.name = item.name;
    var textItem = layer.textItem;
    textItem.kind = TextType.POINTTEXT;
    textItem.contents = item.text;
    if (item.justify == 'center') textItem.justification = Justification.CENTER;
    else if (item.justify == 'right') textItem.justification = Justification.RIGHT;
    else textItem.justification = Justification.LEFT;
    textItem.position = [UnitValue(item.x * documentScale, 'px'), UnitValue((item.y + item.fontSize) * documentScale, 'px')];
    textItem.size = UnitValue(item.fontSize * documentScale, 'pt');
    textItem.color = colorFromHex(item.color);
    textItem.antiAliasMethod = AntiAlias.SHARP;
    setFont(textItem, item.font);
    try { textItem.fauxBold = !!item.bold; } catch (e) {}
    try { textItem.tracking = item.tracking || 0; } catch (e) {}
    createdText.push(textItem.contents);
  }

  var outputFile = new File(spec.outputPsd);
  if (!outputFile.parent.exists) outputFile.parent.create();
  var psdOptions = new PhotoshopSaveOptions();
  psdOptions.layers = true;
  psdOptions.alphaChannels = true;
  psdOptions.annotations = false;
  psdOptions.embedColorProfile = true;
  psdOptions.spotColors = true;
  doc.saveAs(outputFile, psdOptions, true, Extension.LOWERCASE);

  var previewDoc = doc.duplicate('preview_temp', true);
  previewDoc.flatten();
  var pngOptions = new PNGSaveOptions();
  pngOptions.interlaced = false;
  previewDoc.saveAs(new File(${jsxString(previewPath)}), pngOptions, true, Extension.LOWERCASE);
  previewDoc.close(SaveOptions.DONOTSAVECHANGES);

  doc.close(SaveOptions.DONOTSAVECHANGES);
  var reopened = app.open(outputFile);
  var textCount = 0;
  var actualTexts = [];
  for (var k = 0; k < reopened.artLayers.length; k++) {
    var art = reopened.artLayers[k];
    if (art.kind == LayerKind.TEXT) {
      textCount++;
      actualTexts.push(art.textItem.contents);
    }
  }
  var validation = {
    width: reopened.width.as('px'),
    height: reopened.height.as('px'),
    artLayerCount: reopened.artLayers.length,
    expectedImageLayers: imageLayers.length,
    expectedTextLayers: textLayers.length,
    actualTextLayers: textCount,
    textRoundTripMatch: actualTexts.length == createdText.length,
    outputPsd: outputFile.fsName
  };
  var expectedSorted = createdText.slice(0).sort();
  var actualSorted = actualTexts.slice(0).sort();
  var roundTripMatch = actualSorted.length == expectedSorted.length;
  if (roundTripMatch) {
    for (var matchIndex = 0; matchIndex < actualSorted.length; matchIndex++) {
      if (actualSorted[matchIndex] != expectedSorted[matchIndex]) {
        roundTripMatch = false;
        break;
      }
    }
  }
  validation.textRoundTripMatch = roundTripMatch;
  var q = String.fromCharCode(34);
  var nl = String.fromCharCode(10);
  var validationJson = '{' + nl +
    '  ' + q + 'width' + q + ': ' + validation.width + ',' + nl +
    '  ' + q + 'height' + q + ': ' + validation.height + ',' + nl +
    '  ' + q + 'artLayerCount' + q + ': ' + validation.artLayerCount + ',' + nl +
    '  ' + q + 'expectedImageLayers' + q + ': ' + validation.expectedImageLayers + ',' + nl +
    '  ' + q + 'expectedTextLayers' + q + ': ' + validation.expectedTextLayers + ',' + nl +
    '  ' + q + 'actualTextLayers' + q + ': ' + validation.actualTextLayers + ',' + nl +
    '  ' + q + 'textRoundTripMatch' + q + ': ' + (validation.textRoundTripMatch ? 'true' : 'false') + ',' + nl +
    '  ' + q + 'outputPsd' + q + ': ' + q + spec.outputPsd + q + nl +
    '}';
  var validationPath = String(spec.outputPsd).replace(/\\.psd$/i, '.validation.json');
  var validationFile = new File(validationPath);
  if (!validationFile.parent.exists) validationFile.parent.create();
  validationFile.encoding = 'UTF8';
  validationFile.open('w');
  validationFile.write(validationJson);
  validationFile.close();
  reopened.close(SaveOptions.DONOTSAVECHANGES);
  app.preferences.rulerUnits = oldUnits;
  app.displayDialogs = oldDialogs;
})();
`;

fs.writeFileSync(path.join(root, 'compose.jsx'), '\ufeff' + jsx, 'utf8');
console.log(JSON.stringify({
  imageLayers: manifest.imageLayers.length,
  textLayers: manifest.textLayers.length,
  svgFiles: manifest.textLayers.length,
  jsx: path.join(root, 'compose.jsx'),
  outputPsd: manifest.document.outputPsd
}));
