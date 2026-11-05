                                                                        
const {JSDOM,VirtualConsole}=require('jsdom'),fs=require('fs'),assert=require('assert');
const data=JSON.parse(fs.readFileSync(process.argv[2],'utf8')),now=Date.now()/1000;
for(const r of data.pairs){r.current=true;r.candidate=false;r.quote_time_a=r.quote_time_b=now-2;r.session_open=now-3600;r.session_close=now+3600;r.z=1;}
data.pairs[0].candidate=true;data.pairs[0].z=-2.5;
const errors=[],vc=new VirtualConsole();vc.on('jsdomError',e=>errors.push(String(e)));
const html=fs.readFileSync('web/live.html','utf8');
const dom=new JSDOM(html,{runScripts:'dangerously',virtualConsole:vc,beforeParse(w){w.PAIRLAB_TEST={status:'ok',message:'test',data,paper:null};}}),w=dom.window,d=w.document,$=id=>d.getElementById(id);
assert.equal($('count').textContent,'4');assert.equal($('fresh').textContent,'4');assert.equal($('candidates').textContent,'1');assert.equal(d.querySelectorAll('#rows tr').length,4);
w.eval('view.data.generated_at+=1;view.data.pairs.forEach(r=>{r.quote_time_a+=1;r.quote_time_b+=1;r.z+=.1});render()');assert(d.querySelector('#chart svg'));
$('pairSelect').selectedIndex=1;$('pairSelect').dispatchEvent(new w.Event('change'));assert($('title').textContent.includes(data.pairs[1].id));
w.eval('connected=false;render()');assert.equal($('candidates').textContent,'0');assert.equal($('fresh').textContent,'0');
w.eval('connected=true;view.status="error";render()');assert.equal($('candidates').textContent,'0');
w.eval('view.status="ok";view.data.pairs.forEach(r=>r.quote_time_a=0);render()');assert.equal($('fresh').textContent,'0');
w.eval('view.data.source="<img src=x onerror=alert(1)>";render()');assert(!$('source').querySelector('img'));assert($('source').textContent.includes('<img'));
assert.equal(errors.length,0,errors.join('\n'));console.log('Live DOM checks passed: render, charts, selection, stale/error/disconnect gating, escaping.');dom.window.close();
