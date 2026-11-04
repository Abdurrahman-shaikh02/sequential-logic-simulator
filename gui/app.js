// Simplified visual interface; actual simulation results always come from the C++ engine.
const TRAFFIC_TESTBENCH = `# Traffic Light FSM demonstration
FSM tests/netlists/fsm/traffic_light/traffic_light_fsm.net
CLOCK clk 10
MAX_TIME 60
@0 reset=1 en=0
@5 reset=0 en=0
ASSERT @5 R=0 Y=0 G=1 "Initial state must be Green"
ASSERT @10 R=0 Y=0 G=1 "Hold Green when enable is zero"
@10 en=1
ASSERT @16 R=0 Y=1 G=0 "Green to Yellow"
ASSERT @26 R=1 Y=0 G=0 "Yellow to Red"
ASSERT @36 R=0 Y=0 G=1 "Red to Green"
INVARIANT safe_traffic !(R == 1 && G == 1) "Red and Green never active together"
`;
let wave=null, simResult=null, index=0, timer=null, loadedTraffic=true;
let liveMode=true, liveSession='live-'+Math.random().toString(36).slice(2), busy=false;
let circuitCatalog=[], selectedCircuit=null;
const $=id=>document.getElementById(id);
function label(message,bad=false){$('engineStatus').textContent=message;$('engineStatus').classList.toggle('error',bad)}
function stopAuto(){if(timer){clearInterval(timer);timer=null}$('autoBtn').textContent='▶ Auto Clock'}
function setAssertion(id,status){const item=$(id);item.classList.remove('pass','fail');if(status===null){item.querySelector('.result').textContent='—';return}item.classList.add(status?'pass':'fail');item.querySelector('.result').textContent=status?'PASS':'FAIL'}
function v(signal,i){const arr=wave?.data?.[signal];return Array.isArray(arr)?Number(arr[i]??0):0}
function draw(){if(!wave||!wave.times?.length)return;const signals=(wave.signals||Object.keys(wave.data||{})).filter(s=>Array.isArray(wave.data[s]));const canvas=$('waveformCanvas');const parent=canvas.parentElement;const dpr=window.devicePixelRatio||1;const w=Math.max(620,parent.clientWidth),h=Math.max(300,signals.length*46+32);canvas.style.width=w+'px';canvas.style.height=h+'px';canvas.width=w*dpr;canvas.height=h*dpr;const c=canvas.getContext('2d');c.scale(dpr,dpr);c.fillStyle='#132440';c.fillRect(0,0,w,h);const left=74,right=w-20,area=right-left,step=Math.max(1,wave.times.length-1);const color={clk:'#5be2ff',en:'#90a4fd',reset:'#ffb49f',R:'#ffa5ba',Y:'#f7d47a',G:'#61e5b9'};signals.forEach((s,k)=>{const row=k*46+36,high=row-13,low=row+11;c.strokeStyle='#2b4569';c.lineWidth=1;c.beginPath();c.moveTo(left-5,row+19);c.lineTo(right,row+19);c.stroke();c.fillStyle=color[s]||'#bae6fd';c.font='bold 13px Segoe UI,Arial';c.fillText(s,10,row+5);c.strokeStyle=color[s]||'#c4deff';c.lineWidth=2.5;c.beginPath();const y0=v(s,0)?high:low;c.moveTo(left,y0);for(let i=1;i<wave.times.length;i++){const x=left+i*area/step;c.lineTo(x,v(s,i-1)?high:low);c.lineTo(x,v(s,i)?high:low)}c.stroke()});const cursor=left+index*area/step;c.strokeStyle='#fafcff';c.lineWidth=1;c.setLineDash([4,5]);c.beginPath();c.moveTo(cursor,13);c.lineTo(cursor,h-8);c.stroke();c.setLineDash([]);c.fillStyle='#d4e6ff';c.font='11px monospace';c.fillText(`${wave.times[index]} ns`,Math.min(cursor+5,w-85),16)}
function update(){if(!wave||!wave.times?.length)return;const r=loadedTraffic?v('R',index):0,y=loadedTraffic?v('Y',index):0,g=loadedTraffic?v('G',index):0;$('lampRed').classList.toggle('on',r===1);$('lampYellow').classList.toggle('on',y===1);$('lampGreen').classList.toggle('on',g===1);$('stateLabel').textContent=loadedTraffic?(r?'RED':y?'YELLOW':g?'GREEN':'UNKNOWN'):'Inactive — select Traffic Light FSM';$('timeLabel').textContent=`Time: ${wave.times[index]} ns`;if(loadedTraffic){setAssertion('aMutual',!(r&&g));setAssertion('aOneHot',(r+y+g)===1);const prev=index>0?[v('R',index-1),v('Y',index-1),v('G',index-1)]:null;let sequence=null;if(prev){const prevState=prev[0]?'R':prev[1]?'Y':prev[2]?'G':null;const current=r?'R':y?'Y':g?'G':null;const allowed={G:['G','Y'],Y:['Y','R'],R:['R','G']};if(prevState&&current)sequence=allowed[prevState].includes(current)}setAssertion('aSequence',sequence)}else{['aMutual','aOneHot','aSequence'].forEach(id=>setAssertion(id,null))}draw()}
async function simulate(code,isTraffic){stopAuto();label('Running C++ simulation…');$('stepBtn').disabled=true;try{const response=await fetch('/api/simulate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({testbench:code})});const result=await response.json();if(!response.ok||!result.success||!result.waveform?.times?.length){throw Error(result.error||result.stdout||'No waveform returned from C++ simulator')}simResult=result;wave=result.waveform;loadedTraffic=isTraffic;liveMode=false;index=0;label('Simulator Ready');$('simOutput').hidden=false;$('simOutput').textContent=result.stdout||'Simulation completed';update();}catch(error){label('Simulation error',true);$('testOutput').textContent='Simulation error: '+error.message;$('simOutput').hidden=false;$('simOutput').textContent=error.message;wave=null;['aMutual','aOneHot','aSequence'].forEach(id=>setAssertion(id,null))}finally{$('stepBtn').disabled=false}}
function trafficScript(){// Keep all original assertions for enabled clock; for disabled clock, omit only progression assertions.
const enabled=$('enableSwitch').checked, reset=$('resetSwitch').checked;
let code=TRAFFIC_TESTBENCH;
if(!enabled){code=code.replace('@10 en=1','@10 en=0').split('\n').filter(line=>!(/ASSERT @(16|26|36)/.test(line))).join('\n')}
if(reset){code=code.replace('@5 reset=0 en=0','@5 reset=1 en=0').split('\n').filter(line=>!(/ASSERT @(16|26|36)/.test(line))).join('\n')}
return code}
async function liveRequest(action){
    if(busy)return;
    busy=true;
    $('stepBtn').disabled=true;
    try{
        label(action==='reset'?'Resetting C++ circuit…':'Evaluating new clock edge in C++…');
        const result=await fetch('/api/live',{method:'POST',headers:{'Content-Type':'application/json'},
           body:JSON.stringify({session:liveSession,action,en:$('enableSwitch').checked?1:0,reset:$('resetSwitch').checked?1:0})});
        const data=await result.json();
        if(!result.ok||!data.success)throw Error(data.error||'Live simulation failed');
        if(action==='reset'){
            wave={signals:['clk','reset','en','G','Y','R'],times:[],data:{clk:[],reset:[],en:[],G:[],Y:[],R:[]}};
            loadedTraffic=true;liveMode=true;
        }
        for(const sample of data.samples){
            wave.times.push(sample.time);
            for(const sig of wave.signals)wave.data[sig].push(sample.signals[sig]??0);
        }
        index=wave.times.length-1;
        $('stepInfo').textContent='Live C++ simulation: each Tick adds new waveform values.';
        update();label('Live simulator ready');return true;
    }catch(e){label('Live simulator error',true);$('stepInfo').textContent='Error: '+e.message;return false;}
    finally{busy=false;$('stepBtn').disabled=false}
}
async function step(){
    // The dashboard clock controls are reserved for the live Traffic Light demo.
    // Never let them step an unrelated combinational/sequential/FSM test waveform.
    if(!liveMode){const reset=await liveRequest('reset');if(reset===false)return;}
    return liveRequest('step');
}
function downloadBlob(name,type,content){const url=URL.createObjectURL(new Blob([content],{type}));const link=document.createElement('a');link.href=url;link.download=name;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000)}
function downloadWave(type){if(!wave){alert('Run the simulator first.');return}if(type==='json'){downloadBlob('waveform.json','application/json',JSON.stringify(wave,null,2));return}if(type==='txt'){const lines=['Time(ns) '+wave.signals.join(' ')];wave.times.forEach((t,i)=>lines.push(t+' '+wave.signals.map(s=>v(s,i)).join(' ')));downloadBlob('waveform.txt','text/plain',lines.join('\n')+'\n');return}const names=wave.signals||[];const symbols='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';let str='$date Generated by SLMS GUI $end\n$version SLMS $end\n$timescale 1ns $end\n$scope module circuit $end\n';names.forEach((s,i)=>str+=`$var wire 1 ${symbols[i]} ${s} $end\n`);str+='$upscope $end\n$enddefinitions $end\n';wave.times.forEach((t,i)=>{str+='#'+t+'\n';names.forEach((s,j)=>str+=v(s,i)+symbols[j]+'\n')});downloadBlob('simulation.vcd','text/plain',str)}
async function runTests(){const btn=$('testsBtn');btn.disabled=true;$('testOutput').textContent='Running all seven C++ testing categories…';try{const res=await fetch('/api/tests');const data=await res.json();if(!res.ok||data.error)throw Error(data.error||'Test service error');const tests=data.tests||[];const cats=[...new Set(tests.map(x=>x.cat))];const passed=tests.filter(x=>x.ok).length;const failed=tests.length-passed;$('testOutput').textContent=tests.length?`C++ TEST SUITE SUMMARY\nCategories reported: ${cats.length}/7 (${cats.join(', ')})\nIndividual checks: ${passed} passed, ${failed} failed, ${tests.length} total\n\nCATEGORY BREAKDOWN\n`+cats.map(cat=>{const rows=tests.filter(x=>x.cat===cat);return `${cat}: ${rows.filter(x=>x.ok).length}/${rows.length} passed`;}).join('\n')+'\n\nINDIVIDUAL TEST CASES\n'+tests.map(x=>`${x.ok?'PASS':'FAIL'}  ${x.cat}  ${x.name}`).join('\n'):'No individual test records were returned by the server.'}catch(e){$('testOutput').textContent='Test suite error: '+e.message}finally{btn.disabled=false}}
async function runGenericSimulation(){
 const out=$('genericOutput');const btn=$('genericRunBtn');btn.disabled=true;out.hidden=false;out.textContent='Running selected circuit through the C++ engine…';
 try{
  const payload={circuitType:selectedCircuit?.circuitType||($('genericCategory').value==='fsm'?'fsm':'netlist'),netlist:$('genericPath').value.trim(),maxTime:Number($('genericMaxTime').value),clockPeriod:$('genericClock').value.trim(),stimuli:$('genericStimuli').value};
  const response=await fetch('/api/generic-simulate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});const result=await response.json();
  if(!result.waveform?.times?.length)throw Error(result.error||result.stderr||result.stdout||'Simulation failed');
  if(result.netlistPath)$('genericPath').value=result.netlistPath;wave=result.waveform;simResult=result;loadedTraffic=selectedCircuit?.id==='fsm:traffic-light-fsm' && result.netlistPath===selectedCircuit.netlist;liveMode=false;index=0;stopAuto();$('waveformTitle').textContent=selectedCircuit?.name+' — waveform';update();
  const checks=result.assertionSummary||{total:0,passed:0,failed:0};
  const verification=result.verification||(checks.total?(checks.failed?'FAILED':'PASSED'):'NOT_VERIFIED');
  label(verification==='FAILED'?'Simulation completed — assertions failed':verification==='NOT_VERIFIED'?'Simulation complete — not verified':'Simulation and assertions passed',verification==='FAILED');
  out.textContent=(result.stdout||'Simulation completed.')+'\n\nVERIFICATION SUMMARY\nStatus: '+verification+'\nEvaluated: '+checks.total+' | Passed: '+checks.passed+' | Failed: '+checks.failed+'\n'+(checks.total?'Counts are taken from the C++ assertion-engine report.':'No ASSERT directives were supplied; waveform only, not verified.')+'\n\nGenerated testbench:\n'+(result.generatedTestbench||'(not returned)')+'\nSignals: '+(wave.signals||Object.keys(wave.data||{})).join(', ')+'\nSamples: '+wave.times.length;
 }catch(e){out.textContent='Simulation failed: '+e.message;label('Simulation failed',true)}finally{btn.disabled=false}
}
async function loadCircuitCatalog(){
 const category=$('genericCategory'),circuit=$('genericCircuit');
 try{
  const response=await fetch('/api/catalog',{cache:'no-store'});const data=await response.json();if(!response.ok||!data.success)throw Error(data.error||'Circuit catalogue unavailable');
  circuitCatalog=data.circuits||[];$('catalogCount').textContent=`${circuitCatalog.length} bundled circuits`;
  category.onchange=()=>populateCircuitOptions();circuit.onchange=()=>applySelectedCircuit();
  populateCircuitOptions();
 }catch(err){$('catalogCount').textContent='Circuit library unavailable';circuit.innerHTML='<option value="">Unable to load circuits</option>';label('Circuit catalogue error: '+err.message,true)}
}
function populateCircuitOptions(){
 const category=$('genericCategory').value;const circuit=$('genericCircuit');const items=circuitCatalog.filter(x=>x.category===category);
 circuit.innerHTML='';items.forEach(item=>{const option=document.createElement('option');option.value=item.id;option.textContent=item.name;circuit.appendChild(option)});
 if(items.length){circuit.value=items[0].id;applySelectedCircuit()}else{$('genericPath').value='';$('genericStimuli').value='';$('circuitDescription').textContent='No bundled examples are available in this category.'}
}
function applySelectedCircuit(){
 const item=circuitCatalog.find(x=>x.id===$('genericCircuit').value);if(!item)return;selectedCircuit=item;
 $('genericPath').value=item.netlist;$('genericMaxTime').value=item.maxTime;$('genericClock').value=item.clockPeriod||'';$('genericClock').disabled=!item.clockPeriod;
 $('genericStimuli').value=item.stimuli;$('circuitDescription').textContent=item.description||`Reference testbench: ${item.testbench}`;
 $('genericRunBtn').textContent='Run '+item.name;$('genericOutput').hidden=true;$('genericOutput').textContent='';
 // A newly selected circuit must never leave the previous circuit's waveform on screen.
 wave=null;simResult=null;index=0;stopAuto();loadedTraffic=false;liveMode=false;
 $('waveformTitle').textContent='Waveform Viewer — run selected circuit';$('timeLabel').textContent='Time: —';
 $('lampRed').classList.remove('on');$('lampYellow').classList.remove('on');$('lampGreen').classList.remove('on');$('stateLabel').textContent='Not simulated';
 const canvas=$('waveformCanvas');const ctx=canvas.getContext('2d');ctx.clearRect(0,0,canvas.width,canvas.height);
 ['aMutual','aOneHot','aSequence'].forEach(id=>setAssertion(id,null));
}

let vendingCoins=[];
let vendingGeneration=0; // Invalidate an unfinished request when Reset session is clicked.
async function runVending(){const requestId=++vendingGeneration;const out=$('vendingOutput');out.textContent='Running vending FSM through the C++ engine…';$('coin5Btn').disabled=true;$('coin10Btn').disabled=true;
 try{const response=await fetch('/api/vending',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({coins:vendingCoins})});const result=await response.json();if(requestId!==vendingGeneration)return;if(!response.ok||!result.success)throw Error(result.error||'Vending simulation failed');
  $('vendingCoins').textContent=vendingCoins.length?vendingCoins.map(c=>'₹'+c).join(' → '):'None';$('vendingVend').textContent=result.vend?'YES':'NO';$('vendingCredit').textContent=result.credit5?'YES':'NO';
  const checks=result.assertionSummary||{total:0,passed:0,failed:0};
  const verification=result.verification||(checks.total?(checks.failed?'FAILED':'PASSED'):'NOT_VERIFIED');
  out.textContent=(result.stdout||'Simulation completed.')+'\n\nVENDING VERIFICATION\nStatus: '+verification+'\nEvaluated: '+checks.total+' | Passed: '+checks.passed+' | Failed: '+checks.failed+'\nFinal outputs at '+result.time+' ns:\nvend = '+result.vend+'\ncredit5 = '+result.credit5+'\n\nGenerated testbench:\n'+(result.generatedTestbench||'(not returned)');
  wave=result.waveform;simResult=result;loadedTraffic=false;liveMode=false;index=wave.times.findIndex(t=>t>=result.time);if(index<0)index=wave.times.length-1;update();label(verification==='FAILED'?'Vending assertions failed':verification==='NOT_VERIFIED'?'Vending simulation — not verified':'Vending simulation verified',verification==='FAILED');
 }catch(e){if(requestId===vendingGeneration){out.textContent='Vending simulation failed: '+e.message;label('Vending simulation failed',true)}}finally{if(requestId===vendingGeneration){$('coin5Btn').disabled=false;$('coin10Btn').disabled=false}}}
async function loadTestbenchFromFile() {
  const chooser=$('tbFileInput');
  const status=$('tbFileStatus');
  const file=chooser.files && chooser.files[0];
  if(!file)return;
  if(!file.name.toLowerCase().endsWith('.tb')) {
    status.textContent='Please select a .tb testbench file.';
    chooser.value='';
    return;
  }
  if(file.size===0 || file.size>1024*1024) {
    status.textContent='File must be nonempty and at most 1 MB.';
    chooser.value='';
    return;
  }
  try {
    const content=await file.text();
    if(!content.trim())throw Error('File contains no testbench instructions.');
    $('tbEditor').value=content;
    status.textContent='Loaded '+file.name+' ('+file.size+' bytes). Click Run this testbench to simulate.';
    $('advanced').open=true;
  } catch(err) { status.textContent='Unable to read file: '+err.message; }
}
async function runUploadedOrExisting(){
 const file=$('netlistFileInput').files?.[0];
 if(!file){return simulate($('tbEditor').value,false)}
 const resultBox=$('simOutput');resultBox.hidden=false;
 try{
  if(!/\.(table|tbl|net)$/i.test(file.name)||file.size===0||file.size>1024*1024)throw Error('Netlist must be .table, .tbl or .net, at most 1 MB.');
  const body={netlistName:file.name,netlistContent:await file.text(),testbench:$('tbEditor').value};
  resultBox.textContent='Running uploaded netlist and testbench in C++…';
  const response=await fetch('/api/file-simulate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});
  const result=await response.json();
  if(!response.ok||!result.success||!result.waveform?.times?.length)throw Error(result.error||result.stderr||result.stdout||'Simulation failed');
  if(result.netlistPath)$('genericPath').value=result.netlistPath;wave=result.waveform;simResult=result;loadedTraffic=false;liveMode=false;index=0;stopAuto();update();label('Uploaded circuit simulated');
  resultBox.textContent=result.stdout||'Simulation finished';
 }catch(e){resultBox.textContent='Upload simulation failed: '+e.message;label('Upload simulation failed',true)}
}
document.addEventListener('DOMContentLoaded',()=>{$('tbEditor').value=TRAFFIC_TESTBENCH;$('tbFileInput').addEventListener('change',loadTestbenchFromFile);$('netlistFileInput').addEventListener('change',()=>{$('netlistFileStatus').textContent=$('netlistFileInput').files?.[0]?.name||'No external netlist selected';});$('stepBtn').onclick=step;$('resetBtn').onclick=()=>{stopAuto();liveRequest('reset')};$('enableSwitch').onchange=()=>{$('stepInfo').textContent='Enable will be applied on the next Tick.'};$('resetSwitch').onchange=()=>{$('stepInfo').textContent='Reset will be applied on the next Tick.'};$('autoBtn').onclick=()=>{if(timer){stopAuto();return}$('autoBtn').textContent='❚❚ Pause Clock';timer=setInterval(()=>{if(!busy)step()},650)};$('testsBtn').onclick=runTests;$('loadTrafficBtn').onclick=()=>{$('tbEditor').value=TRAFFIC_TESTBENCH;$('tbFileInput').value='';$('tbFileStatus').textContent='Traffic-light example restored.';liveRequest('reset')};$('runCustomBtn').onclick=runUploadedOrExisting;$('vcdBtn').onclick=()=>downloadWave('vcd');$('txtBtn').onclick=()=>downloadWave('txt');$('jsonBtn').onclick=()=>downloadWave('json');window.addEventListener('resize',draw);
 $('genericRunBtn').onclick=runGenericSimulation;
 $('genericReloadBtn').onclick=()=>{if(selectedCircuit)applySelectedCircuit()};
 loadCircuitCatalog();
 $('coin5Btn').onclick=()=>{if(vendingCoins.length<50){vendingCoins.push(5);runVending()}};
 $('coin10Btn').onclick=()=>{if(vendingCoins.length<50){vendingCoins.push(10);runVending()}};
 $('vendingResetBtn').onclick=()=>{
  ++vendingGeneration; // Clear session and ignore any previous in-flight vending result.
  vendingCoins=[];
  $('vendingCoins').textContent='None';
  $('vendingVend').textContent='NO';
  $('vendingCredit').textContent='NO';
  $('coin5Btn').disabled=false;
  $('coin10Btn').disabled=false;
  $('vendingOutput').textContent=[
    'SLMS ASSERTION ENGINE REPORT',
    'Total Evaluated : 0',
    'Passed          : 0 (N/A)',
    'Failed          : 0',
    'Overall Status  : [ NOT VERIFIED ]',
    '',
    'Session reset. Insert a coin to run new assertions.'
  ].join('\n');
  label('Vending session cleared — no new assertions evaluated');
};
 $('genericClock').disabled=true;liveRequest('reset')});
