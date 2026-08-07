param(
    [string]$PsdPath = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$ExportRoot = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/CurrentMasterV1'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psd = [IO.Path]::GetFullPath((Join-Path $projectRoot $PsdPath))
$stamp = [DateTimeOffset]::Now.ToString('yyyyMMdd-HHmmss')
$output = [IO.Path]::GetFullPath((Join-Path $projectRoot (Join-Path $ExportRoot $stamp)))
[IO.Directory]::CreateDirectory($output) | Out-Null

function Convert-ToJsString([string]$Value) {
    return '"' + $Value.Replace('\', '/').Replace('"', '\"') + '"'
}

$psdJs = Convert-ToJsString $psd
$outputJs = Convert-ToJsString $output
$jsx = @"
#target photoshop
(function () {
  var psdPath = $psdJs;
  var outputPath = $outputJs;
  var originalDialogs = app.displayDialogs;
  app.displayDialogs = DialogModes.NO;
  var doc = null;
  var openedForExport = false;

  function absolutePath(value) {
    try { return String(value.fsName).replace(/\\/g, '/').toLowerCase(); }
    catch (error) { return ''; }
  }
  function boundsOf(layer) {
    var b = layer.bounds;
    return [b[0].as('px'), b[1].as('px'), b[2].as('px'), b[3].as('px')];
  }
  function layerRecord(layer, prefix, kind) {
    var path = prefix ? prefix + '/' + layer.name : layer.name;
    return {name: String(layer.name), path: String(path), kind: kind, visible: !!layer.visible, bounds: boundsOf(layer)};
  }
  function jsonStringify(value) {
    if (value === null) return 'null';
    var type = typeof value;
    if (type == 'string') return '"' + value.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n') + '"';
    if (type == 'number') return isFinite(value) ? String(value) : 'null';
    if (type == 'boolean') return value ? 'true' : 'false';
    if (value instanceof Array) {
      var items = [];
      for (var i = 0; i < value.length; ++i) items.push(jsonStringify(value[i]));
      return '[' + items.join(',') + ']';
    }
    var fields = [];
    for (var key in value) {
      if (!value.hasOwnProperty(key) || typeof value[key] == 'undefined' || typeof value[key] == 'function') continue;
      fields.push(jsonStringify(String(key)) + ':' + jsonStringify(value[key]));
    }
    return '{' + fields.join(',') + '}';
  }
  function walk(group, prefix, records) {
    for (var i = 0; i < group.artLayers.length; ++i) records.push(layerRecord(group.artLayers[i], prefix, 'art'));
    for (var j = 0; j < group.layerSets.length; ++j) {
      var child = group.layerSets[j];
      var childPath = prefix ? prefix + '/' + child.name : child.name;
      records.push(layerRecord(child, prefix, 'group'));
      walk(child, childPath, records);
    }
  }
  function exportCrop(spec) {
    var exportDoc = doc.duplicate('GameXXK_CurrentMaster_' + spec.name, true);
    app.activeDocument = exportDoc;
    exportDoc.crop([
      UnitValue(spec.x, 'px'), UnitValue(spec.y, 'px'),
      UnitValue(spec.x + 1920, 'px'), UnitValue(spec.y + 1080, 'px')
    ]);
    if (exportDoc.bitsPerChannel != BitsPerChannelType.EIGHT) exportDoc.bitsPerChannel = BitsPerChannelType.EIGHT;
    var options = new PNGSaveOptions();
    options.interlaced = false;
    var file = new File(outputPath + '/' + spec.file);
    exportDoc.saveAs(file, options, true, Extension.LOWERCASE);
    var result = {name: spec.name, file: absolutePath(file), origin: [spec.x, spec.y], width: 1920, height: 1080};
    exportDoc.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
    return result;
  }

  try {
    for (var i = 0; i < app.documents.length; ++i) {
      var candidate = app.documents[i];
      try {
        if (absolutePath(candidate.fullName) == absolutePath(new File(psdPath))) { doc = candidate; break; }
      } catch (ignore) {}
    }
    if (!doc) { doc = app.open(new File(psdPath)); openedForExport = true; }
    app.activeDocument = doc;

    var top = [];
    for (var a = 0; a < doc.artLayers.length; ++a) top.push(layerRecord(doc.artLayers[a], '', 'art'));
    for (var s = 0; s < doc.layerSets.length; ++s) top.push(layerRecord(doc.layerSets[s], '', 'group'));

    var requiredNames = {'02_城镇HUD': true, '03_主角背包': true, '04_伙伴编队': true, '07_商店交易': true, '18_主角背包_卡组页': true};
    var pageLayers = {};
    for (var p = 0; p < doc.layerSets.length; ++p) {
      var page = doc.layerSets[p];
      if (!requiredNames[String(page.name)]) continue;
      var records = [layerRecord(page, '', 'group')];
      walk(page, String(page.name), records);
      pageLayers[String(page.name)] = records;
    }

    var specs = [
      {name: '02_城镇HUD', file: '02_城镇HUD.png', x: 4080, y: 0},
      {name: '03_主角背包', file: '03_主角背包.png', x: 6120, y: 0},
      {name: '04_伙伴编队', file: '04_伙伴编队.png', x: 8160, y: 0},
      {name: '07_商店交易', file: '07_商店交易.png', x: 4080, y: 1200},
      {name: '18_主角背包_卡组页', file: '18_主角背包_卡组页.png', x: 6120, y: 3600}
    ];
    var exports = [];
    for (var e = 0; e < specs.length; ++e) exports.push(exportCrop(specs[e]));
    return jsonStringify({
      status: 'PASS', psdPath: psdPath, documentSaved: !!doc.saved,
      width: doc.width.as('px'), height: doc.height.as('px'),
      topLevelLayers: top, relevantPageLayers: pageLayers, exports: exports
    });
  } finally {
    if (openedForExport && doc) doc.close(SaveOptions.DONOTSAVECHANGES);
    app.displayDialogs = originalDialogs;
  }
}());
"@

$photoshop = New-Object -ComObject Photoshop.Application
$json = [string]$photoshop.DoJavaScript($jsx)
$report = $json | ConvertFrom-Json -Depth 100
$report | Add-Member -NotePropertyName psdSha256 -NotePropertyValue ((Get-FileHash -Algorithm SHA256 -LiteralPath $psd).Hash.ToLowerInvariant())
foreach ($export in $report.exports) {
    $localPath = Join-Path $output ([IO.Path]::GetFileName([string]$export.file))
    $export.file = $localPath
    $export | Add-Member -NotePropertyName sha256 -NotePropertyValue ((Get-FileHash -Algorithm SHA256 -LiteralPath $localPath).Hash.ToLowerInvariant())
}
$reportPath = Join-Path $output 'master-v1-current-pages.json'
$report | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output "Current Master V1 read-only export: PASS"
Write-Output "PSD SHA256: $($report.psdSha256)"
Write-Output "Output: $output"
Write-Output "Report: $reportPath"
