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
  { name: '18_主角背包_卡组页', origin: [6120, 3600] },
].map((record) => ({
  ...record,
  path: path.join(args['export-dir'], `${record.name}.png`).replace(/\\/g, '/'),
}));

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exports)};
  var sourceName = '03_主角背包';
  var targetName = '18_主角背包_卡组页';
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) { return String(value || '').replace(/\\\\/g, '/').toLowerCase(); }
  function findOpenDocument(file) { var expected = normalizedPath(file.fsName); for (var index = 0; index < app.documents.length; index++) { var candidate = app.documents[index]; try { if (normalizedPath(candidate.fullName.fsName) == expected) return candidate; } catch (ignored) {} } return null; }
  function findDirectLayerSet(container, name) { for (var index = 0; index < container.layerSets.length; index++) if (container.layerSets[index].name == name) return container.layerSets[index]; return null; }
  function requireDirectLayerSet(container, name) { var group = findDirectLayerSet(container, name); if (!group) throw new Error('Missing required group ' + name + ' in ' + container.name); return group; }
  function findDirectArtLayer(container, name) { for (var index = 0; index < container.artLayers.length; index++) if (container.artLayers[index].name == name) return container.artLayers[index]; return null; }
  function requireDirectArtLayer(container, name) { var layer = findDirectArtLayer(container, name); if (!layer) throw new Error('Missing required layer ' + name + ' in ' + container.name); return layer; }
  function exportPage(record) { var exportDocument = doc.duplicate('GameXXK_Page18Workspace_' + record.name, true); app.activeDocument = exportDocument; exportDocument.crop([UnitValue(record.origin[0], 'px'), UnitValue(record.origin[1], 'px'), UnitValue(record.origin[0] + 1920, 'px'), UnitValue(record.origin[1] + 1080, 'px')]); if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT; var options = new PNGSaveOptions(); options.interlaced = false; exportDocument.saveAs(new File(record.path), options, true, Extension.LOWERCASE); exportDocument.close(SaveOptions.DONOTSAVECHANGES); app.activeDocument = doc; }
  function writeUtf8(filePath, contents) { var file = new File(filePath); file.encoding = 'UTF8'; file.open('w'); file.write(contents); file.close(); }
  function escapeJsonString(value) { var slash = String.fromCharCode(92); var quote = String.fromCharCode(34); return String(value).split(slash).join(slash + slash).split(quote).join(slash + quote).split(String.fromCharCode(13)).join(slash + 'r').split(String.fromCharCode(10)).join(slash + 'n').split(String.fromCharCode(9)).join(slash + 't'); }
  function stringifyJson(value) { if (value === null) return 'null'; var valueType = typeof value; if (valueType == 'string') return '"' + escapeJsonString(value) + '"'; if (valueType == 'number') return isFinite(value) ? String(value) : 'null'; if (valueType == 'boolean') return value ? 'true' : 'false'; if (value instanceof Array) { var parts = []; for (var arrayIndex = 0; arrayIndex < value.length; arrayIndex++) parts.push(stringifyJson(value[arrayIndex])); return '[' + parts.join(',') + ']'; } var objectParts = []; for (var key in value) if (value.hasOwnProperty(key)) objectParts.push('"' + escapeJsonString(key) + '":' + stringifyJson(value[key])); return '{' + objectParts.join(',') + '}'; }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before page duplication');
    initialHistoryState = doc.activeHistoryState;
    var source = findDirectLayerSet(doc, sourceName);
    if (!source) throw new Error('Missing source page: ' + sourceName);
    if (findDirectLayerSet(doc, targetName)) throw new Error('Target page already exists: ' + targetName);
    var beforeTopLevelCount = doc.layerSets.length;
    var target = source.duplicate(doc, ElementPlacement.PLACEATEND);
    target.name = targetName;
    target.translate(UnitValue(0, 'px'), UnitValue(3600, 'px'));
    target.visible = true;
    // The approved close icon was manually placed in the shared-component group at page-03
    // coordinates, so it visually overlays page 03 but is not a child of the source page.
    // Copy that same real layer into the page-18 workspace explicitly.
    var sharedComponents = requireDirectLayerSet(doc, '00_公共组件');
    var sharedWindowControls = requireDirectLayerSet(sharedComponents, '02_WindowControls');
    var sharedCloseButton = requireDirectArtLayer(sharedWindowControls, '01_CloseButton');
    var targetWindowControls = target.layerSets.add();
    targetWindowControls.name = '02_WindowControls';
    var targetCloseButton = sharedCloseButton.duplicate(targetWindowControls, ElementPlacement.PLACEATBEGINNING);
    targetCloseButton.name = '01_CloseButton';
    targetCloseButton.translate(UnitValue(0, 'px'), UnitValue(3600, 'px'));
    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();
    var report = {
      status: 'PASS',
      sourcePage: sourceName,
      targetPage: targetName,
      translation: [0, 3600],
      beforeTopLevelCount: beforeTopLevelCount,
      afterTopLevelCount: doc.layerSets.length,
      exports: exportRecords
    };
    writeUtf8(reportPath, stringifyJson(report));
    if (openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES);
  } catch (error) {
    try { if (doc && initialHistoryState) { app.activeDocument = doc; doc.activeHistoryState = initialHistoryState; } if (doc && openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES); } catch (rollbackError) {}
    try { writeUtf8(reportPath + '.error.txt', String(error)); } catch (writeError) {}
    throw error;
  } finally {
    app.preferences.rulerUnits = oldUnits;
    app.displayDialogs = oldDialogs;
  }
})();`;

fs.writeFileSync(args.output, jsx, 'utf8');
