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

const exportRecords = [
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
  var exportRecords = ${JSON.stringify(exportRecords)};
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

  function findRecursiveArtLayer(container, name) {
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      if (container.artLayers[artIndex].name == name) return container.artLayers[artIndex];
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      var found = findRecursiveArtLayer(container.layerSets[groupIndex], name);
      if (found) return found;
    }
    return null;
  }

  function pixels(value) {
    return Number(value.as('px'));
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

  function directLayerIndex(container, target) {
    for (var index = 0; index < container.layers.length; index++) {
      if (container.layers[index] == target) return index;
    }
    return -1;
  }

  function exportPage(record) {
    var exportDocument = doc.duplicate('GameXXK_ApprovedSelection_' + record.name, true);
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
      for (var arrayIndex = 0; arrayIndex < value.length; arrayIndex++) {
        arrayParts.push(stringifyJson(value[arrayIndex]));
      }
      return '[' + arrayParts.join(',') + ']';
    }
    var objectParts = [];
    for (var key in value) {
      if (value.hasOwnProperty(key)) {
        objectParts.push('"' + escapeJsonString(key) + '":' + stringifyJson(value[key]));
      }
    }
    return '{' + objectParts.join(',') + '}';
  }

  function collectVisibleOverlap(container, ancestorPath, inheritedVisible, queryBounds, output) {
    var containerVisible = inheritedVisible && container.visible !== false;
    if (!containerVisible) return;
    var currentPath = ancestorPath ? ancestorPath + '/' + container.name : container.name;
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var layer = container.artLayers[artIndex];
      if (!layer.visible) continue;
      var layerBounds = roundBounds(boundsOf(layer));
      var overlaps = layerBounds[0] < queryBounds[2] && layerBounds[2] > queryBounds[0]
        && layerBounds[1] < queryBounds[3] && layerBounds[3] > queryBounds[1];
      if (overlaps) output.push({ path: currentPath + '/' + layer.name, bounds: layerBounds });
    }
    for (var groupIndex = 0; groupIndex < container.layerSets.length; groupIndex++) {
      collectVisibleOverlap(container.layerSets[groupIndex], currentPath, containerVisible, queryBounds, output);
    }
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before selection-ink repair');
    initialHistoryState = doc.activeHistoryState;
    if (doc.layerSets.length != 18) throw new Error('Expected eighteen top-level pages');

    var shopPage = requireDirectLayerSet(doc, '07_商店交易');
    var selectedPage = requireDirectLayerSet(doc, '13_主角背包_物品选中');
    var sourceInk = findRecursiveArtLayer(shopPage, '032_selected_selection_ink');
    if (!sourceInk) throw new Error('Missing shop selection ink');
    var sourceBounds = roundBounds(boundsOf(sourceInk));
    if (sourceBounds[2] - sourceBounds[0] != 194 || sourceBounds[3] - sourceBounds[1] != 66) {
      throw new Error('Unexpected shop selection ink bounds: ' + sourceBounds.join(','));
    }

    var oldSelected = requireDirectLayerSet(selectedPage, '44_SelectedInventorySlot');
    oldSelected.name = '99_Legacy_SelectedInventorySlot';
    oldSelected.visible = false;
    if (oldSelected.visible) throw new Error('Legacy selected group remained visible immediately after rename');

    var inventorySlots = requireDirectLayerSet(selectedPage, '40_InventorySlots');
    if (findDirectLayerSet(selectedPage, '39_SelectedSlotInk')) {
      throw new Error('Approved selection-ink group already exists');
    }
    var selectedInkGroup = selectedPage.layerSets.add();
    selectedInkGroup.name = '39_SelectedSlotInk';
    selectedInkGroup.visible = true;
    var selectedInk = sourceInk.duplicate(selectedInkGroup, ElementPlacement.INSIDE);
    selectedInk.name = '01_SharedShopSelectionInk';
    selectedInk.resize(64.7058823529, 64.7058823529, AnchorPosition.MIDDLECENTER);

    var targetLeft = 7247;
    var targetTop = 2684;
    var resizedBounds = boundsOf(selectedInk);
    selectedInk.translate(targetLeft - resizedBounds[0], targetTop - resizedBounds[1]);
    selectedInkGroup.move(inventorySlots, ElementPlacement.PLACEAFTER);

    var finalBounds = roundBounds(boundsOf(selectedInk));
    var finalWidth = finalBounds[2] - finalBounds[0];
    var finalHeight = finalBounds[3] - finalBounds[1];
    if (Math.abs(finalWidth - 126) > 1 || Math.abs(finalHeight - 43) > 1) {
      throw new Error('Unexpected approved selection ink size: ' + finalBounds.join(','));
    }
    if (Math.abs(finalBounds[0] - targetLeft) > 1 || Math.abs(finalBounds[1] - targetTop) > 1) {
      throw new Error('Unexpected approved selection ink position: ' + finalBounds.join(','));
    }
    var slotIndex = directLayerIndex(selectedPage, inventorySlots);
    var inkIndex = directLayerIndex(selectedPage, selectedInkGroup);
    if (slotIndex < 0 || inkIndex != slotIndex + 1) {
      throw new Error('Selection ink is not immediately behind inventory slots: slots=' + slotIndex + ', ink=' + inkIndex);
    }
    oldSelected.visible = false;
    if (oldSelected.visible) throw new Error('Legacy selected group became visible before export');

    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) {
      exportPage(exportRecords[exportIndex]);
    }
    doc.save();

    var visibleFirstSlotLayers = [];
    collectVisibleOverlap(selectedPage, '', true, [7240, 2675, 7380, 2830], visibleFirstSlotLayers);
    var report = {
      status: 'PASS',
      topLevelPageCount: doc.layerSets.length,
      sourceLayer: '07_商店交易/40_ProductGrid/032_selected_selection_ink',
      sourceBounds: sourceBounds,
      targetGroup: '13_主角背包_物品选中/39_SelectedSlotInk',
      targetLayer: '01_SharedShopSelectionInk',
      targetBounds: finalBounds,
      selectionScalePercent: 64.7058823529,
      inventorySlotsLayerIndex: slotIndex,
      selectionInkLayerIndex: inkIndex,
      selectionImmediatelyBehindSlots: true,
      legacySelectedGroup: oldSelected.name,
      legacySelectedGroupVisible: oldSelected.visible,
      duplicateItemAdded: false,
      visibleFirstSlotLayers: visibleFirstSlotLayers,
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
