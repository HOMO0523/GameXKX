param(
    [string]$PsdPath = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$ExportRoot = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/RedesignPages'
)

# Read-only extended export: full layer trees of the six redesign pages
# (06/10/11/12/17 + confirmed component sources 02/03/07/18) with text
# contents, fonts, colors, visibility and bounds. Never mutates the PSD.

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
    var record = {name: String(layer.name), path: String(path), kind: kind, visible: !!layer.visible, bounds: boundsOf(layer)};
    record.layerKind = String(layer.kind);
    if (layer.kind == LayerKind.TEXT && layer.textItem) {
      try {
        record.text = String(layer.textItem.contents);
        record.fontSize = layer.textItem.size ? Number(layer.textItem.size) : 0;
        record.font = String(layer.textItem.font);
        record.color = layer.textItem.color ? String(layer.textItem.color.rgb.hexValue) : '';
      } catch (textError) {
        record.text = '';
      }
    }
    return record;
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
    var exportDoc = doc.duplicate('GameXXK_Redesign_' + spec.name, true);
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

    var requiredNames = {
      '02_城镇HUD': true, '03_主角背包': true, '06_任务日志': true, '07_商店交易': true,
      '10_战斗HUD': true, '11_战斗奖励结算': true, '12_系统菜单': true,
      '13_主角背包_物品选中': true, '17_战斗HUD_卡牌选中目标': true, '18_主角背包_卡组页': true
    };
    var pageLayers = {};
    for (var p = 0; p < doc.layerSets.length; ++p) {
      var page = doc.layerSets[p];
      if (!requiredNames[String(page.name)]) continue;
      var records = [layerRecord(page, '', 'group')];
      walk(page, String(page.name), records);
      pageLayers[String(page.name)] = records;
    }

    var specs = [
      {name: '06_任务日志', file: '06_任务日志.png', x: 2040, y: 1200},
      {name: '10_战斗HUD', file: '10_战斗HUD.png', x: 0, y: 2400},
      {name: '11_战斗奖励结算', file: '11_战斗奖励结算.png', x: 2040, y: 2400},
      {name: '12_系统菜单', file: '12_系统菜单.png', x: 4080, y: 2400},
      {name: '17_战斗HUD_卡牌选中目标', file: '17_战斗HUD_选中目标.png', x: 4080, y: 3600}
    ];
    var exports = [];
    for (var e = 0; e < specs.length; ++e) exports.push(exportCrop(specs[e]));
    return jsonStringify({
      status: 'PASS', psdPath: psdPath, documentSaved: !!doc.saved,
      width: doc.width.as('px'), height: doc.height.as('px'),
      topLevelLayers: top, pageLayers: pageLayers, exports: exports
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
}
$reportPath = Join-Path $output 'redesign-pages.json'
$report | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $reportPath -Encoding UTF8
Write-Output "Redesign pages read-only export: PASS"
Write-Output "Output: $output"
Write-Output "Report: $reportPath"
