const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{pathToFileURL}=require('node:url');
const {chromium}=require('C:/Users/shxuw/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const root=path.resolve(__dirname,'..'),base=path.join(root,'Deliverables/音效需求'),out=process.env.GAMEXXK_SFX_REVIEW_OUT||path.join(root,'Saved/Codex/EssentialSfxRequirements-20260914/browser-review');
fs.mkdirSync(out,{recursive:true});const entries=JSON.parse(fs.readFileSync(path.join(base,'目录清单.json'),'utf8')).packages;
const fixture=path.join(root,'Saved/Codex/ToolSfxHtmlReview-20260914/candidate_24bit.wav'),fixtureHash=crypto.createHash('sha256').update(fs.readFileSync(fixture)).digest('hex');
const report={packages:[],errors:[],externalRequests:[]};
async function run(){const browser=await chromium.launch({channel:'chrome',headless:true});try{
 const selected=process.argv[2]==='extra'?entries.filter(e=>e.number>=16):process.argv[2]?entries.filter(e=>e.cue===process.argv[2]):entries;assert.ok(selected.length);
 for(const entry of selected){const context=await browser.newContext({viewport:{width:1440,height:1000},acceptDownloads:true});const page=await context.newPage();
  page.on('pageerror',e=>report.errors.push({cue:entry.cue,error:e.message}));page.on('request',r=>{if(/^https?:/.test(r.url()))report.externalRequests.push(r.url());});
  await page.addInitScript(()=>{window.previewStarts=[];const start=AudioBufferSourceNode.prototype.start;AudioBufferSourceNode.prototype.start=function(...args){window.previewStarts.push({buffer:this.buffer,offset:args[1]||0,time:document.querySelector('video')?.currentTime||0});return start.apply(this,args);};});
  const file=path.join(base,entry.html);await page.goto(pathToFileURL(file).href);await page.waitForFunction(()=>!document.getElementById('play').disabled);
  assert.match(await page.locator('h1').textContent(),new RegExp(entry.title));assert.ok(Math.abs(await page.locator('video').evaluate(v=>v.duration)-entry.seconds)<.04);
  const meta=await page.evaluate(()=>{const d=JSON.parse(document.getElementById('reviewPayload').textContent);return {targets:d.scenes.filter(s=>s.tool).map(s=>s.targetFrame),fps:d.video.fps,last:d.video.frame_count-1,review:d.review};});
  assert.equal(meta.review,null);assert.equal(meta.targets.length,entry.primary_events);assert.equal(await page.locator('#seek').getAttribute('max'),String(meta.last));
  await page.getByRole('button',{name:`定位第 ${meta.targets[0]} 帧`,exact:true}).click();
  await page.waitForFunction(f=>document.getElementById('clock').textContent.includes('F'+String(f).padStart(4,'0')),meta.targets[0]);
  await page.locator('[data-play-name]:visible').first().click();await page.waitForFunction(()=>window.previewStarts.length>0);
  await page.locator('#fileInput').setInputFiles(fixture);await page.waitForFunction(()=>document.getElementById('candidateBadge').textContent==='已导入'&&!document.getElementById('exportReview').disabled);
  await page.locator('#notes').fill(`${entry.title}：检查尾音与触发位置。`);await page.locator('#version').fill('review-test');
  await page.locator('#seek').fill('0');await page.locator('#play').click();
  await page.waitForFunction(duration=>window.previewStarts.some(n=>Math.abs(n.buffer?.duration-duration)<.04),entry.seconds);
  assert.equal(await page.locator('video').evaluate(v=>v.muted),true);
  const peaks=await page.evaluate(({duration,frames,fps})=>{const n=window.previewStarts.find(n=>Math.abs(n.buffer?.duration-duration)<.04),data=n.buffer.getChannelData(0),rate=n.buffer.sampleRate;return frames.map(f=>{let peak=0;for(let i=Math.round(f/fps*rate);i<Math.min(data.length,Math.round((f/fps+.2)*rate));i++)peak=Math.max(peak,Math.abs(data[i]));return peak;});},{duration:entry.seconds,frames:meta.targets,fps:meta.fps});
  assert.ok(peaks.every(p=>p>.05),`${entry.cue} preview cues are silent`);
  await page.locator('#play').click();
  const download=page.waitForEvent('download');await page.locator('#exportReview').click();const d=await download;const exported=path.join(out,entry.cue+'-review.html');await d.saveAs(exported);
  const content=fs.readFileSync(exported,'utf8'),payload=JSON.parse(content.match(/<script id="reviewPayload" type="application\/json">([\s\S]*?)<\/script>/)[1]);
  assert.equal(crypto.createHash('sha256').update(Buffer.from(payload.review.candidate.base64,'base64')).digest('hex'),fixtureHash);
  await page.goto(pathToFileURL(exported).href);await page.waitForFunction(()=>document.getElementById('candidateBadge').textContent==='已导入');
  assert.equal(await page.locator('#notes').inputValue(),`${entry.title}：检查尾音与触发位置。`);assert.equal(await page.locator('#version').inputValue(),'review-test');
  await page.goto(pathToFileURL(file).href);await page.waitForFunction(()=>!document.getElementById('play').disabled);
  if(entry.number>=16||['HitLight','Lightning','Reward','Button'].includes(entry.cue))await page.screenshot({path:path.join(out,entry.cue+'.png'),fullPage:true});
  await page.setViewportSize({width:390,height:844});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),entry.cue+' overflows');
  report.packages.push({cue:entry.cue,seconds:entry.seconds,primary_events:entry.primary_events,offline_load:true,frame_seek:true,wav_audition:true,import_and_preview:true,export_roundtrip:true,narrow_layout:true});
  console.log('PASS '+entry.cue);await context.close();
 }
 const p=await browser.newPage({viewport:{width:1440,height:1050}});await p.goto(pathToFileURL(path.join(base,'index.html')).href);assert.equal(await p.locator('tbody tr').count(),entries.length);await p.screenshot({path:path.join(out,'index.png'),fullPage:true});
 for(const href of await p.locator('a').evaluateAll(nodes=>nodes.map(a=>a.getAttribute('href')))){if(href&&!/^https?:/.test(href))assert.ok(fs.existsSync(path.join(base,decodeURIComponent(href))),href);}
 await p.setViewportSize({width:390,height:844});assert.ok(await p.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 assert.deepEqual(report.errors,[]);assert.deepEqual(report.externalRequests,[]);report.status='passed';report.index_links=true;
 }catch(error){report.status='failed';report.failure=error.stack;throw error;}finally{await browser.close();fs.writeFileSync(path.join(out,process.argv[2]?'checks-'+process.argv[2]+'.json':'checks.json'),JSON.stringify(report,null,2));}}
run().catch(e=>{console.error(e);process.exitCode=1;});
