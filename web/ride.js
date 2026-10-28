(function(){
var w,rt=1e3,el=0,st='IDLE',spd=0,dst=0,ap=0,pc=0,lastRx=0;
var $=function(i){return document.getElementById(i)};
function fmt(s){var h=s/3600|0,m=(s%3600)/60|0,sec=s%60|0;return h?(h+':'+p(m)+':'+p(sec)):m+':'+p(sec)}
function p(n){return n<10?'0'+n:''+n}
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
var u='ws://'+location.host+'/ws/echenon/live';
w=new WebSocket(u);
w.onopen=function(){$('ws-dot').className='conn on';rt=1e3;try{w.send('ping')}catch(e){}};
w.onclose=function(){$('ws-dot').className='conn off';setTimeout(ws,rt);rt=Math.min(rt*2,3e4)};
w.onmessage=function(e){lastRx=Date.now();try{tl(JSON.parse(e.data))}catch(x){}};
}
function pl(){
if(!w||w.readyState!==1||(lastRx>0&&(Date.now()-lastRx)>2500)){
fetch('/api/echenon/live').then(function(r){return r.json()}).then(function(d){tl(d)}).catch(function(){});
}}
function ub(){
var isA=st==='ACTIVE',isP=st==='PAUSED';
if(!isA&&!isP)st='IDLE';
var b=$('badge');b.textContent=st;
b.className='badge '+(isA?'b-active':isP?'b-paused':'b-idle');
$('b-start').disabled=isA;
$('b-pause').disabled=!isA;
$('b-pause').textContent=isP?'Resume':'Pause';
$('b-stop').disabled=!isA&&!isP;
}
window.R={
cmd:function(c){
if(c==='stop'){var m=$('stop-modal');if(m)m.style.display='flex';return}
if(c==='pause'&&st==='PAUSED')c='resume';
fetch('/api/session/'+c,{method:'POST'}).then(function(r){return r.json()}).then(function(d){
if(d&&d.state==='ENDED'){st='IDLE'}
else if(d&&d.state){st=d.state}
ub();ss();lh();
}).catch(function(){ub();});
},
stopAction:function(a){
if(a==='cancel'){$('stop-modal').style.display='none';return}
if(a==='discard'){
if(!confirm('Discard this ride? It will not be recorded or sent to Strava.'))return;
$('stop-modal').style.display='none';
fetch('/api/session/discard',{method:'POST'}).then(function(r){return r.json()}).then(function(d){
st='IDLE';ub();ss();lh();
}).catch(function(){ub();});
return;
}
if(a==='save'){
$('stop-modal').style.display='none';
fetch('/api/session/stop',{method:'POST'}).then(function(r){return r.json()}).then(function(d){
st='IDLE';ub();ss();lh();
}).catch(function(){ub();});
return;
}
}
};
function ss(){
fetch('/api/session/status').then(function(r){return r.json()}).then(function(d){
if(d.state==='ENDED'){st='IDLE'}else if(d.state){st=d.state}
ub();
if(d.elapsed_s!=null){el=d.elapsed_s;$('tmr').textContent=fmt(el)}
if(d.avg_power_w!=null)$('ap').textContent=d.avg_power_w+' W';
var as=$('auto-start');
if(as&&(d.auto_start_enabled!=null||d.auto_start!=null)){
as.checked=!!(d.auto_start_enabled!=null?d.auto_start_enabled:d.auto_start);
}
});
fetch('/api/strava/status').then(function(r){return r.json()}).then(function(d){
var t=[];
if(d.pending_count)t.push('⏳ '+d.pending_count+' pending');
if(d.queue_size>0&&!d.pending_count)t.push('✓ uploads synced');
if(d.authenticated===false)t.push('Strava not connected');
$('upload').textContent=t.join(' · ')||'';
}).catch(function(){});
}
function lh(){
fetch('/api/session/history').then(function(r){return r.json()}).then(function(d){
var rides=d.rides||d;if(!rides||!rides.length){$('history').innerHTML='<div id="empty">No rides yet — start your first session!</div>';return}
var h='';rides.slice(-5).reverse().forEach(function(r){
var dt=r.start_epoch_s?new Date(r.start_epoch_s*1e3).toLocaleDateString():'--';
var dur=fmt(r.duration_s||0);
h+='<div class="r"><span>'+dt+' · '+dur+' · '+((r.avg_power_w||0))+'W</span><a href="/api/session/download?id='+r.session_id+'" download="echenon_'+r.session_id+'.tcx">TCX</a>'+(r.activity_id?'<a href="https://www.strava.com/activities/'+r.activity_id+'" target="_blank">Strava ↗</a>':'')+'</div>';
});$('history').innerHTML=h;
}).catch(function(){});
}
var asInit=$('auto-start');
if(asInit){
asInit.onchange=function(){
fetch('/api/session/config',{
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
