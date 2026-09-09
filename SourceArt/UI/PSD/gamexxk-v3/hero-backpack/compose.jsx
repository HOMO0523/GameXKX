#target photoshop
app.bringToFront();
(function () {
  var oldUnits = app.preferences.rulerUnits;
  var oldDialogs = app.displayDialogs;
  app.preferences.rulerUnits = Units.PIXELS;
  app.displayDialogs = DialogModes.NO;
  var imageLayers = [{"name":"approved_reference_hidden","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Reference/approved_hero_backpack_reference.png","x":0,"y":0,"width":1920,"height":1080,"group":"00_Reference","visible":false},{"name":"world_context","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/world_context.png","x":0,"y":0,"width":1920,"height":1080,"group":"10_WorldContext","visible":true},{"name":"screen_scrim","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/screen_scrim.png","x":0,"y":0,"width":1920,"height":1080,"group":"10_WorldContext","visible":true},{"name":"hero_backpack_panel","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/paper_panel.png","x":290,"y":150,"width":1560,"height":790,"group":"20_Shell","visible":true},{"name":"title_underline","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/title_underline.png","x":338,"y":242,"width":180,"height":12,"group":"20_Shell","visible":true},{"name":"content_divider","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/ink_divider.png","x":1110,"y":258,"width":5,"height":620,"group":"20_Shell","visible":true},{"name":"close_button","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/close_button.png","x":1760,"y":178,"width":54,"height":54,"group":"20_Shell","visible":true},{"name":"hero_ink_shadow","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/hero_ink_shadow.png","x":458,"y":704,"width":500,"height":90,"group":"30_Hero","visible":true},{"name":"hero_runtime_idle_frame","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/hero_runtime_idle_frame_0000.png","x":455,"y":276,"width":490,"height":490,"group":"30_Hero","visible":true},{"name":"hero_progress_track","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/progress_track.png","x":520,"y":782,"width":360,"height":18,"group":"30_Hero","visible":true},{"name":"hero_progress_fill","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/progress_fill.png","x":525,"y":787,"width":238,"height":8,"group":"30_Hero","visible":true},{"name":"equipment_slot_01","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":342,"y":318,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_01","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_1.png","x":359,"y":336,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"equipment_slot_02","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":342,"y":480,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_02","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_2.png","x":359,"y":498,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"equipment_slot_03","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":342,"y":642,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_03","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_3.png","x":359,"y":660,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"equipment_slot_04","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":962,"y":318,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_04","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_4.png","x":979,"y":336,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"equipment_slot_05","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":962,"y":480,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_05","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_5.png","x":979,"y":498,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"equipment_slot_06","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_slot.png","x":962,"y":642,"width":112,"height":118,"group":"40_Equipment","visible":true},{"name":"equipment_icon_06","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/equipment_6.png","x":979,"y":660,"width":78,"height":82,"group":"40_Equipment","visible":true},{"name":"inventory_category_tab_01","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/tab_selected.png","x":1148,"y":246,"width":116,"height":43,"group":"50_Inventory","visible":true},{"name":"inventory_category_tab_02","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/tab_normal.png","x":1273,"y":246,"width":116,"height":43,"group":"50_Inventory","visible":true},{"name":"inventory_category_tab_03","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/tab_normal.png","x":1398,"y":246,"width":116,"height":43,"group":"50_Inventory","visible":true},{"name":"inventory_category_tab_04","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/tab_normal.png","x":1523,"y":246,"width":116,"height":43,"group":"50_Inventory","visible":true},{"name":"inventory_category_tab_05","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/tab_normal.png","x":1648,"y":246,"width":116,"height":43,"group":"50_Inventory","visible":true},{"name":"inventory_slot_01","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1155,"y":314,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_02","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1267,"y":314,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_03","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1379,"y":314,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_04","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1491,"y":314,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_05","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1603,"y":314,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_06","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1155,"y":416,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_07","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1267,"y":416,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_08","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1379,"y":416,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_09","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1491,"y":416,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_10","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1603,"y":416,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_11","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1155,"y":518,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_12","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1267,"y":518,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_13","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1379,"y":518,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_14","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1491,"y":518,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_15","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1603,"y":518,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_16","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1155,"y":620,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_17","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1267,"y":620,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_18","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1379,"y":620,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_19","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1491,"y":620,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_slot_20","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_slot.png","x":1603,"y":620,"width":92,"height":92,"group":"50_Inventory","visible":true},{"name":"inventory_item_01","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_1.png","x":1170,"y":328,"width":62,"height":64,"group":"50_Inventory","visible":true},{"name":"inventory_item_02","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_2.png","x":1282,"y":328,"width":62,"height":64,"group":"50_Inventory","visible":true},{"name":"inventory_item_03","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_3.png","x":1394,"y":328,"width":62,"height":64,"group":"50_Inventory","visible":true},{"name":"inventory_item_04","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_4.png","x":1506,"y":328,"width":62,"height":64,"group":"50_Inventory","visible":true},{"name":"inventory_item_05","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_5.png","x":1618,"y":328,"width":62,"height":64,"group":"50_Inventory","visible":true},{"name":"item_detail_panel","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/item_detail_panel.png","x":1148,"y":735,"width":620,"height":120,"group":"60_Detail","visible":true},{"name":"button_sort","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/button_neutral.png","x":1270,"y":868,"width":150,"height":54,"group":"60_Detail","visible":true},{"name":"button_use","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/button_primary.png","x":1605,"y":770,"width":150,"height":54,"group":"60_Detail","visible":true},{"name":"button_disassemble","path":"D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/button_destructive.png","x":1450,"y":868,"width":150,"height":54,"group":"60_Detail","visible":true}];
  var textLayers = [{"name":"title","text":"主角","x":340,"y":178,"fontSize":44,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"tab_attribute","text":"属性","x":514,"y":184,"fontSize":25,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"tab_equipment","text":"装备","x":645,"y":184,"fontSize":25,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"tab_skill","text":"技能","x":776,"y":184,"fontSize":25,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"tab_talent","text":"天赋","x":907,"y":184,"fontSize":25,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"tab_title","text":"称号","x":1038,"y":184,"fontSize":25,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"hero_name","text":"小侠客","x":620,"y":250,"fontSize":29,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"hero_level","text":"Lv. 1","x":470,"y":818,"fontSize":24,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"hero_exp","text":"0 / 100","x":785,"y":818,"fontSize":19,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"hero_attack","text":"攻击 33","x":484,"y":860,"fontSize":22,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"hero_health","text":"气血 120","x":650,"y":860,"fontSize":22,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"hero_defense","text":"防御 18","x":830,"y":860,"fontSize":22,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"category_all","text":"全部","x":1206,"y":253,"fontSize":20,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"category_consumable","text":"消耗","x":1330,"y":253,"fontSize":20,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"category_material","text":"材料","x":1455,"y":253,"fontSize":20,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"category_quest","text":"任务","x":1580,"y":253,"fontSize":20,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"category_other","text":"其他","x":1705,"y":253,"fontSize":20,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"inventory_capacity","text":"背包  5 / 80","x":1150,"y":188,"fontSize":26,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"resource_coin","text":"铜钱 50","x":1530,"y":192,"fontSize":22,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"detail_name","text":"小布袋","x":1248,"y":760,"fontSize":27,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"detail_count","text":"拥有：2","x":1480,"y":764,"fontSize":19,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"detail_description","text":"普通的布袋，能装下一些小物件。","x":1248,"y":806,"fontSize":19,"font":"STKaiti","color":"#2a2822","bold":false,"group":"70_RuntimeText"},{"name":"button_sort_text","text":"整理","x":1320,"y":879,"fontSize":21,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"button_use_text","text":"使用","x":1655,"y":781,"fontSize":21,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"button_disassemble_text","text":"分解","x":1500,"y":879,"fontSize":21,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"},{"name":"close_text","text":"×","x":1777,"y":182,"fontSize":30,"font":"STKaiti","color":"#2a2822","bold":true,"group":"70_RuntimeText"}];
  var spec = {"name":"GameXXK_HeroBackpack_V1","width":1920,"height":1080,"resolution":72,"scale":1,"outputPsd":"D:/UE5 demo/GameXXK/outputs/UI_PSD/Candidates/GameXXK_HeroBackpack_V1.psd"};

  function colorFromHex(hex) {
    var c = new SolidColor();
    c.rgb.red = parseInt(hex.substr(1, 2), 16);
    c.rgb.green = parseInt(hex.substr(3, 2), 16);
    c.rgb.blue = parseInt(hex.substr(5, 2), 16);
    return c;
  }

  function setFont(textItem, preferred) {
    var candidates = [preferred, 'STKaiti', 'KaiTi', 'MicrosoftYaHei', 'SimSun', 'ArialMT'];
    for (var i = 0; i < candidates.length; i++) {
      try {
        textItem.font = candidates[i];
        return candidates[i];
      } catch (e) {}
    }
    return '';
  }

  function ensureGroup(doc, name) {
    for (var groupIndex = 0; groupIndex < doc.layerSets.length; groupIndex++) {
      if (doc.layerSets[groupIndex].name == name) return doc.layerSets[groupIndex];
    }
    var group = doc.layerSets.add();
    group.name = name;
    return group;
  }

  function resizeLayerTo(layer, width, height) {
    if (!(width > 0) || !(height > 0)) return;
    var bounds = layer.bounds;
    var currentWidth = bounds[2].as('px') - bounds[0].as('px');
    var currentHeight = bounds[3].as('px') - bounds[1].as('px');
    if (!(currentWidth > 0) || !(currentHeight > 0)) return;
    layer.resize(width / currentWidth * 100, height / currentHeight * 100, AnchorPosition.TOPLEFT);
  }

  function importImage(doc, item) {
    var sourceFile = new File(item.path);
    if (!sourceFile.exists) throw new Error('Missing image: ' + item.path);
    var source = app.open(sourceFile);
    var sourceLayer = source.activeLayer;
    var duplicated = sourceLayer.duplicate(doc, ElementPlacement.PLACEATBEGINNING);
    source.close(SaveOptions.DONOTSAVECHANGES);
    app.activeDocument = doc;
    doc.activeLayer = duplicated;
    duplicated.name = item.name;
    if (item.width > 0 && item.height > 0) {
      try {
        resizeLayerTo(duplicated, item.width, item.height);
      } catch (resizeError) {
        throw new Error('Failed to resize layer ' + item.name + ': ' + resizeError.message);
      }
    }
    if (item.group) duplicated.move(ensureGroup(doc, item.group), ElementPlacement.INSIDE);
    var bounds = duplicated.bounds;
    var currentLeft = bounds[0].as('px');
    var currentTop = bounds[1].as('px');
    duplicated.translate(UnitValue(item.x - currentLeft, 'px'), UnitValue(item.y - currentTop, 'px'));
    duplicated.visible = item.visible !== false;
    return duplicated;
  }

  for (var cleanupIndex = app.documents.length - 1; cleanupIndex >= 0; cleanupIndex--) {
    var cleanupDoc = app.documents[cleanupIndex];
    if (cleanupDoc.name == spec.name || cleanupDoc.name == 'background.png') {
      cleanupDoc.close(SaveOptions.DONOTSAVECHANGES);
    }
  }

  var doc = app.documents.add(
    spec.width,
    spec.height,
    spec.resolution,
    spec.name,
    NewDocumentMode.RGB,
    DocumentFill.TRANSPARENT,
    1,
    BitsPerChannelType.EIGHT
  );

  for (var i = 0; i < imageLayers.length; i++) {
    importImage(doc, imageLayers[i]);
  }

  var createdText = [];
  var documentScale = spec.scale || 1;
  for (var j = 0; j < textLayers.length; j++) {
    var item = textLayers[j];
    var layer = doc.artLayers.add();
    layer.kind = LayerKind.TEXT;
    layer.name = item.name;
    var textItem = layer.textItem;
    textItem.kind = TextType.POINTTEXT;
    textItem.contents = item.text;
    if (item.justify == 'center') textItem.justification = Justification.CENTER;
    else if (item.justify == 'right') textItem.justification = Justification.RIGHT;
    else textItem.justification = Justification.LEFT;
    textItem.position = [UnitValue(item.x * documentScale, 'px'), UnitValue((item.y + item.fontSize) * documentScale, 'px')];
    textItem.size = UnitValue(item.fontSize * documentScale, 'pt');
    textItem.color = colorFromHex(item.color);
    textItem.antiAliasMethod = AntiAlias.SHARP;
    setFont(textItem, item.font);
    try { textItem.fauxBold = !!item.bold; } catch (e) {}
    try { textItem.tracking = item.tracking || 0; } catch (e) {}
    if (item.group) layer.move(ensureGroup(doc, item.group), ElementPlacement.INSIDE);
    createdText.push(textItem.contents);
  }

  var outputFile = new File(spec.outputPsd);
  if (!outputFile.parent.exists) outputFile.parent.create();
  var psdOptions = new PhotoshopSaveOptions();
  psdOptions.layers = true;
  psdOptions.alphaChannels = true;
  psdOptions.annotations = false;
  psdOptions.embedColorProfile = true;
  psdOptions.spotColors = true;
  doc.saveAs(outputFile, psdOptions, true, Extension.LOWERCASE);

  var previewDoc = doc.duplicate('preview_temp', true);
  previewDoc.flatten();
  var pngOptions = new PNGSaveOptions();
  pngOptions.interlaced = false;
  previewDoc.saveAs(new File("D:/UE5 demo/GameXXK/SourceArt/UI/PSD/gamexxk-v3/hero-backpack/final_preview.png"), pngOptions, true, Extension.LOWERCASE);
  previewDoc.close(SaveOptions.DONOTSAVECHANGES);

  doc.close(SaveOptions.DONOTSAVECHANGES);
  var reopened = app.open(outputFile);
  var textCount = 0;
  var actualTexts = [];
  var walkedArtLayerCount = 0;
  function walkLayers(container, actualTexts) {
    for (var artIndex = 0; artIndex < container.artLayers.length; artIndex++) {
      var art = container.artLayers[artIndex];
      walkedArtLayerCount++;
      if (art.kind == LayerKind.TEXT) {
        textCount++;
        actualTexts.push(art.textItem.contents);
      }
    }
    for (var setIndex = 0; setIndex < container.layerSets.length; setIndex++) {
      walkLayers(container.layerSets[setIndex], actualTexts);
    }
  }
  walkLayers(reopened, actualTexts);
  var validation = {
    width: reopened.width.as('px'),
    height: reopened.height.as('px'),
    artLayerCount: walkedArtLayerCount,
    expectedImageLayers: imageLayers.length,
    expectedTextLayers: textLayers.length,
    actualTextLayers: textCount,
    textRoundTripMatch: actualTexts.length == createdText.length,
    outputPsd: outputFile.fsName
  };
  var expectedSorted = createdText.slice(0).sort();
  var actualSorted = actualTexts.slice(0).sort();
  var roundTripMatch = actualSorted.length == expectedSorted.length;
  if (roundTripMatch) {
    for (var matchIndex = 0; matchIndex < actualSorted.length; matchIndex++) {
      if (actualSorted[matchIndex] != expectedSorted[matchIndex]) {
        roundTripMatch = false;
        break;
      }
    }
  }
  validation.textRoundTripMatch = roundTripMatch;
  var q = String.fromCharCode(34);
  var nl = String.fromCharCode(10);
  var validationJson = '{' + nl +
    '  ' + q + 'width' + q + ': ' + validation.width + ',' + nl +
    '  ' + q + 'height' + q + ': ' + validation.height + ',' + nl +
    '  ' + q + 'artLayerCount' + q + ': ' + validation.artLayerCount + ',' + nl +
    '  ' + q + 'expectedImageLayers' + q + ': ' + validation.expectedImageLayers + ',' + nl +
    '  ' + q + 'expectedTextLayers' + q + ': ' + validation.expectedTextLayers + ',' + nl +
    '  ' + q + 'actualTextLayers' + q + ': ' + validation.actualTextLayers + ',' + nl +
    '  ' + q + 'textRoundTripMatch' + q + ': ' + (validation.textRoundTripMatch ? 'true' : 'false') + ',' + nl +
    '  ' + q + 'outputPsd' + q + ': ' + q + spec.outputPsd + q + nl +
    '}';
  var validationPath = String(spec.outputPsd).replace(/\.psd$/i, '.validation.json');
  var validationFile = new File(validationPath);
  if (!validationFile.parent.exists) validationFile.parent.create();
  validationFile.encoding = 'UTF8';
  validationFile.open('w');
  validationFile.write(validationJson);
  validationFile.close();
  reopened.close(SaveOptions.DONOTSAVECHANGES);
  app.preferences.rulerUnits = oldUnits;
  app.displayDialogs = oldDialogs;
})();
