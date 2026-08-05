const fs = require('fs');
const path = require('path');

function readArguments(argumentsList) {
  const required = ['psd', 'components', 'master-manifest', 'output', 'receipt'];
  const values = {};
  for (const name of required) {
    const flag = `--${name}`;
    const index = argumentsList.indexOf(flag);
    if (index === -1) {
      throw new Error(`Missing required argument ${flag}`);
    }
    const value = argumentsList[index + 1];
    if (!value || value.startsWith('--')) {
      throw new Error(`${flag} requires a value`);
    }
    values[name] = path.resolve(value);
  }
  return values;
}

function readJson(filePath, label) {
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    throw new Error(`Missing ${label}: ${filePath}`);
  }
  return JSON.parse(fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, ''));
}

function jsxString(value) {
  return JSON.stringify(String(value));
}

function requireBox(value, label) {
  if (!Array.isArray(value) || value.length !== 4 || value.some((entry) => !Number.isFinite(Number(entry)))) {
    throw new Error(`${label} must be a four-number box`);
  }
  return value.map(Number);
}

const args = readArguments(process.argv.slice(2));
if (!fs.existsSync(args.psd) || !fs.statSync(args.psd).isFile()) {
  throw new Error(`Missing target PSD: ${args.psd}`);
}

const componentsManifest = readJson(args.components, 'component manifest');
const masterManifest = readJson(args['master-manifest'], 'master manifest');
const page = (masterManifest.pages || []).find((entry) => entry.group === '07_商店交易');
if (!page) {
  throw new Error('Master manifest is missing page 07_商店交易');
}
if (!Array.isArray(page.origin) || page.origin.length !== 2 || page.origin[0] !== 4080 || page.origin[1] !== 1200) {
  throw new Error(`Unexpected 07_商店交易 origin: ${JSON.stringify(page.origin)}`);
}
if (!componentsManifest.background || !Array.isArray(componentsManifest.components)) {
  throw new Error('Component manifest must contain one background and a components array');
}
if (componentsManifest.components.length !== 8) {
  throw new Error(`Expected eight shell components, got ${componentsManifest.components.length}`);
}
if (
  !Array.isArray(componentsManifest.targetCanvas)
  || componentsManifest.targetCanvas[0] !== 1920
  || componentsManifest.targetCanvas[1] !== 1080
) {
  throw new Error(`Unexpected component target canvas: ${JSON.stringify(componentsManifest.targetCanvas)}`);
}

const componentsRoot = path.dirname(args.components);
function resolveAsset(record, label) {
  if (!record || typeof record.name !== 'string' || !record.name || typeof record.file !== 'string' || !record.file) {
    throw new Error(`${label} requires non-empty name and file fields`);
  }
  const box = requireBox(record.box, `${label}.box`);
  const assetPath = path.resolve(componentsRoot, record.file);
  if (!fs.existsSync(assetPath) || !fs.statSync(assetPath).isFile()) {
    throw new Error(`Missing ${label} asset: ${assetPath}`);
  }
  return {
    name: record.name,
    path: assetPath.replace(/\\/g, '/'),
    x: page.origin[0] + box[0],
    y: page.origin[1] + box[1],
  };
}

const backgroundItem = resolveAsset(componentsManifest.background, 'background');
const componentItems = componentsManifest.components.map((record, index) => (
  resolveAsset(record, `component ${index + 1}`)
));
const importItems = [backgroundItem, ...componentItems];
const componentLayerNames = componentItems.map((item) => item.name);

fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.receipt), { recursive: true });

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd.replace(/\\/g, '/'))};
  var receiptPath = ${jsxString(args.receipt.replace(/\\/g, '/'))};
  var importItems = ${JSON.stringify(importItems)};
  var componentLayerNames = ${JSON.stringify(componentLayerNames)};
  var targetFile = new File(targetPsdPath);
  var openedByScript = false;
  var doc = null;
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

  function collectArtLayersContaining(container, token, result) {
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      if (String(layer.name).indexOf(token) >= 0) result.push(layer);
    }
    for (var setIndex = 0; setIndex < container.layerSets.length; setIndex++) {
      collectArtLayersContaining(container.layerSets[setIndex], token, result);
    }
  }

  function importImage(targetDocument, targetGroup, item) {
    var sourceFile = new File(item.path);
    if (!sourceFile.exists) throw new Error('Missing image: ' + item.path);
    var sourceDocument = app.open(sourceFile);
    var sourceLayer = sourceDocument.activeLayer;
    var sourceBounds = sourceLayer.bounds;
    var sourceLeft = sourceBounds[0].as('px');
    var sourceTop = sourceBounds[1].as('px');
    var duplicated = sourceLayer.duplicate(targetDocument, ElementPlacement.PLACEATBEGINNING);
    sourceDocument.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = targetDocument;
    targetDocument.activeLayer = duplicated;
    duplicated.name = item.name;
    var duplicatedBounds = duplicated.bounds;
    var currentLeft = duplicatedBounds[0].as('px');
    var currentTop = duplicatedBounds[1].as('px');
    var desiredLeft = Number(item.x) + sourceLeft;
    var desiredTop = Number(item.y) + sourceTop;
    duplicated.translate(
      UnitValue(desiredLeft - currentLeft, 'px'),
      UnitValue(desiredTop - currentTop, 'px')
    );
    duplicated.move(targetGroup, ElementPlacement.INSIDE);
    duplicated.visible = true;
    return duplicated;
  }

  function jsonQuote(value) {
    var input = String(value);
    var slash = String.fromCharCode(92);
    var quoted = '"';
    for (var index = 0; index < input.length; index++) {
      var code = input.charCodeAt(index);
      var character = input.charAt(index);
      if (code == 34) quoted += slash + '"';
      else if (code == 92) quoted += slash + slash;
      else if (code == 8) quoted += slash + 'b';
      else if (code == 9) quoted += slash + 't';
      else if (code == 10) quoted += slash + 'n';
      else if (code == 12) quoted += slash + 'f';
      else if (code == 13) quoted += slash + 'r';
      else if (code < 32) quoted += slash + 'u' + ('000' + code.toString(16)).slice(-4);
      else quoted += character;
    }
    return quoted + '"';
  }

  function jsonStringArray(values) {
    var parts = [];
    for (var index = 0; index < values.length; index++) parts.push(jsonQuote(values[index]));
    return '[' + parts.join(',') + ']';
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) {
      doc = app.open(targetFile);
      openedByScript = true;
    }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes; save it before shell injection.');
    initialHistoryState = doc.activeHistoryState;

    var pageGroup = findDirectLayerSet(doc, '07_商店交易');
    if (!pageGroup) throw new Error('Missing top-level group 07_商店交易');
    var backgroundGroup = findDirectLayerSet(pageGroup, '10_Background');
    if (!backgroundGroup) throw new Error('Missing group 07_商店交易/10_Background');
    if (findDirectLayerSet(backgroundGroup, '00_ShellComponents')) {
      throw new Error('00_ShellComponents already exists; refusing a duplicate injection.');
    }

    var originalMatches = [];
    collectArtLayersContaining(pageGroup, 'approved_v2_shop_shell', originalMatches);
    if (originalMatches.length != 1) {
      throw new Error('Expected one approved_v2_shop_shell layer, got ' + originalMatches.length);
    }
    var originalLayer = originalMatches[0];
    originalLayer.name = '99_原始合成壳体_备份';
    originalLayer.visible = false;

    var shellGroup = backgroundGroup.layerSets.add();
    shellGroup.name = '00_ShellComponents';
    var importedNames = [];
    for (var itemIndex = 0; itemIndex < importItems.length; itemIndex++) {
      var imported = importImage(doc, shellGroup, importItems[itemIndex]);
      importedNames.push(imported.name);
    }
    if (importedNames.length != 9) throw new Error('Expected nine imported layers.');

    doc.save();

    var receiptText = '{' +
      '"page":' + jsonQuote(pageGroup.name) + ',' +
      '"group":' + jsonQuote(shellGroup.name) + ',' +
      '"importedLayerCount":' + importedNames.length + ',' +
      '"originalLayerHidden":' + (!originalLayer.visible ? 'true' : 'false') + ',' +
      '"originalLayer":' + jsonQuote(originalLayer.name) + ',' +
      '"backgroundLayer":' + jsonQuote(importedNames[0]) + ',' +
      '"componentLayers":' + jsonStringArray(componentLayerNames) + ',' +
      '"psd":' + jsonQuote(targetFile.fsName) +
      '}';
    var receiptFile = new File(receiptPath);
    if (!receiptFile.parent.exists && !receiptFile.parent.create()) {
      throw new Error('Could not create receipt directory: ' + receiptFile.parent.fsName);
    }
    receiptFile.encoding = 'UTF8';
    if (!receiptFile.open('w')) throw new Error('Could not open receipt: ' + receiptPath);
    receiptFile.write(receiptText);
    receiptFile.close();
  } catch (operationError) {
    if (doc && initialHistoryState && !doc.saved) {
      try { doc.activeHistoryState = initialHistoryState; } catch (rollbackError) {}
    }
    if (doc && openedByScript && !doc.saved) {
      try { doc.close(SaveOptions.DONOTSAVECHANGES); } catch (closeError) {}
    }
    throw operationError;
  } finally {
    app.preferences.rulerUnits = oldUnits;
    app.displayDialogs = oldDialogs;
  }
})();
`;

fs.writeFileSync(args.output, '\uFEFF' + jsx, 'utf8');
console.log(JSON.stringify({
  ok: true,
  page: page.group,
  origin: page.origin,
  importedLayerCount: importItems.length,
  output: args.output,
  receipt: args.receipt,
}));
