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

function jsxString(value) {
  return JSON.stringify(String(value).replace(/\\/g, '/'));
}

const args = parseArguments(process.argv.slice(2));
requireFile(args.psd, 'master PSD');
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
fs.mkdirSync(args['export-dir'], { recursive: true });

const exports = [
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
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exports)};
  var sourceOrigin = [6120, 0];
  var targetOrigin = [6120, 2400];
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

  function findDirectArtLayer(container, name) {
    for (var index = 0; index < container.artLayers.length; index++) {
      if (container.artLayers[index].name == name) return container.artLayers[index];
    }
    return null;
  }

  function requireDirectArtLayer(container, name) {
    var layer = findDirectArtLayer(container, name);
    if (!layer) throw new Error('Missing required art layer ' + name + ' in ' + container.name);
    return layer;
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
    return [
      bounds[0] - origin[0],
      bounds[1] - origin[1],
      bounds[2] - origin[0],
      bounds[3] - origin[1]
    ];
  }

  function colorFromHex(value) {
    var clean = String(value).replace('#', '');
    var color = new SolidColor();
    color.rgb.red = parseInt(clean.substring(0, 2), 16);
    color.rgb.green = parseInt(clean.substring(2, 4), 16);
    color.rgb.blue = parseInt(clean.substring(4, 6), 16);
    return color;
  }

  function addCenteredText(group, name, contents, button, size, color, bold) {
    if (findDirectArtLayer(group, name)) throw new Error('Text layer already exists: ' + group.name + '/' + name);
    var buttonBounds = boundsOf(button);
    var centerX = (buttonBounds[0] + buttonBounds[2]) / 2;
    var centerY = (buttonBounds[1] + buttonBounds[3]) / 2;
    var layer = group.artLayers.add();
    layer.name = name;
    layer.kind = LayerKind.TEXT;
    var item = layer.textItem;
    item.contents = contents;
    item.position = [UnitValue(centerX, 'px'), UnitValue(centerY, 'px')];
    item.size = UnitValue(size, 'pt');
    item.font = 'MicrosoftYaHei';
    item.color = colorFromHex(color);
    item.justification = Justification.CENTER;
    try { item.fauxBold = !!bold; } catch (ignored) {}
    try { item.antiAliasMethod = AntiAlias.SMOOTH; } catch (ignored2) {}
    var textBounds = boundsOf(layer);
    var textCenterX = (textBounds[0] + textBounds[2]) / 2;
    var textCenterY = (textBounds[1] + textBounds[3]) / 2;
    layer.translate(UnitValue(centerX - textCenterX, 'px'), UnitValue(centerY - textCenterY, 'px'));
    return layer;
  }

  function cloneArtLayer(source, targetGroup, deltaX, deltaY) {
    var copied = source.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    copied.move(targetGroup, ElementPlacement.INSIDE);
    copied.name = source.name;
    if (deltaX || deltaY) {
      copied.translate(UnitValue(deltaX, 'px'), UnitValue(deltaY, 'px'));
    }
    copied.visible = source.visible;
    return copied;
  }

  function cloneLayerSet(source, targetParent, targetName, deltaX, deltaY, forceVisible) {
    var target = targetParent.layerSets.add();
    target.name = targetName;
    for (var index = source.layers.length - 1; index >= 0; index--) {
      var sourceLayer = source.layers[index];
      if (sourceLayer.typename == 'LayerSet') {
        cloneLayerSet(sourceLayer, target, sourceLayer.name, deltaX, deltaY, null);
      } else {
        cloneArtLayer(sourceLayer, target, deltaX, deltaY);
      }
    }
    try { target.opacity = source.opacity; } catch (ignored) {}
    try { target.blendMode = source.blendMode; } catch (ignored2) {}
    target.visible = forceVisible === null ? source.visible : forceVisible;
    return target;
  }

  function hideTargetVisibleGroups(targetPage) {
    var groups = [];
    for (var index = 0; index < targetPage.layerSets.length; index++) {
      var group = targetPage.layerSets[index];
      if (group.visible && !beginsWith(group.name, '99_')) groups.push(group);
    }
    var hiddenNames = [];
    for (var groupIndex = 0; groupIndex < groups.length; groupIndex++) {
      var current = groups[groupIndex];
      var legacyName = '99_PreUser03Sync_' + current.name;
      if (findDirectLayerSet(targetPage, legacyName)) {
        throw new Error('Target preservation group already exists: ' + legacyName);
      }
      current.name = legacyName;
      current.visible = false;
      hiddenNames.push(legacyName);
    }
    return hiddenNames;
  }

  function cloneVisibleSourceGroups(sourcePage, targetPage) {
    var clonedNames = [];
    for (var index = sourcePage.layers.length - 1; index >= 0; index--) {
      var layer = sourcePage.layers[index];
      if (layer.typename != 'LayerSet' || !layer.visible || beginsWith(layer.name, '99_')) continue;
      cloneLayerSet(layer, targetPage, layer.name, targetOrigin[0] - sourceOrigin[0], targetOrigin[1] - sourceOrigin[1], true);
      clonedNames.push(layer.name);
    }
    return clonedNames;
  }

  function forceVisibleTree(container) {
    container.visible = true;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) container.artLayers[artIndex].visible = true;
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) forceVisibleTree(container.layerSets[groupIndex]);
  }

  function visibleSignature(container, origin, ancestorVisible, path) {
    var containerVisible = ancestorVisible && container.visible !== false;
    if (!containerVisible) return '';
    var records = [];
    for (var index = 0; index < container.layers.length; index++) {
      var layer = container.layers[index];
      if (!layer.visible) continue;
      var currentPath = path ? path + '/' + layer.name : layer.name;
      if (layer.typename == 'LayerSet') {
        records.push('G:' + currentPath + '{' + visibleSignature(layer, origin, containerVisible, currentPath) + '}');
      } else {
        var bounds = localBounds(layer, origin);
        var record = 'A:' + currentPath + ':[' + bounds.join(',') + ']';
        try {
          if (layer.kind == LayerKind.TEXT) record += ':text=' + String(layer.textItem.contents);
        } catch (ignored) {}
        records.push(record);
      }
    }
    return records.join('|');
  }

  function countVisibleArt(container, ancestorVisible) {
    var containerVisible = ancestorVisible && container.visible !== false;
    if (!containerVisible) return 0;
    var count = 0;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      if (container.artLayers[artIndex].visible) count++;
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      count += countVisibleArt(container.layerSets[groupIndex], containerVisible);
    }
    return count;
  }

  function exportPage(record) {
    var exportDocument = doc.duplicate('GameXXK_User03Canonical_' + record.name, true);
    app.activeDocument = exportDocument;
    exportDocument.crop([
      UnitValue(record.origin[0], 'px'),
      UnitValue(record.origin[1], 'px'),
      UnitValue(record.origin[0] + 1920, 'px'),
      UnitValue(record.origin[1] + 1080, 'px')
    ]);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) {
      exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
    }
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
      var arrayParts = [];
      for (var arrayIndex = 0; arrayIndex < value.length; arrayIndex++) arrayParts.push(stringifyJson(value[arrayIndex]));
      return '[' + arrayParts.join(',') + ']';
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
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before user-03 sync');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');

    var sourcePage = requireDirectLayerSet(doc, '03_主角背包');
    var targetPage = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    var sourceActions = requireDirectLayerSet(sourcePage, '43_InventoryActions');
    var sourceSelection = requireDirectLayerSet(sourcePage, '39_SelectedSlotInk');
    var sourceButton = requireDirectArtLayer(sourceActions, '01_DecomposeButton');
    var sourceScrollbar = requireDirectLayerSet(sourcePage, '42_InventoryScrollbar');
    var sourceThumb = requireDirectArtLayer(sourceScrollbar, 'inventory_scrollbar_Button');
    var sourceButtonBefore = localBounds(sourceButton, sourceOrigin);
    var sourceThumbBounds = localBounds(sourceThumb, sourceOrigin);
    if (sourceButtonBefore[0] < 900 || sourceButtonBefore[2] > 1070 || sourceButtonBefore[1] < 850 || sourceButtonBefore[3] > 960) {
      throw new Error('Unexpected user-authored decompose button bounds: ' + sourceButtonBefore.join(','));
    }
    if (sourceThumbBounds[0] < 1600 || sourceThumbBounds[2] > 1700 || sourceThumbBounds[1] < 280 || sourceThumbBounds[3] > 500) {
      throw new Error('Unexpected user-authored scrollbar button bounds: ' + sourceThumbBounds.join(','));
    }

    sourceSelection.visible = false;
    var sourceLabel = addCenteredText(sourceActions, '02_DecomposeLabel', '分解', sourceButton, 22, '#2B2822', true);
    var sourceButtonBounds = localBounds(sourceButton, sourceOrigin);
    var sourceLabelBounds = localBounds(sourceLabel, sourceOrigin);
    var buttonCenterX = (sourceButtonBounds[0] + sourceButtonBounds[2]) / 2;
    var buttonCenterY = (sourceButtonBounds[1] + sourceButtonBounds[3]) / 2;
    var labelCenterX = (sourceLabelBounds[0] + sourceLabelBounds[2]) / 2;
    var labelCenterY = (sourceLabelBounds[1] + sourceLabelBounds[3]) / 2;
    if (Math.abs(buttonCenterX - labelCenterX) > 1 || Math.abs(buttonCenterY - labelCenterY) > 1) {
      throw new Error('Decompose label was not centered on the user button');
    }

    var hiddenTargetGroups = hideTargetVisibleGroups(targetPage);
    var clonedGroupsBottomToTop = cloneVisibleSourceGroups(sourcePage, targetPage);
    var targetSlots = requireDirectLayerSet(targetPage, '40_InventorySlots');
    var targetSelection = cloneLayerSet(
      sourceSelection,
      targetPage,
      '39_SelectedSlotInk',
      targetOrigin[0] - sourceOrigin[0],
      targetOrigin[1] - sourceOrigin[1],
      true
    );
    targetSelection.move(targetSlots, ElementPlacement.PLACEAFTER);
    forceVisibleTree(targetSelection);
    targetSelection.visible = false;
    var sourceSignature = visibleSignature(sourcePage, sourceOrigin, true, '');
    var targetSignature = visibleSignature(targetPage, targetOrigin, true, '');
    if (sourceSignature != targetSignature) throw new Error('Base source/target signatures differ after user-03 sync');
    targetSelection.visible = true;
    forceVisibleTree(targetSelection);

    var targetActions = requireDirectLayerSet(targetPage, '43_InventoryActions');
    var targetButton = requireDirectArtLayer(targetActions, '01_DecomposeButton');
    var targetLabel = requireDirectArtLayer(targetActions, '02_DecomposeLabel');
    var targetScrollbar = requireDirectLayerSet(targetPage, '42_InventoryScrollbar');
    var targetThumb = requireDirectArtLayer(targetScrollbar, 'inventory_scrollbar_Button');

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();

    var report = {
      status: 'PASS',
      topLevelPageCount: doc.layerSets.length,
      sourcePage: sourcePage.name,
      targetPage: targetPage.name,
      hiddenTargetGroups: hiddenTargetGroups,
      clonedGroupsBottomToTop: clonedGroupsBottomToTop,
      sourceVisibleArtCount: countVisibleArt(sourcePage, true),
      targetVisibleArtCount: countVisibleArt(targetPage, true),
      selectionVisibleOnlyOnTarget: !sourceSelection.visible && targetSelection.visible,
      selectionArtCount: countVisibleArt(targetSelection, true),
      sourceButtonBounds: sourceButtonBounds,
      sourceLabelBounds: sourceLabelBounds,
      sourceScrollbarButtonBounds: sourceThumbBounds,
      targetButtonBounds: localBounds(targetButton, targetOrigin),
      targetLabelBounds: localBounds(targetLabel, targetOrigin),
      targetScrollbarButtonBounds: localBounds(targetThumb, targetOrigin),
      baseVisibleSignatureMatch: sourceSignature == targetSignature,
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
