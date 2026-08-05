const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

function parseArguments(values) {
  const required = ['psd', 'manifest', 'master-manifest', 'source-receipt', 'output', 'receipt'];
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

function readJson(filePath, label) {
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    throw new Error(`Missing ${label}: ${filePath}`);
  }
  return JSON.parse(fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, ''));
}

function hashFile(filePath) {
  const digest = crypto.createHash('sha256');
  digest.update(fs.readFileSync(filePath));
  return digest.digest('hex');
}

function jsxString(value) {
  return JSON.stringify(String(value));
}

const args = parseArguments(process.argv.slice(2));
const manifest = readJson(args.manifest, 'family correction manifest');
const masterManifest = readJson(args['master-manifest'], 'master manifest');
const sourceReceipt = readJson(args['source-receipt'], 'source receipt');

if (!fs.existsSync(args.psd) || !fs.statSync(args.psd).isFile()) {
  throw new Error(`Missing target PSD: ${args.psd}`);
}
if (manifest.version !== 1 || !Array.isArray(manifest.pages) || manifest.pages.length !== 16) {
  throw new Error('Family correction manifest must contain sixteen version-1 page records');
}
if (!manifest.shopReference || manifest.shopReference.name !== '07_商店交易') {
  throw new Error('Family correction manifest is missing the shop reference');
}
if (!Array.isArray(masterManifest.pages) || masterManifest.pages.length < 18) {
  throw new Error('Master manifest page inventory is incomplete');
}
const currentHash = hashFile(args.psd);
if (currentHash.toLowerCase() !== String(sourceReceipt.sourceSha256 || '').toLowerCase()) {
  throw new Error(`Target PSD hash differs from source receipt: ${currentHash}`);
}
if (!sourceReceipt.backupPsd || !fs.existsSync(sourceReceipt.backupPsd)) {
  throw new Error(`Missing recorded family-correction backup: ${sourceReceipt.backupPsd}`);
}
if (hashFile(sourceReceipt.backupPsd).toLowerCase() !== String(sourceReceipt.backupSha256 || '').toLowerCase()) {
  throw new Error('Recorded family-correction backup hash does not match its receipt');
}
const seenPages = new Set();
for (const pageRecord of manifest.pages) {
  if (!pageRecord.name || seenPages.has(pageRecord.name)) throw new Error(`Invalid or duplicate page: ${pageRecord.name}`);
  seenPages.add(pageRecord.name);
  if (!Array.isArray(pageRecord.origin) || pageRecord.origin.length !== 2) throw new Error(`Missing origin: ${pageRecord.name}`);
  if (!Array.isArray(pageRecord.assets) || pageRecord.assets.length < 1) throw new Error(`Missing assets: ${pageRecord.name}`);
  for (const asset of pageRecord.assets) {
    if (!asset.path || !fs.existsSync(asset.path)) throw new Error(`Missing asset for ${pageRecord.name}: ${asset.path}`);
    if (!Array.isArray(asset.box) || asset.box.length !== 4) throw new Error(`Invalid asset box for ${pageRecord.name}`);
  }
}
if (seenPages.has('07_商店交易')) throw new Error('Shop page must not be a mutation target');

fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.receipt), { recursive: true });

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd.replace(/\\/g, '/'))};
  var receiptPath = ${jsxString(args.receipt.replace(/\\/g, '/'))};
  var pageRecords = ${JSON.stringify(manifest.pages)};
  var shopReference = ${JSON.stringify(manifest.shopReference)};
  var targetFile = new File(targetPsdPath);
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

  function findDirectArtLayer(container, name) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (container.artLayers[index].name == name) return container.artLayers[index];
    }
    return null;
  }

  function setLightText(layer) {
    var color = new SolidColor();
    color.rgb.red = 240;
    color.rgb.green = 222;
    color.rgb.blue = 182;
    layer.textItem.color = color;
  }

  function collectTextLayers(container, content, result) {
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      if (layer.kind == LayerKind.TEXT && layer.textItem.contents == content) result.push(layer);
    }
    for (var setIndex = 0; setIndex < container.layerSets.length; setIndex++) {
      collectTextLayers(container.layerSets[setIndex], content, result);
    }
  }

  function findRuntimeTextGroup(pageGroup) {
    return findDirectLayerSet(pageGroup, '70_RuntimeText');
  }

  function setJustification(textItem, value) {
    if (value == 'center') textItem.justification = Justification.CENTER;
    else if (value == 'right') textItem.justification = Justification.RIGHT;
    else textItem.justification = Justification.LEFT;
  }

  function alignText(layer, rule, origin) {
    setJustification(layer.textItem, rule.justification);
    var bounds = layerBounds(layer);
    var desiredX = Number(origin[0]) + Number(rule.target[0]);
    var desiredY = Number(origin[1]) + Number(rule.target[1]);
    var deltaX = 0;
    var deltaY = desiredY - bounds[1];
    if (rule.mode == 'center') {
      deltaX = desiredX - (bounds[0] + bounds[2]) / 2;
      deltaY = desiredY - (bounds[1] + bounds[3]) / 2;
    } else if (rule.mode == 'topRight') {
      deltaX = desiredX - bounds[2];
    } else {
      deltaX = desiredX - bounds[0];
    }
    layer.translate(UnitValue(deltaX, 'px'), UnitValue(deltaY, 'px'));
  }

  function importImage(targetDocument, targetGroup, pageRecord, asset) {
    var sourceFile = new File(asset.path);
    if (!sourceFile.exists) throw new Error('Missing image: ' + asset.path);
    var sourceDocument = null;
    var duplicated = null;
    try {
      sourceDocument = app.open(sourceFile);
      var targetWidth = Number(asset.box[2]);
      var targetHeight = Number(asset.box[3]);
      if (Math.round(sourceDocument.width.as('px')) != Math.round(targetWidth) || Math.round(sourceDocument.height.as('px')) != Math.round(targetHeight)) {
        sourceDocument.resizeImage(
          UnitValue(targetWidth, 'px'),
          UnitValue(targetHeight, 'px'),
          null,
          ResampleMethod.BICUBICSHARPER
        );
      }
      var sourceLayer = sourceDocument.activeLayer;
      var sourceBounds = layerBounds(sourceLayer);
      duplicated = sourceLayer.duplicate(targetDocument, ElementPlacement.PLACEATBEGINNING);
      sourceDocument.close(SaveOptions.DONOTSAVECHANGES);
      sourceDocument = null;
      app.activeDocument = targetDocument;
      targetDocument.activeLayer = duplicated;
      duplicated.name = asset.name;
      var duplicatedBounds = layerBounds(duplicated);
      var desiredLeft = Number(pageRecord.origin[0]) + Number(asset.box[0]) + sourceBounds[0];
      var desiredTop = Number(pageRecord.origin[1]) + Number(asset.box[1]) + sourceBounds[1];
      duplicated.translate(
        UnitValue(desiredLeft - duplicatedBounds[0], 'px'),
        UnitValue(desiredTop - duplicatedBounds[1], 'px')
      );
      duplicated.move(targetGroup, ElementPlacement.INSIDE);
      duplicated.visible = true;
      return duplicated;
    } finally {
      if (sourceDocument) {
        try { sourceDocument.close(SaveOptions.DONOTSAVECHANGES); } catch (ignored) {}
      }
    }
  }

  function copyShopGlobalLayers(shopGlobal, correctionGroup, pageRecord) {
    if (shopGlobal.layerSets.length != 0) {
      throw new Error('Shop global-content group unexpectedly contains child groups.');
    }
    var targetGroup = correctionGroup.layerSets.add();
    targetGroup.name = '20_GlobalShell_V2';
    var deltaX = Number(pageRecord.origin[0]) - Number(shopReference.origin[0]);
    var deltaY = Number(pageRecord.origin[1]) - Number(shopReference.origin[1]);
    for (var artIndex = shopGlobal.artLayers.length - 1; artIndex >= 0; artIndex--) {
      var copied = shopGlobal.artLayers[artIndex].duplicate();
      copied.move(targetGroup, ElementPlacement.INSIDE);
      copied.translate(UnitValue(deltaX, 'px'), UnitValue(deltaY, 'px'));
    }
    if (targetGroup.artLayers.length != shopGlobal.artLayers.length) {
      throw new Error('Shop global-content layer count changed during copy.');
    }
    return targetGroup;
  }

  function preflightPage(pageRecord, pageGroup) {
    if (findDirectLayerSet(pageGroup, '00_FamilyCorrection')) {
      throw new Error('00_FamilyCorrection already exists in ' + pageRecord.name);
    }
    for (var hideIndex = 0; hideIndex < pageRecord.hideGroups.length; hideIndex++) {
      if (!findDirectLayerSet(pageGroup, pageRecord.hideGroups[hideIndex])) {
        throw new Error('Missing legacy group ' + pageRecord.hideGroups[hideIndex] + ' in ' + pageRecord.name);
      }
    }
    for (var preserveIndex = 0; preserveIndex < pageRecord.preserveGroups.length; preserveIndex++) {
      if (!findDirectLayerSet(pageGroup, pageRecord.preserveGroups[preserveIndex])) {
        throw new Error('Missing preserved group ' + pageRecord.preserveGroups[preserveIndex] + ' in ' + pageRecord.name);
      }
    }
    for (var clusterIndex = 0; clusterIndex < pageRecord.clusterRules.length; clusterIndex++) {
      var clusterRule = pageRecord.clusterRules[clusterIndex];
      if (!findDirectLayerSet(pageGroup, clusterRule.anchorGroup)) {
        throw new Error('Missing cluster anchor ' + clusterRule.anchorGroup + ' in ' + pageRecord.name);
      }
      for (var memberIndex = 0; memberIndex < clusterRule.members.length; memberIndex++) {
        if (!findDirectLayerSet(pageGroup, clusterRule.members[memberIndex])) {
          throw new Error('Missing cluster member ' + clusterRule.members[memberIndex] + ' in ' + pageRecord.name);
        }
      }
    }
    var runtimeText = findRuntimeTextGroup(pageGroup);
    if ((pageRecord.hiddenTextContents.length > 0 || pageRecord.textRules.length > 0) && !runtimeText) {
      throw new Error('Missing 70_RuntimeText in ' + pageRecord.name);
    }
    for (var ruleIndex = 0; ruleIndex < pageRecord.textRules.length; ruleIndex++) {
      var matches = [];
      collectTextLayers(runtimeText, pageRecord.textRules[ruleIndex].content, matches);
      if (matches.length != 1) {
        throw new Error('Expected one text match for "' + pageRecord.textRules[ruleIndex].content + '" in ' + pageRecord.name + ', got ' + matches.length);
      }
    }
    for (var layerRuleIndex = 0; layerRuleIndex < pageRecord.layerTextRules.length; layerRuleIndex++) {
      var layerRule = pageRecord.layerTextRules[layerRuleIndex];
      var layerMatch = findDirectArtLayer(runtimeText, layerRule.layer);
      if (!layerMatch || layerMatch.kind != LayerKind.TEXT) {
        throw new Error('Missing named text layer "' + layerRule.layer + '" in ' + pageRecord.name);
      }
    }
    for (var lightIndex = 0; lightIndex < pageRecord.lightTextLayers.length; lightIndex++) {
      var lightMatch = findDirectArtLayer(runtimeText, pageRecord.lightTextLayers[lightIndex]);
      if (!lightMatch || lightMatch.kind != LayerKind.TEXT) {
        throw new Error('Missing light text layer "' + pageRecord.lightTextLayers[lightIndex] + '" in ' + pageRecord.name);
      }
    }
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

  function jsonPageArray(records) {
    var parts = [];
    for (var index = 0; index < records.length; index++) {
      var record = records[index];
      parts.push('{' +
        '"page":' + jsonQuote(record.page) + ',' +
        '"family":' + jsonQuote(record.family) + ',' +
        '"group":' + jsonQuote(record.group) + ',' +
        '"importedLayerCount":' + record.importedLayerCount + ',' +
        '"importedLayers":' + jsonStringArray(record.importedLayers) + ',' +
        '"hiddenLegacyGroups":' + jsonStringArray(record.hiddenLegacyGroups) + ',' +
        '"preservedGroups":' + jsonStringArray(record.preservedGroups) +
        '}');
    }
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
    if (!doc.saved) throw new Error('Target PSD has unsaved changes.');
    initialHistoryState = doc.activeHistoryState;

    var shopPage = findDirectLayerSet(doc, shopReference.name);
    if (!shopPage) throw new Error('Missing shop reference page.');
    var shopGlobal = findDirectLayerSet(shopPage, shopReference.globalGroup);
    if (!shopGlobal) throw new Error('Missing shop global-content group.');

    var pageGroups = [];
    for (var preflightIndex = 0; preflightIndex < pageRecords.length; preflightIndex++) {
      var preflightRecord = pageRecords[preflightIndex];
      var preflightGroup = findDirectLayerSet(doc, preflightRecord.name);
      if (!preflightGroup) throw new Error('Missing target page ' + preflightRecord.name);
      preflightPage(preflightRecord, preflightGroup);
      pageGroups.push(preflightGroup);
    }

    var receiptPages = [];
    for (var pageIndex = 0; pageIndex < pageRecords.length; pageIndex++) {
      var pageRecord = pageRecords[pageIndex];
      var pageGroup = pageGroups[pageIndex];
      var originalPageLayers = [];
      for (var originalLayerIndex = 0; originalLayerIndex < pageGroup.layers.length; originalLayerIndex++) {
        originalPageLayers.push(pageGroup.layers[originalLayerIndex]);
      }
      var correctionGroup = pageGroup.layerSets.add();
      correctionGroup.name = '00_FamilyCorrection';
      var shellGroup = correctionGroup.layerSets.add();
      shellGroup.name = '00_ShellComponents';
      var importedNames = [];
      for (var assetIndex = 0; assetIndex < pageRecord.assets.length; assetIndex++) {
        var importedLayer = importImage(doc, shellGroup, pageRecord, pageRecord.assets[assetIndex]);
        importedNames.push(importedLayer.name);
      }

      if (pageRecord.duplicateShopGlobal) {
        copyShopGlobalLayers(shopGlobal, correctionGroup, pageRecord);
      }

      var hiddenLegacyNames = [];
      for (var hideIndex = 0; hideIndex < pageRecord.hideGroups.length; hideIndex++) {
        var legacy = findDirectLayerSet(pageGroup, pageRecord.hideGroups[hideIndex]);
        var originalName = legacy.name;
        legacy.name = '99_Legacy_' + originalName;
        legacy.visible = false;
        hiddenLegacyNames.push(legacy.name);
      }

      var runtimeText = findRuntimeTextGroup(pageGroup);
      for (var hiddenTextIndex = 0; hiddenTextIndex < pageRecord.hiddenTextContents.length; hiddenTextIndex++) {
        var hiddenTextMatches = [];
        collectTextLayers(runtimeText, pageRecord.hiddenTextContents[hiddenTextIndex], hiddenTextMatches);
        for (var matchIndex = 0; matchIndex < hiddenTextMatches.length; matchIndex++) hiddenTextMatches[matchIndex].visible = false;
      }

      for (var clusterIndex = 0; clusterIndex < pageRecord.clusterRules.length; clusterIndex++) {
        var clusterRule = pageRecord.clusterRules[clusterIndex];
        var anchorGroup = findDirectLayerSet(pageGroup, clusterRule.anchorGroup);
        var anchorBounds = layerBounds(anchorGroup);
        var targetCenterX = Number(pageRecord.origin[0]) + Number(clusterRule.targetCenter[0]);
        var targetCenterY = Number(pageRecord.origin[1]) + Number(clusterRule.targetCenter[1]);
        var clusterDeltaX = targetCenterX - (anchorBounds[0] + anchorBounds[2]) / 2;
        var clusterDeltaY = targetCenterY - (anchorBounds[1] + anchorBounds[3]) / 2;
        for (var memberIndex = 0; memberIndex < clusterRule.members.length; memberIndex++) {
          var memberGroup = findDirectLayerSet(pageGroup, clusterRule.members[memberIndex]);
          memberGroup.translate(UnitValue(clusterDeltaX, 'px'), UnitValue(clusterDeltaY, 'px'));
        }
      }

      for (var textRuleIndex = 0; textRuleIndex < pageRecord.textRules.length; textRuleIndex++) {
        var textMatches = [];
        collectTextLayers(runtimeText, pageRecord.textRules[textRuleIndex].content, textMatches);
        alignText(textMatches[0], pageRecord.textRules[textRuleIndex], pageRecord.origin);
      }

      for (var layerTextIndex = 0; layerTextIndex < pageRecord.layerTextRules.length; layerTextIndex++) {
        var namedRule = pageRecord.layerTextRules[layerTextIndex];
        alignText(findDirectArtLayer(runtimeText, namedRule.layer), namedRule, pageRecord.origin);
      }

      for (var lightTextIndex = 0; lightTextIndex < pageRecord.lightTextLayers.length; lightTextIndex++) {
        setLightText(findDirectArtLayer(runtimeText, pageRecord.lightTextLayers[lightTextIndex]));
      }

      for (var reorderIndex = 0; reorderIndex < originalPageLayers.length; reorderIndex++) {
        originalPageLayers[reorderIndex].move(correctionGroup, ElementPlacement.PLACEBEFORE);
      }
      if (pageGroup.layers[pageGroup.layers.length - 1].name != '00_FamilyCorrection') {
        throw new Error('Correction group is not bottom-most in ' + pageRecord.name);
      }

      for (var preserveIndex = 0; preserveIndex < pageRecord.preserveGroups.length; preserveIndex++) {
        if (!findDirectLayerSet(pageGroup, pageRecord.preserveGroups[preserveIndex])) {
          throw new Error('Preserved group disappeared: ' + pageRecord.preserveGroups[preserveIndex] + ' in ' + pageRecord.name);
        }
      }
      receiptPages.push({
        page: pageRecord.name,
        family: pageRecord.family,
        group: correctionGroup.name,
        importedLayerCount: importedNames.length,
        importedLayers: importedNames,
        hiddenLegacyGroups: hiddenLegacyNames,
        preservedGroups: pageRecord.preserveGroups
      });
    }

    doc.save();
    var receiptText = '{' +
      '"pageCount":' + receiptPages.length + ',' +
      '"familyCount":4,' +
      '"psd":' + jsonQuote(targetFile.fsName) + ',' +
      '"pages":' + jsonPageArray(receiptPages) +
      '}';
    var receiptFile = new File(receiptPath);
    if (!receiptFile.parent.exists && !receiptFile.parent.create()) throw new Error('Could not create receipt directory.');
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
  pageCount: manifest.pages.length,
  output: args.output,
  receipt: args.receipt,
}));
