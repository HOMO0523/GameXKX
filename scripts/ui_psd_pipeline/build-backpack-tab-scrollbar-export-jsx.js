// Generates a Photoshop JSX that exports only the approved backpack scrollbar
// thumb. The rejected legacy tab backgrounds are intentionally not exported.
import fs from 'node:fs';
import path from 'node:path';

function parseArguments(values) {
  const required = ['psd', 'output', 'report', 'export-dir'];
  const result = {};
  for (const name of required) {
    const flag = `--${name}`;
    const index = values.indexOf(flag);
    if (index === -1 || !values[index + 1] || values[index + 1].startsWith('--')) throw new Error(`${flag} requires a value`);
    result[name] = path.resolve(values[index + 1]);
  }
  return result;
}

function requireFile(filePath, label) {
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) throw new Error(`Missing ${label}: ${filePath}`);
}

function jsxString(value) { return JSON.stringify(String(value).replace(/\\/g, '/')); }

const args = parseArguments(process.argv.slice(2));
requireFile(args.psd, 'master PSD');
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
fs.mkdirSync(args['export-dir'], { recursive: true });

// Page 03 origin inside the master PSD: x 6120, y 0.
const PAGE_ORIGIN = [6120, 0];
const exports = [
  { name: 'T_MasterV2_BackpackScrollbarThumb', path: '42_InventoryScrollbar/inventory_scrollbar_Button' },
].map((record) => ({ ...record, out: path.join(args['export-dir'], `${record.name}.png`).replace(/\\/g, '/') }));

const jsx = `#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  var targetPsdPath = ${jsxString(args.psd)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exports)};
  var doc = null;
  var openedByScript = false;
  function normalizedPath(value) { return String(value || '').replace(/\\\\/g, '/').toLowerCase(); }
  function findOpenDocument(file) { var expected = normalizedPath(file.fsName); for (var index = 0; index < app.documents.length; index++) { var candidate = app.documents[index]; try { if (normalizedPath(candidate.fullName.fsName) == expected) return candidate; } catch (ignored) {} } return null; }
  function findDirectLayerSet(container, name) { for (var index = 0; index < container.layerSets.length; index++) if (container.layerSets[index].name == name) return container.layerSets[index]; return null; }
  function requireDirectLayerSet(container, name) { var group = findDirectLayerSet(container, name); if (!group) throw new Error('Missing required group ' + name + ' in ' + container.name); return group; }
  function findDirectArtLayer(container, name) { for (var index = 0; index < container.artLayers.length; index++) if (container.artLayers[index].name == name) return container.artLayers[index]; return null; }
  function requireDirectArtLayer(container, name) { var layer = findDirectArtLayer(container, name); if (!layer) throw new Error('Missing required layer ' + name + ' in ' + container.name); return layer; }
  function pixels(value) { try { return Number(value.as('px')); } catch (ignored) { return Number(value); } }
  function localBounds(layer, origin) { var bounds = layer.bounds; return [Math.round(pixels(bounds[0]) - origin[0]), Math.round(pixels(bounds[1]) - origin[1]), Math.round(pixels(bounds[2]) - origin[0]), Math.round(pixels(bounds[3]) - origin[1])]; }
  function exportLayer(record, layer, origin) {
    var bounds = localBounds(layer, origin);
    var exportDocument = doc.duplicate('GameXXK_BackpackExport_' + record.name, true);
    app.activeDocument = exportDocument;
    var rect = [UnitValue(bounds[0], 'px'), UnitValue(bounds[1], 'px'), UnitValue(bounds[2], 'px'), UnitValue(bounds[3], 'px')];
    exportDocument.crop(rect);
    if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT;
    var options = new PNGSaveOptions();
    options.interlaced = false;
    exportDocument.saveAs(new File(record.out), options, true, Extension.LOWERCASE);
    exportDocument.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
    return bounds;
  }
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
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before backpack export');
    // Locate the backpack page by its English child groups so the JSX stays
    // ASCII-only (Photoshop reads JSX files with a legacy codepage, so any
    // CJK group name in the script would arrive garbled).
    function findTopLevelGroupByChild(childName) {
      for (var index = 0; index < doc.layerSets.length; index++) {
        var candidate = doc.layerSets[index];
        if (findDirectLayerSet(candidate, childName)) return candidate;
      }
      return null;
    }
    var page = findTopLevelGroupByChild('20_Tabs');
    if (!page) throw new Error('Backpack page (containing 20_Tabs) not found');
    var tabs = requireDirectLayerSet(page, '20_Tabs');
    var scrollbar = requireDirectLayerSet(page, '42_InventoryScrollbar');
    var results = [];
    for (var index = 0; index < exportRecords.length; index++) {
      var record = exportRecords[index];
      var parts = record.path.split('/');
      var layer = parts[0] === '20_Tabs' ? requireDirectArtLayer(tabs, parts[1]) : requireDirectArtLayer(scrollbar, parts[1]);
      var bounds = exportLayer(record, layer, ${JSON.stringify(PAGE_ORIGIN)});
      results.push({ name: record.name, path: record.out, bounds: bounds, size: [bounds[2] - bounds[0], bounds[3] - bounds[1]] });
    }
    writeUtf8(reportPath, stringifyJson({ status: 'PASS', exports: results }));
    if (openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES);
  } catch (error) {
    try { if (doc && openedByScript) doc.close(SaveOptions.DONOTSAVECHANGES); } catch (rollbackError) {}
    writeUtf8(reportPath + '.error.txt', String(error && error.message || error));
    throw error;
  } finally {
    app.preferences.rulerUnits = oldUnits;
    app.displayDialogs = oldDialogs;
  }
})();
`;

fs.writeFileSync(args.output, jsx, 'utf8');
fs.writeFileSync(args.report, JSON.stringify({ status: 'GENERATED', exports: exports.map((r) => ({ name: r.name, path: r.out })) }, null, 2), 'utf8');
console.log(JSON.stringify({ status: 'GENERATED', exports: exports.map((r) => ({ name: r.name, path: r.out })) }, null, 2));
