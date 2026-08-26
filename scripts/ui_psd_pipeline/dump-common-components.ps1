param(
    [string]$PsdPath = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psd = [IO.Path]::GetFullPath((Join-Path $projectRoot $PsdPath))
$outFile = [IO.Path]::GetFullPath((Join-Path $projectRoot 'Saved/VisualReview/common-components-tree-20260813.json'))
$psdJs = $psd.Replace('\', '/').Replace('"', '\"')
$outJs = $outFile.Replace('\', '/').Replace('"', '\"')
$jsx = @'
#target photoshop
(function () {
  var psdPath = "__PSDPATH__";
  var outPath = "__OUTPATH__";
  app.displayDialogs = DialogModes.NO;
  function absolutePath(value) { try { return String(value.fsName).replace(/\\/g, "/").toLowerCase(); } catch (e) { return ""; } }
  function boundsOf(layer) { var b = layer.bounds; return [b[0].as("px"), b[1].as("px"), b[2].as("px"), b[3].as("px")]; }
  function jsonStringify(value) {
    if (value === null) return "null";
    var t = typeof value;
    if (t == 'string') return '"' + String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\r/g, '\\r').replace(/\n/g, '\\n') + '"';
    if (t == 'number') return isFinite(value) ? String(value) : "null";
    if (t == 'boolean') return value ? "true" : "false";
    if (value instanceof Array) { var it = []; for (var i = 0; i < value.length; ++i) it.push(jsonStringify(value[i])); return "[" + it.join(",") + "]"; }
    var f = [];
    for (var k in value) {
      if (!value.hasOwnProperty(k) || typeof value[k] == "undefined" || typeof value[k] == "function") continue;
      f.push(jsonStringify(String(k)) + ":" + jsonStringify(value[k]));
    }
    return "{" + f.join(",") + "}";
  }
  function rec(layer, prefix) {
    var path = prefix ? prefix + "/" + layer.name : layer.name;
    var r = { name: String(layer.name), path: String(path), kind: String(layer.kind), visible: !!layer.visible, bounds: boundsOf(layer) };
    if (layer.kind == LayerKind.TEXT && layer.textItem) { try { r.text = String(layer.textItem.contents); r.fontSize = Number(layer.textItem.size); } catch (e) { r.text = ""; } }
    return r;
  }
  function walk(group, prefix, records) {
    for (var i = 0; i < group.artLayers.length; ++i) records.push(rec(group.artLayers[i], prefix));
    for (var j = 0; j < group.layerSets.length; ++j) {
      var child = group.layerSets[j];
      var childPath = prefix ? prefix + "/" + child.name : child.name;
      records.push(rec(child, prefix));
      walk(child, childPath, records);
    }
  }
  var doc = null;
  try {
    for (var i = 0; i < app.documents.length; ++i) {
      var c = app.documents[i];
      try { if (absolutePath(c.fullName) == absolutePath(new File(psdPath))) { doc = c; break; } } catch (e) {}
    }
    if (!doc) doc = app.open(new File(psdPath));
    app.activeDocument = doc;
    var common = null;
    for (var s = 0; s < doc.layerSets.length; ++s) {
      if (String(doc.layerSets[s].name).indexOf(String.fromCharCode(20844, 20849)) >= 0) { common = doc.layerSets[s]; break; }
    }
    var records = [];
    if (common) { records.push(rec(common, "")); walk(common, String(common.name), records); }
    var f = new File(outPath); f.encoding = "UTF8"; f.open("w"); f.write(jsonStringify({ status: "PASS", found: !!common, records: records })); f.close();
    doc.close(SaveOptions.DONOTSAVECHANGES);
  } catch (e) {
    var f2 = new File(outPath + ".error.txt"); f2.encoding = "UTF8"; f2.open("w"); f2.write(String(e)); f2.close();
    throw e;
  }
}());

'@
$jsx = $jsx.Replace('__PSDPATH__', $psdJs).Replace('__OUTPATH__', $outJs)
$photoshop = New-Object -ComObject Photoshop.Application
$null = $photoshop.DoJavaScript($jsx)
try { $photoshop.DoJavaScript('if (app.documents.length > 0) { for (var i = app.documents.length - 1; i >= 0; i--) { try { app.documents[i].close(SaveOptions.DONOTSAVECHANGES); } catch (e) {} } }') } catch {}
try { $photoshop.Quit() } catch {}
Write-Output 'Common components dump: PASS (PS closed)'
