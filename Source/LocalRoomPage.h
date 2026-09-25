#pragma once

#include <juce_core/juce_core.h>

// The page a phone opens in a local seminar room (LocalRoom). One file, no
// external requests: the room has no internet, so nothing may come from a
// CDN - not a font, not a script. The app substitutes /*STRINGS*/{} with
// the phone texts in its own language.
//
// The answer rules are the trainer's (docs/orientation.md): tapping a
// button or letting go of the slider *is* the answer - there is no Submit -
// and it can be changed until the presenter shows the answer.
namespace LocalRoomPage
{
    inline juce::String html()
    {
        return juce::String::fromUTF8 (R"PAGE(<!doctype html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>abcTrain</title>
<style>
:root{--bg:#15151d;--panel:#1e1e2e;--line:#32323f;--text:#e0e0e0;--dim:#b0b0bf;--bright:#f2f2f7;--accent:#5b9bd5;--ok:#6fbf8b;--bad:#d9615f;--warm:#d98c5f}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--text);font:17px/1.45 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;-webkit-text-size-adjust:100%}
header{display:flex;justify-content:space-between;align-items:center;padding:14px 20px;border-bottom:1px solid var(--line)}
header b{font-size:19px;color:var(--bright);letter-spacing:.02em}
header span{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--dim)}
main{padding:24px 20px 40px;display:flex;flex-direction:column;gap:18px;max-width:560px;margin:0 auto}
h1{margin:0;font-size:30px;line-height:1.1;color:var(--bright);font-weight:650}
.cap{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:var(--dim)}
.dim{color:var(--dim)}
label{font-size:14px;color:var(--dim)}
input[type=text],input[inputmode]{width:100%;height:56px;padding:0 14px;background:#0f0f15;border:1px solid var(--line);color:var(--bright);font-size:22px;border-radius:2px}
input:focus{outline:none;border-color:var(--accent)}
input.code{text-align:center;letter-spacing:.35em;font-variant-numeric:tabular-nums;font-size:30px}
button{font:inherit;cursor:pointer;border-radius:2px}
.primary{height:56px;background:var(--accent);border:0;color:#10141c;font-weight:650;letter-spacing:.08em;text-transform:uppercase}
.choices{display:grid;gap:10px}
.choice{min-height:58px;padding:10px 14px;background:var(--panel);border:1px solid var(--line);color:var(--bright);font-size:19px;text-align:left}
.choice.on{border-color:var(--accent);background:#243249}
.choice.right{border-color:var(--ok);background:#1f3527}
.choice.wrong{border-color:var(--bad);background:#3a2224}
.readout{font-size:40px;font-weight:650;color:var(--bright);text-align:center;font-variant-numeric:tabular-nums;min-height:56px}
.slider{position:relative;padding:8px 0 34px}
input[type=range]{width:100%;height:44px;background:transparent;-webkit-appearance:none;appearance:none;margin:0}
input[type=range]::-webkit-slider-runnable-track{height:8px;background:#0f0f15;border:1px solid var(--line)}
input[type=range]::-moz-range-track{height:8px;background:#0f0f15;border:1px solid var(--line)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:34px;height:34px;margin-top:-14px;background:var(--accent);border:0;border-radius:50%}
input[type=range]::-moz-range-thumb{width:34px;height:34px;background:var(--accent);border:0;border-radius:50%}
.marks{position:absolute;left:17px;right:17px;bottom:6px;height:20px;font-size:12px;color:var(--dim)}
.marks span{position:absolute;transform:translateX(-50%);white-space:nowrap}
.zone{position:absolute;top:26px;height:12px;background:rgba(111,191,139,.45);border:1px solid var(--ok)}
.pin{position:absolute;top:18px;width:3px;height:28px;background:var(--warm)}
.box{padding:14px 16px;background:var(--panel);border:1px solid var(--line)}
.ok{color:var(--ok)} .bad{color:var(--bad)} .warm{color:var(--warm)}
.big{font-size:52px;font-weight:700;color:var(--bright);line-height:1}
.err{color:var(--bad);min-height:1.4em}
footer{font-size:13px;color:#6e6e82;text-align:center;padding:0 20px 24px}
</style></head>
<body>
<header><b>abcTrain</b><span id="status"></span></header>
<main id="app"></main>
<footer id="foot"></footer>
<script>
"use strict";
var D={"joinTitle": "Join the seminar", "yourName": "Your name", "roomCode": "Room code (on the screen)", "personalCode": "Your code", "listHint": "The code is on your card from the presenter.", "join": "Join", "hello": "Hi, {{name}}!", "waiting": "The presenter will start soon. Keep this page open.", "headphones": "The sound plays in the hall. Answer here: tap an option, or drag the scale and let go \u2014 you can change it until the answer is shown.", "round": "Round {{n}} of {{m}}", "tapHint": "Tap your answer. You can change it until the answer is shown.", "dragHint": "Drag and let go. You can change it until the answer is shown.", "sent": "Answer sent \u00b7 answered {{n}} of {{m}}", "answeredOf": "Answered {{n}} of {{m}}", "right": "Right!", "wrong": "Not this time", "noVote": "You did not answer this one", "answerWas": "The answer: {{answer}}", "yourVote": "Yours: {{answer}}", "points": "Points: {{n}}", "finished": "That's all!", "place": "Your place: {{place}} of {{count}}", "offline": "No connection to the presenter's computer \u2014 retrying\u2026", "footer": "abcTrain \u00b7 the room is served by the presenter's computer, nothing goes to the internet", "err.badCode": "No such code \u2014 check your card.", "err.badRoom": "Wrong room code \u2014 it is on the screen.", "err.nameNeeded": "Write your name.", "err.full": "The room is full.", "err.x": "Something went wrong \u2014 try again."};
var S=Object.assign({},D,/*STRINGS*/{});
function t(k,v){var s=S[k]||k;if(v)for(var n in v)s=s.split("{{"+n+"}}").join(v[n]);return s}
var params=new URLSearchParams(location.search);
var token=null;try{token=localStorage.getItem("abctrain.room."+location.port)}catch(e){}
var state=null,shownKey="",lastOk=Date.now(),sending=false,local={};
var app=document.getElementById("app");
function esc(s){return String(s==null?"":s).replace(/[&<>"']/g,function(c){return{"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c]})}
function save(tk){token=tk;try{localStorage.setItem("abctrain.room."+location.port,tk)}catch(e){}}
function api(method,path,body){
  return fetch(path,{method:method,headers:body?{"Content-Type":"application/json"}:{},body:body?JSON.stringify(body):undefined,cache:"no-store"})
    .then(function(r){return r.json().then(function(j){j.__status=r.status;return j})});
}
function poll(){
  api("GET","/api/state?t="+encodeURIComponent(token||"")).then(function(s){
    lastOk=Date.now();document.getElementById("status").textContent="";
    if(!s.joined&&token&&state&&state.joined){token=null}
    state=s;render();
  }).catch(function(){
    if(Date.now()-lastOk>3500)document.getElementById("status").textContent=t("offline");
  }).then(function(){setTimeout(poll,1000)});
}
function autoJoin(){
  var c=params.get("c"),r=params.get("r");
  if(token){api("POST","/api/join",{token:token}).then(function(j){if(j.token){save(j.token)}else{token=null}poll()}).catch(poll);return}
  if(c){api("POST","/api/join",{code:c}).then(function(j){if(j.token)save(j.token);poll()}).catch(poll);return}
  poll();
}
function render(){
  var s=state;if(!s)return;
  document.getElementById("foot").textContent=t("footer");
  if(!s.joined){renderJoin(s);return}
  var key=s.phase+":"+s.serial;
  if(s.phase==="question"){
    if(shownKey!==key){renderQuestion(s);shownKey=key}
    else updateQuestion(s);
    return;
  }
  if(shownKey===key&&s.phase!=="lobby")return;
  shownKey=key;
  if(s.phase==="lobby"){app.innerHTML='<span class="cap">'+esc(s.title)+'</span><h1>'+esc(t("hello",{name:s.name}))+'</h1><p class="dim">'+esc(t("waiting"))+'</p><div class="box">'+esc(t("headphones"))+'</div>';shownKey=key+":"+s.players;return}
  if(s.phase==="revealed")renderReveal(s);
  if(s.phase==="finished")app.innerHTML='<span class="cap">'+esc(s.title)+'</span><h1>'+esc(t("finished"))+'</h1><div class="big">'+esc(s.place)+'</div><p class="dim">'+esc(t("place",{place:s.place,count:s.players}))+'</p><p>'+esc(t("points",{n:s.score}))+'</p>';
}
function renderJoin(s){
  if(shownKey==="join")return;shownKey="join";
  var list=s.listOnly,room=params.get("r")||"";
  var h='<span class="cap">'+esc(s.title)+'</span><h1>'+esc(t("joinTitle"))+'</h1>';
  if(list){h+='<p class="dim">'+esc(t("listHint"))+'</p><label for="code">'+esc(t("personalCode"))+'</label><input id="code" class="code" inputmode="numeric" maxlength="4" autocomplete="one-time-code" value="'+esc(params.get("c")||"")+'">'}
  else{
    h+='<label for="name">'+esc(t("yourName"))+'</label><input id="name" type="text" maxlength="40" autocomplete="name">';
    h+='<div'+(room?' hidden':'')+'><label for="room">'+esc(t("roomCode"))+'</label><input id="room" class="code" inputmode="numeric" maxlength="4" value="'+esc(room)+'"></div>';
  }
  h+='<div class="err" id="err"></div><button class="primary" id="go">'+esc(t("join"))+'</button>';
  app.innerHTML=h;
  var first=document.getElementById(list?"code":"name");if(first)first.focus();
  document.getElementById("go").onclick=doJoin;
  app.querySelectorAll("input").forEach(function(i){i.onkeydown=function(e){if(e.key==="Enter")doJoin()}});
}
function doJoin(){
  if(sending)return;sending=true;
  var body={};var c=document.getElementById("code"),n=document.getElementById("name"),r=document.getElementById("room");
  if(c)body.code=c.value;if(n)body.name=n.value;if(r)body.room=r.value;
  api("POST","/api/join",body).then(function(j){
    sending=false;
    if(j.token){save(j.token);shownKey="";api("GET","/api/state?t="+encodeURIComponent(token)).then(function(s){state=s;render()});return}
    document.getElementById("err").textContent=t("err."+(j.error||"x"));
  }).catch(function(){sending=false;document.getElementById("err").textContent=t("offline")});
}
function head(s){
  var q=s.question;
  return '<span class="cap">'+esc(t("round",{n:q.round,m:q.total}))+' · '+esc(q.exercise)+'</span><h1>'+esc(q.prompt)+'</h1>';
}
function renderQuestion(s){
  var q=s.question;local.value=null;
  var h=head(s);
  if(q.continuous){
    h+='<div class="readout" id="readout">'+esc(s.vote!=null?q.labels[Math.round(s.vote*100)]:"")+'</div>';
    h+='<div class="slider"><input type="range" id="scale" min="0" max="1000" step="1" value="'+(s.vote!=null?Math.round(s.vote*1000):500)+'" aria-label="'+esc(t("dragHint"))+'"><div class="marks">';
    (q.marks||[]).forEach(function(m){h+='<span style="left:'+(m.at*100)+'%">'+esc(m.label)+'</span>'});
    h+='</div></div><p class="dim" id="hint">'+esc(t("dragHint"))+'</p>';
  }else{
    h+='<div class="choices">';
    q.choices.forEach(function(c,i){h+='<button class="choice'+(s.vote===i?' on':'')+'" data-i="'+i+'">'+esc(c)+'</button>'});
    h+='</div><p class="dim" id="hint">'+esc(t("tapHint"))+'</p>';
  }
  h+='<p class="dim" id="sent"></p>';
  app.innerHTML=h;
  if(q.continuous){
    var sc=document.getElementById("scale");
    sc.oninput=function(){document.getElementById("readout").textContent=q.labels[Math.round(sc.value/10)]};
    sc.onchange=function(){send({value:sc.value/1000})};
  }else{
    app.querySelectorAll(".choice").forEach(function(b){b.onclick=function(){
      app.querySelectorAll(".choice").forEach(function(x){x.classList.remove("on")});b.classList.add("on");send({choice:+b.dataset.i})}});
  }
  updateQuestion(s);
}
function updateQuestion(s){
  var el=document.getElementById("sent");if(!el)return;
  el.textContent=s.vote!=null?t("sent",{n:s.answered,m:s.players}):t("answeredOf",{n:s.answered,m:s.players});
}
function send(v){
  v.token=token;
  api("POST","/api/vote",v).then(function(s){if(s.question){state=s;updateQuestion(s)}}).catch(function(){
    var el=document.getElementById("sent");if(el)el.textContent=t("offline")});
}
function renderReveal(s){
  var q=s.question,a=s.answer;
  var h=head(s);
  if(!a.voted)h+='<div class="box warm">'+esc(t("noVote"))+'</div>';
  else h+='<div class="box '+(a.right?'ok':'bad')+'"><b>'+esc(a.right?t("right"):t("wrong"))+'</b></div>';
  h+='<p>'+esc(t("answerWas",{answer:a.label}))+'</p>';
  if(q.continuous){
    h+='<div class="slider" style="height:70px"><input type="range" min="0" max="1000" value="'+Math.round(a.value*1000)+'" disabled>';
    h+='<div class="marks">';(q.marks||[]).forEach(function(m){h+='<span style="left:'+(m.at*100)+'%">'+esc(m.label)+'</span>'});h+='</div></div>';
    if(s.vote!=null)h+='<p class="dim">'+esc(t("yourVote",{answer:q.labels[Math.round(s.vote*100)]}))+'</p>';
  }else{
    h+='<div class="choices">';
    q.choices.forEach(function(c,i){var cls=i===a.choice?' right':(s.vote===i?' wrong':'');h+='<button class="choice'+cls+'" disabled>'+esc(c)+'</button>'});
    h+='</div>';
  }
  h+='<p class="dim">'+esc(t("points",{n:s.score}))+'</p>';
  app.innerHTML=h;
}
autoJoin();
</script>
</body></html>
)PAGE");
    }
}
