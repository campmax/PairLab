                                                                                      
const {JSDOM,VirtualConsole}=require('jsdom');
const fs=require('fs'),assert=require('assert'),errors=[];
const consoleHook=new VirtualConsole();consoleHook.on('jsdomError',e=>errors.push(String(e)));
const dom=new JSDOM(fs.readFileSync(process.argv[2]||'results/baseline/report.html','utf8'),{runScripts:'dangerously',virtualConsole:consoleHook});
const w=dom.window,d=w.document,el=id=>d.getElementById(id),data=JSON.parse(el('data').textContent),base=data.baseline;
assert.equal(d.querySelectorAll('#tradeRows tr').length,base.trades.length);
assert.equal(d.querySelectorAll('#comparison tr').length,4);assert.equal(d.querySelectorAll('#stress tr').length,1+data.scenarios.length);
assert.equal(d.querySelectorAll('.chart svg').length,5);
for(let i=0;i<base.pairs.length;i++){
 el('pairSelect').value=String(i);el('pairSelect').dispatchEvent(new w.Event('change'));
 assert.equal(el('pairTitle').textContent,base.pairs[i].id);
 assert.equal(d.querySelectorAll('#folds tr').length,base.pairs[i].folds.length);
}
el('pairSelect').value='0';el('eligibleOnly').checked=true;el('eligibleOnly').dispatchEvent(new w.Event('change'));
if(base.pairs[0].metrics.eligible_windows===0)assert(el('folds').textContent.includes('No eligible'));
el('curveMode').value='all';el('curveMode').dispatchEvent(new w.Event('change'));assert.equal(d.querySelectorAll('#equityChart path').length,4);
el('tradeSearch').value='does-not-exist';el('tradeSearch').dispatchEvent(new w.Event('input'));assert(el('tradeRows').textContent.includes('No matching'));
el('tradeSearch').value='';el('tradeSort').value='loss';el('tradeSort').dispatchEvent(new w.Event('change'));assert.equal(d.querySelectorAll('#tradeRows tr').length,base.trades.length);
d.querySelector('[data-tab="method"]').click();assert(!el('method').hidden&&el('overview').hidden);
assert(el('settings').textContent.includes('borrow_rate'));assert(el('period').textContent.includes('Real historical data'));assert.equal(errors.length,0,errors.join('\n'));
d.querySelector('[data-tab="diagnostics"]').click();
assert(!el('diagnostics').hidden&&el('method').hidden);
assert.equal(d.querySelectorAll('#rejectionRows tr').length,base.pairs.length);
assert.equal(d.querySelectorAll('#exitRows tr').length,new Set(base.trades.map(t=>t.reason)).size);
const rejected=base.pairs.reduce((n,p)=>n+p.folds.filter(f=>f.reasons.includes('Residual unit-root screen failed (1%)')).length,0);assert(el('rejectionNote').textContent.includes(String(rejected)));

const expected=base.metrics.end_equity-base.metrics.start_equity+base.metrics.fees+base.metrics.slippage+base.metrics.borrow+base.metrics.interest;
assert(el('costBridge').textContent.includes(new Intl.NumberFormat('en-US',{style:'currency',currency:'USD',maximumFractionDigits:0}).format(expected)));
console.log('Report DOM checks passed: charts, pairs, all windows, filters, sorting, tabs and settings.');dom.window.close();
