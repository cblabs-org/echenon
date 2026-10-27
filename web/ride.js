(function(){
var w,rt=1e3,el=0,st='IDLE',spd=0,dst=0,ap=0,pc=0;
var $=function(i){return document.getElementById(i)};
function fmt(s){var h=s/3600|0,m=(s%3600)/60|0,sec=s%60|0;return h?(h+':'+p(m)+':'+p(sec)):m+':'+p(sec)}
function p(n){return n<10?'0'+n:''+n}
function ws(){
var u='ws://'+location.host+'/ws/echenon/live';
w=new WebSocket(u);
w.onopen=function(){$('ws-dot').className='conn on';rt=1e3};
w.onclose=function(){$('ws-dot').className='conn off';setTimeout(ws,rt);rt=Math.min(rt*2,3e4)};
w.onmessage=function(e){
try{var d=JSON.parse(e.data);
$('cad').textContent=Math.round(d.cadence_rpm||0);
$('pwr').textContent=Math.round(d.power_watts||0);
$('spd').textContent=(d.speed_kph||0).toFixed(1);
$('hr').textContent=d.hr||'--';
$('lvl').textContent=d.level_echelon||d.level||'--';
if(d.session_state){st=d.session_state;ub()}
if(d.session_elapsed_s!=null){el=d.session_elapsed_s;$('tmr').textContent=fmt(el)}
spd=d.speed_kph||0;
}catch(x){}
}}
function ub(){
var b=$('badge');b.textContent=st;
b.className='badge '+(st==='ACTIVE'?'b-active':st==='PAUSED'?'b-paused':'b-idle');
$('b-start').disabled=st==='ACTIVE';
$('b-pause').disabled=st!=='ACTIVE';
$('b-pause').textContent=st==='PAUSED'?'Resume':'Pause';
$('b-stop').disabled=st!=='ACTIVE'&&st!=='PAUSED';
}
window.R={cmd:function(c){
if(c==='stop'&&!confirm('Stop this ride?'))return;
if(c==='pause'&&st==='PAUSED')c='resume';
fetch('/api/session/'+c,{method:'POST'}).then(function(r){return r.json()}).then(function(d){
if(d.state){st=d.state;ub()}ss();
})}};
function ss(){
fetch('/api/session/status').then(function(r){return r.json()}).then(function(d){
if(d.state){st=d.state;ub()}
if(d.elapsed_s!=null){el=d.elapsed_s;$('tmr').textContent=fmt(el)}
if(d.avg_power_w!=null)$('ap').textContent=d.avg_power_w+' W';
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
h+='<div class="r"><span>'+dt+' · '+dur+' · '+((r.avg_power_w||0))+'W</span><a href="/api/session/download?id='+r.session_id+'">TCX</a></div>';
});$('history').innerHTML=h;
}).catch(function(){});
}
ws();ss();lh();setInterval(ss,5e3);setInterval(lh,30e3);
})();
