const fs = require('fs');
const vm = require('vm');
const assert = require('assert/strict');
const path = 'docs/design/2026-09-03-all-card-text-review/';
const data = JSON.parse(fs.readFileSync(path+'card-texts.json','utf8'));
const docEvents = new Map();
const winEvents = new Map();
const label = {textContent:''};
const content = {innerHTML:'',scrollTop:0};
const box = {isConnected:true,dataset:{cardId:data.cards[0].id,quality:data.cards[0].variants[0].quality},
  querySelector:s=>s==='.interaction-content'?content:label,
  contains:e=>e===box,
  closest:s=>s==='.interactive-preview'?box:null};
const sandbox = {DATA:data,esc:String,copy:String,
  document:{addEventListener:(name,fn)=>docEvents.set(name,fn)},
  window:{addEventListener:(name,fn)=>winEvents.set(name,fn)}};
vm.runInNewContext(fs.readFileSync(path+'_authoring/card_text_review_interaction.js','utf8'),sandbox);
function fire(name,props={}) {let prevented=false;docEvents.get(name)?.({target:box,shiftKey:false,relatedTarget:null,preventDefault(){prevented=true},...props});return prevented;}
fire('pointerover'); assert.equal(box.dataset.mode,'compact');
assert.equal(fire('contextmenu'),true); assert.equal(box.dataset.mode,'pills');
fire('pointerup',{button:2}); assert.equal(box.dataset.mode,'pills');
fire('mouseup',{button:2}); assert.equal(box.dataset.mode,'pills');
fire('keydown',{key:'Shift'}); assert.equal(box.dataset.mode,'detail');
fire('keyup',{key:'Shift'}); assert.equal(box.dataset.mode,'pills');
fire('contextmenu'); assert.equal(box.dataset.mode,'compact');
fire('contextmenu'); fire('keydown',{key:'Escape'}); assert.equal(box.dataset.mode,'compact');
fire('contextmenu'); fire('pointerout'); assert.equal(box.dataset.mode,'compact');
fire('pointerover'); fire('contextmenu'); winEvents.get('blur')(); assert.equal(box.dataset.mode,'compact');
console.log('Interaction state checks passed: right-click toggle, mouse release persistence, Shift restore, Escape, leave, blur.');
const html = fs.readFileSync(path+'review.html','utf8');
fs.writeFileSync('Saved/HarnessReports/card_tooltip_review_complete.js',html.split('<script>')[1].split('</script>')[0]);
