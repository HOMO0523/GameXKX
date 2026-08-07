import fs from 'node:fs';
import path from 'node:path';

function parseArguments(values) {
  const required = ['psd', 'output', 'report', 'portrait', 'export-dir'];
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
requireFile(args.portrait, 'hero card portrait');
fs.mkdirSync(path.dirname(args.output), { recursive: true });
fs.mkdirSync(path.dirname(args.report), { recursive: true });
fs.mkdirSync(args['export-dir'], { recursive: true });

// The deck page (18_主角背包_卡组页) is page 03 translated by (0, 3600).
// The nine deck cards sit in a 3x3 grid on the right side. Each frame is an
// art layer named card_frame_base_PSD057 (+ copies). The portrait fills the
// card face, preserving the lower strip for card name/cost.
const PAGE_18_ORIGIN = [6120, 3600];
const PAGE_NAME = '18_主角背包_卡组页';
const FRAME_PREFIX = 'card_frame_base_PSD057';
const PORTRAIT_SOURCE_SIZE = [171, 205];
const PORTRAIT_LAYER_NAME = 'card_portrait';
const MARGIN_X = 5;
const MARGIN_TOP = 6;
const BOTTOM_RESERVE = 30;
const CARD_NAMES = [
  '青锋一式', '鹤羽斩', '风身步',
  '碎岩击', '归元术', '横剑守势',
  '凝神吐纳', '观隙', '破云一闪',
];
const exports = [
  { name: '18_主角背包_卡组页', origin: PAGE_18_ORIGIN },
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
  var portraitPath = ${jsxString(args.portrait)};
  var reportPath = ${jsxString(args.report)};
  var exportRecords = ${JSON.stringify(exports)};
  var pageName = ${jsxString(PAGE_NAME)};
  var framePrefix = ${jsxString(FRAME_PREFIX)};
  var portraitLayerName = ${jsxString(PORTRAIT_LAYER_NAME)};
  var portraitSource = ${JSON.stringify(PORTRAIT_SOURCE_SIZE)};
  var marginX = ${MARGIN_X};
  var marginTop = ${MARGIN_TOP};
  var bottomReserve = ${BOTTOM_RESERVE};
  var cardNames = ${JSON.stringify(CARD_NAMES)};
  var doc = null;
  var openedByScript = false;
  var initialHistoryState = null;

  function normalizedPath(value) { return String(value || '').replace(/\\\\/g, '/').toLowerCase(); }
  function findOpenDocument(file) { var expected = normalizedPath(file.fsName); for (var index = 0; index < app.documents.length; index++) { var candidate = app.documents[index]; try { if (normalizedPath(candidate.fullName.fsName) == expected) return candidate; } catch (ignored) {} } return null; }
  function findDirectLayerSet(container, name) { for (var index = 0; index < container.layerSets.length; index++) if (container.layerSets[index].name == name) return container.layerSets[index]; return null; }
  function pixels(value) { try { return Number(value.as('px')); } catch (ignored) { return Number(value); } }
  function exportPage(record) { var exportDocument = doc.duplicate('GameXXK_CardPortraits_' + record.name, true); app.activeDocument = exportDocument; exportDocument.crop([UnitValue(record.origin[0], 'px'), UnitValue(record.origin[1], 'px'), UnitValue(record.origin[0] + 1920, 'px'), UnitValue(record.origin[1] + 1080, 'px')]); if (exportDocument.bitsPerChannel != BitsPerChannelType.EIGHT) exportDocument.bitsPerChannel = BitsPerChannelType.EIGHT; var options = new PNGSaveOptions(); options.interlaced = false; exportDocument.saveAs(new File(record.path), options, true, Extension.LOWERCASE); exportDocument.close(SaveOptions.DONOTSAVECHANGES); app.activeDocument = doc; }
  function writeUtf8(filePath, contents) { var file = new File(filePath); file.encoding = 'UTF8'; file.open('w'); file.write(contents); file.close(); }
  function escapeJsonString(value) { var slash = String.fromCharCode(92); var quote = String.fromCharCode(34); return String(value).split(slash).join(slash + slash).split(quote).join(slash + quote).split(String.fromCharCode(13)).join(slash + 'r').split(String.fromCharCode(10)).join(slash + 'n').split(String.fromCharCode(9)).join(slash + 't'); }
  function stringifyJson(value) { if (value === null) return 'null'; var valueType = typeof value; if (valueType == 'string') return '"' + escapeJsonString(value) + '"'; if (valueType == 'number') return isFinite(value) ? String(value) : 'null'; if (valueType == 'boolean') return value ? 'true' : 'false'; if (value instanceof Array) { var parts = []; for (var arrayIndex = 0; arrayIndex < value.length; arrayIndex++) parts.push(stringifyJson(value[arrayIndex])); return '[' + parts.join(',') + ']'; } var objectParts = []; for (var key in value) if (value.hasOwnProperty(key)) objectParts.push('"' + escapeJsonString(key) + '":' + stringifyJson(value[key])); return '{' + objectParts.join(',') + '}'; }

  function findFrames(page) {
    var frames = [];
    for (var i = 0; i < page.artLayers.length; ++i) {
      var layer = page.artLayers[i];
      if (String(layer.name).indexOf(framePrefix) === 0) frames.push(layer);
    }
    // Stable order: top-left to bottom-right by (top, left).
    frames.sort(function (a, b) {
      var ab = a.bounds, bb = b.bounds;
      var at = pixels(ab[1]), bt = pixels(bb[1]);
      if (Math.abs(at - bt) > 4) return at - bt;
      return pixels(ab[0]) - pixels(bb[0]);
    });
    return frames;
  }

  function placePortrait(frame, index) {
    var bounds = frame.bounds;
    var fx0 = pixels(bounds[0]), fy0 = pixels(bounds[1]);
    var fw = pixels(bounds[2]) - fx0, fh = pixels(bounds[3]) - fy0;
    var pw = fw - 2 * marginX;
    var ph = pw * (portraitSource[1] / portraitSource[0]);
    if (ph > fh - marginTop - bottomReserve) {
      ph = fh - marginTop - bottomReserve;
      pw = ph * (portraitSource[0] / portraitSource[1]);
    }
    var portraitDoc = app.open(new File(portraitPath));
    app.activeDocument = portraitDoc;
    portraitDoc.selection.selectAll();
    portraitDoc.selection.copy();
    portraitDoc.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
    doc.activeLayer = frame;
    var layer = doc.paste();
    layer.name = index === 0 ? portraitLayerName : (portraitLayerName + '_' + (index + 1));
    var placed = layer.bounds;
    var cw = pixels(placed[2]) - pixels(placed[0]);
    var ch = pixels(placed[3]) - pixels(placed[1]);
    layer.resize((pw / cw) * 100.0, (ph / ch) * 100.0, AnchorPosition.MIDDLECENTER);
    var resized = layer.bounds;
    var dx = (fx0 + marginX) - pixels(resized[0]);
    var dy = (fy0 + marginTop) - pixels(resized[1]);
    layer.translate(UnitValue(dx, 'px'), UnitValue(dy, 'px'));
    var finalBounds = layer.bounds;
    return {
      index: index,
      cardName: cardNames[index],
      frame: [fx0, fy0, fx0 + fw, fy0 + fh],
      portrait: [
        Math.round(pixels(finalBounds[0])), Math.round(pixels(finalBounds[1])),
        Math.round(pixels(finalBounds[2])), Math.round(pixels(finalBounds[3]))
      ],
      size: [Math.round(pw), Math.round(ph)],
      layerName: String(layer.name)
    };
  }

  try {
    app.preferences.rulerUnits = Units.PIXELS;
    app.displayDialogs = DialogModes.NO;
    var targetFile = new File(targetPsdPath);
    if (!targetFile.exists) throw new Error('Target PSD does not exist: ' + targetPsdPath);
    doc = findOpenDocument(targetFile);
    if (!doc) { doc = app.open(targetFile); openedByScript = true; }
    app.activeDocument = doc;
    if (!doc.saved) throw new Error('Target PSD has unsaved changes before card portrait placement');
    initialHistoryState = doc.activeHistoryState;
    var page = findDirectLayerSet(doc, pageName);
    if (!page) throw new Error('Missing page: ' + pageName);
    var frames = findFrames(page);
    if (frames.length !== 9) throw new Error('Expected 9 card frames on ' + pageName + ', found ' + frames.length);
    var placed = [];
    for (var i = 0; i < frames.length; ++i) placed.push(placePortrait(frames[i], i));
    for (var exportIndex = 0; exportIndex < exportRecords.length; exportIndex++) exportPage(exportRecords[exportIndex]);
    doc.save();
    var report = { status: 'PASS', page: pageName, frameCount: frames.length, portraitSource: portraitSource, placed: placed, exports: exportRecords };
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
