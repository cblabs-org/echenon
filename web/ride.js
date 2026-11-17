(function(){
var w,rt=1e3,el=0,st='IDLE',spd=0,dst=0,ap=0,pc=0,lastRx=0;
var connectedSinks=null; // cached list of configured+authenticated service_ids
var $=function(i){return document.getElementById(i)};
function f(u,o){return fetch(u,Object.assign({credentials:'same-origin'},o))}
function fmt(s){var h=s/3600|0,m=(s%3600)/60|0,sec=s%60|0;return h?(h+':'+p(m)+':'+p(sec)):m+':'+p(sec)}
function p(n){return n<10?'0'+n:''+n}
function escHtml(s){return s?String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;'):''}
function tl(d){
if(!d)return;
var rb=$('runaway-banner');
if(rb){rb.style.display=(d.runaway===true||d.runaway==='true')?'block':'none'}
$('cad').textContent=Math.round(d.cadence_rpm||0);
$('pwr').textContent=Math.round(d.power_watts||0);
$('spd').textContent=(d.speed_kph||0).toFixed(1);
$('hr').textContent=d.hr||'--';
$('lvl').textContent=d.level_echelon||d.level||'--';
if(d.session_state){st=(d.session_state==='ENDED')?'IDLE':d.session_state;ub()}
if(d.session_elapsed_s!=null){el=d.session_elapsed_s;$('tmr').textContent=fmt(el)}
spd=d.speed_kph||0;
}
function ws(){
var u=(location.protocol==='https:'?'wss://':'ws://')+location.host+'/ws/echenon/live';
w=new WebSocket(u);
w.onopen=function(){$('ws-dot').className='conn on';rt=1e3;try{w.send('ping')}catch(e){}};
w.onclose=function(){$('ws-dot').className='conn off';setTimeout(ws,rt);rt=Math.min(rt*2,3e4)};
w.onmessage=function(e){lastRx=Date.now();try{tl(JSON.parse(e.data))}catch(x){}};
}
function pl(){
if(!w||w.readyState!==1||(lastRx>0&&(Date.now()-lastRx)>2500)){
f('/api/echenon/live').then(function(r){return r.json()}).then(function(d){tl(d)}).catch(function(){});
}}
function ub(){
var isA=st==='ACTIVE',isP=st==='PAUSED';
if(!isA&&!isP)st='IDLE';
var b=$('badge');b.textContent=st;
b.className='badge '+(isA?'b-active':isP?'b-paused':'b-idle');
$('b-start').disabled=isA;
$('b-pause').disabled=!isA&&!isP;
$('b-pause').textContent=isP?'Resume':'Pause';
$('b-stop').disabled=!isA&&!isP;
}
window.R={
cmd:function(c){
if(c==='stop'){var m=$('stop-modal');if(m)m.style.display='flex';return}
if(c==='pause'&&st==='PAUSED')c='resume';
f('/api/session/'+c,{method:'POST'}).then(function(r){return r.json()}).then(function(d){
if(d&&d.state==='ENDED'){st='IDLE'}
else if(d&&d.state){st=d.state}
// Fix 1: If start returned error (409), force state sync from server
if(d&&d.ok===false){
ss();return;
}
ub();ss();lh();
}).catch(function(){
// Fix 1: On network error, re-sync state from server
ss();
});
},
stopAction:function(a){
if(a==='cancel'){$('stop-modal').style.display='none';return}
if(a==='discard'){
if(!confirm('Discard this ride? It will not be recorded or sent to Strava.'))return;
$('stop-modal').style.display='none';
f('/api/session/discard',{method:'POST'}).then(function(r){return r.json()}).then(function(d){
st='IDLE';ub();ss();lh();
}).catch(function(){ss();});
return;
}
if(a==='save'){
$('stop-modal').style.display='none';
f('/api/session/stop',{method:'POST'}).then(function(r){return r.json()}).then(function(d){
st='IDLE';ub();ss();lh();
}).catch(function(){ss();});
return;
}
}
};
function ss(){
f('/api/session/status').then(function(r){return r.json()}).then(function(d){
if(d.state==='ENDED'){st='IDLE'}else if(d.state){st=d.state}
ub();
if(d.elapsed_s!=null){el=d.elapsed_s;$('tmr').textContent=fmt(el)}
if(d.avg_power_w!=null)$('ap').textContent=d.avg_power_w+' W';
var as=$('auto-start');
if(as&&(d.auto_start_enabled!=null||d.auto_start!=null)){
as.checked=!!(d.auto_start_enabled!=null?d.auto_start_enabled:d.auto_start);
}
});
f('/api/upload/status').then(function(r){return r.json()}).then(function(d){
var t=[];
if(d.pending_count)t.push('⏳ '+d.pending_count+' pending');
if(d.pending_count===0)t.push('✓ uploads synced');
if(d.last_error)t.push('⚠ '+d.last_error);
$('upload').textContent=t.join(' · ')||'';
}).catch(function(){});
}

// --- Fetch configured+authenticated sinks ---
function fetchSinks(){
return f('/api/upload/sinks').then(function(r){return r.json()}).then(function(d){
var sinks=d.sinks||[];
connectedSinks=[];
for(var i=0;i<sinks.length;i++){
if(sinks[i].configured&&sinks[i].authenticated){
connectedSinks.push((sinks[i].service_id||'').toLowerCase());
}
}
}).catch(function(){connectedSinks=null;});
}

// --- TCX Download via fetch+Blob (Fix 2) ---
function downloadTcx(sessionId){
f('/api/session/download?id='+sessionId).then(function(r){
if(!r.ok)throw new Error('Download failed');
return r.blob();
}).then(function(blob){
var url=URL.createObjectURL(blob);
var a=document.createElement('a');
a.href=url;a.download='echenon_'+sessionId+'.tcx';
document.body.appendChild(a);a.click();
document.body.removeChild(a);URL.revokeObjectURL(url);
}).catch(function(err){alert('Download failed: '+err)});
}

// --- Build sync status for a ride (Fix 3 + Fix 4) ---
function buildSyncHtml(ride){
if(!ride.sync||!ride.sync.length)return '';
var filtered=ride.sync;
if(connectedSinks!==null){
filtered=ride.sync.filter(function(s){
return connectedSinks.indexOf((s.service_id||'').toLowerCase())!==-1;
});
}
if(!filtered.length)return '';
var h='<div style="margin-top:4px;font-size:11px">';
for(var i=0;i<filtered.length;i++){
var s=filtered[i];
var sid=ride.session_id;
var status=(s.status||'NONE').toUpperCase();
var label=formatServiceLabel(s.service_id);
if(status==='COMPLETE'){
var url=s.remote_url||buildRemoteUrl(s.service_id,s.remote_id);
if(url){
h+='<a href="'+escHtml(url)+'" target="_blank" style="color:var(--primary, #2196F3);text-decoration:none;margin-right:6px">'+escHtml(label)+' ↗</a>';
}else{
h+='<span style="color:var(--success, #4CAF50);margin-right:6px">'+escHtml(label)+' ✓</span>';
}
}else if(status==='PENDING'){
h+='<span style="color:var(--warning, #FF9800);margin-right:6px">'+escHtml(label)+' ⏳</span>';
}else if(status==='FAILED'){
h+='<span style="color:var(--danger, #f44336);margin-right:6px">'+escHtml(label)+' ❌</span>';
h+='<a href="#" data-retry-sid="'+sid+'" data-retry-svc="'+escHtml(s.service_id)+'" style="color:var(--warning, #FF9800);text-decoration:none;margin-right:6px">retry</a>';
}else{
h+='<a href="#" data-retry-sid="'+sid+'" data-retry-svc="'+escHtml(s.service_id)+'" style="color:var(--primary, #2196F3);text-decoration:none;margin-right:6px">Sync '+escHtml(label)+'</a>';
}
}
h+='</div>';
return h;
}
function formatServiceLabel(serviceId){
var sid=(serviceId||'').toLowerCase();
if(sid==='strava')return 'Strava';
if(sid==='intervals'||sid==='intervals_icu')return 'Intervals.icu';
if(sid==='wahoo')return 'Wahoo';
return serviceId;
}
function buildRemoteUrl(serviceId,remoteId){
if(!remoteId)return '';
var sid=(serviceId||'').toLowerCase();
if(sid==='strava')return 'https://www.strava.com/activities/'+remoteId;
if(sid==='intervals'||sid==='intervals_icu')return 'https://intervals.icu/activities/'+remoteId;
if(sid==='wahoo')return 'https://api.wahooligan.com/v1/workouts/'+remoteId;
return '';
}

function lh(){
fetchSinks().then(function(){
f('/api/session/history').then(function(r){return r.json()}).then(function(d){
var rides=d.rides||d;if(!rides||!rides.length){$('history').innerHTML='<div id="empty">No rides yet — start your first session!</div>';return}
var h='';rides.slice(-5).reverse().forEach(function(r){
var dt=r.start_epoch_s?new Date(r.start_epoch_s*1e3).toLocaleDateString():'--';
var dur=fmt(r.duration_s||0);
h+='<div class="r"><span>'+dt+' · '+dur+' · '+((r.avg_power_w||0))+'W</span>';
h+='<a href="#" data-dl-sid="'+r.session_id+'" style="color:var(--primary, #2196F3);text-decoration:none;font-size:12px">TCX</a>';
h+=buildSyncHtml(r);
h+='</div>';
});$('history').innerHTML=h;
// Attach download click handlers (Fix 2)
var dlLinks=document.querySelectorAll('[data-dl-sid]');
for(var i=0;i<dlLinks.length;i++){
dlLinks[i].addEventListener('click',function(e){
e.preventDefault();downloadTcx(this.getAttribute('data-dl-sid'));
});
}
// Attach retry/sync click handlers (Fix 3)
var retryLinks=document.querySelectorAll('[data-retry-sid]');
for(var i=0;i<retryLinks.length;i++){
retryLinks[i].addEventListener('click',function(e){
e.preventDefault();
var sid=this.getAttribute('data-retry-sid');
var svc=this.getAttribute('data-retry-svc');
var el=this;el.textContent='...';
f('/api/session/upload',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({session_id:Number(sid),service_id:svc})
}).then(function(r){return r.json()}).then(function(d){
if(d&&d.ok){el.textContent='Queued ✓';el.style.color='var(--success, #4CAF50)';setTimeout(lh,2000)}
else{el.textContent='Failed';el.style.color='var(--danger, #f44336)'}
}).catch(function(){el.textContent='Error';el.style.color='var(--danger, #f44336)'});
});
}
}).catch(function(){});
});
}
var asInit=$('auto-start');
if(asInit){
asInit.onchange=function(){
f('/api/session/config',{
method:'POST',
headers:{'Content-Type':'application/json'},
body:JSON.stringify({auto_start:asInit.checked})
}).catch(function(){});
};
}
var sm=$('stop-modal');
if(sm){
sm.addEventListener('click',function(e){if(e.target===sm)sm.style.display='none'});
}
document.addEventListener('keydown',function(e){
if(e.key==='Escape'){var m=$('stop-modal');if(m&&m.style.display==='flex')m.style.display='none'}
});
ws();setInterval(function(){if(w&&w.readyState===1)try{w.send('ping')}catch(e){}},15e3);ss();lh();setInterval(pl,2500);setInterval(ss,5e3);setInterval(lh,30e3);
})();
