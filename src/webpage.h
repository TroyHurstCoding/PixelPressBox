// webpage.h - the phone page served at http://pixelpressbox.local (one file: HTML + CSS + JS, kept in flash).
// It reads /config (the SD card's config.json), shows the common settings as dropdowns, switches and sliders, and
// posts the whole file back to /save. Anything it doesn't show is kept as it was; "Edit the settings file" under
// System is the escape hatch for the rest. Preview on a computer: python3 tools/preview_page.py
#pragma once
#include <Arduino.h>

static const char WEB_PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#0e1014">
<title>PixelPressbox</title>
<style>
:root{--bg:#0e1014;--card:#181b22;--line:#272b35;--tx:#eef0f4;--mut:#8b93a3;--acc:#ffc000;--ok:#2fbf71;--bad:#e5484d}
*{box-sizing:border-box}html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--tx);font:16px/1.4 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif;padding:0 0 calc(76px + env(safe-area-inset-bottom))}
header{position:sticky;top:0;z-index:5;background:rgba(14,16,20,.94);backdrop-filter:blur(8px);padding:14px 16px 10px;border-bottom:1px solid var(--line)}
h1{font-size:19px;margin:0;letter-spacing:.3px}h1 b{color:var(--acc)}
#st{color:var(--mut);font-size:13px;margin-top:3px;min-height:18px}
main{max-width:640px;margin:0 auto;padding:12px}
section{display:none}section.on{display:block}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px;margin-bottom:12px}
.card h2,summary{font-size:15px;font-weight:600;margin:0 0 10px}
details.card{padding:0}details.card>summary{padding:14px;margin:0;cursor:pointer;list-style:none;display:flex;justify-content:space-between;align-items:center}
details.card>summary::-webkit-details-marker{display:none}details.card>summary:after{content:"+";color:var(--mut);font-size:20px;line-height:1}
details[open].card>summary:after{content:"\2212"}details.card>.in{padding:0 14px 6px}
.f{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:10px 0;border-top:1px solid var(--line)}
.f>span{flex:1;min-width:0}.f small{display:block;color:var(--mut);font-size:12.5px;margin-top:2px}
select,input[type=text],input[type=number],input[type=password],textarea{background:#0c0e12;color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:9px 10px;font:inherit;max-width:100%}
select{min-width:128px}.f input[type=text],.f input[type=number]{width:150px}
input[type=range]{width:150px;accent-color:var(--acc)}
.sw{position:relative;width:48px;height:28px;flex:none}.sw input{opacity:0;width:100%;height:100%;margin:0;position:absolute;z-index:1;cursor:pointer}
.sw i{position:absolute;inset:0;background:#3a3f4b;border-radius:14px;transition:.15s}.sw i:after{content:"";position:absolute;width:22px;height:22px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.15s}
.sw input:checked+i{background:var(--ok)}.sw input:checked+i:after{left:23px}.sw input:focus-visible+i{outline:2px solid var(--acc)}
button,.btn{background:#262a34;color:var(--tx);border:1px solid var(--line);border-radius:10px;padding:11px 14px;font:inherit;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block}
button:active{transform:scale(.97)}button.p{background:var(--acc);border-color:var(--acc);color:#111}button.r{background:var(--bad);border-color:var(--bad)}
button.on{outline:2px solid var(--acc)}button:disabled{opacity:.5}
.g{display:grid;grid-template-columns:repeat(auto-fill,minmax(96px,1fr));gap:8px}.g2{display:grid;grid-template-columns:1fr 1fr;gap:8px}.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
.chips{display:flex;flex-wrap:wrap;gap:8px;padding:6px 0 10px}
.chip{position:relative}.chip input{position:absolute;opacity:0;inset:0;width:100%;height:100%;margin:0;cursor:pointer}
.chip span{display:block;padding:8px 12px;border-radius:20px;border:1px solid var(--line);color:var(--mut);font-size:14px}
.chip input:checked+span{background:rgba(255,192,0,.14);border-color:var(--acc);color:var(--tx)}.chip input:focus-visible+span{outline:2px solid var(--acc)}
.sub{color:var(--mut);font-size:12.5px;margin:10px 0 2px;text-transform:uppercase;letter-spacing:.6px}
.rowl{display:flex;gap:6px;padding:6px 0;align-items:center}.rowl input[type=text]{flex:1;min-width:0;width:auto}.rowl select{min-width:0}.rowl button{padding:8px 11px}
input[type=color]{width:40px;height:38px;padding:2px;border:1px solid var(--line);border-radius:10px;background:#0c0e12}
ul{list-style:none;margin:0;padding:0}li{display:flex;align-items:center;gap:10px;padding:9px 0;border-top:1px solid var(--line)}li span{flex:1;min-width:0;overflow-wrap:anywhere}li.d span{text-decoration:line-through;color:var(--mut)}
li input[type=checkbox]{width:22px;height:22px;accent-color:var(--ok)}li button{padding:6px 11px}
nav{position:fixed;left:0;right:0;bottom:0;z-index:6;display:flex;background:#12151b;border-top:1px solid var(--line);padding-bottom:env(safe-area-inset-bottom)}
nav button{flex:1;background:none;border:0;border-radius:0;color:var(--mut);padding:13px 4px;font-size:13px}nav button.on{color:var(--acc);outline:0;box-shadow:inset 0 2px var(--acc)}
#bar{position:fixed;left:0;right:0;bottom:calc(50px + env(safe-area-inset-bottom));z-index:6;padding:10px 12px;background:linear-gradient(transparent,var(--bg) 40%);display:none}
#bar button{width:100%;max-width:616px;display:block;margin:0 auto}
textarea{width:100%;height:46vh;font:13px ui-monospace,Menlo,monospace}
.mut{color:var(--mut);font-size:13px}
</style></head><body>
<header><h1>Pixel<b>Pressbox</b></h1><div id="st">Connecting...</div></header>
<main>
<section id="t0" class="on">
 <div class="card"><h2>On screen now</h2>
  <select id="pin" style="width:100%" aria-label="Screen to show"></select>
  <div class="g2" style="margin-top:8px"><button class="p" data-go="/next">Next screen</button><button data-go="/pin?k=__current__">Keep this one</button></div>
  <div class="g2" style="margin-top:8px"><button data-go="/pin?k=">Rotate all</button><button data-go="/favs">Favorites only on/off</button></div>
 </div>
 <div class="card"><h2>Focus timer</h2><div class="g3"><button data-go="/focus?m=25">25 min</button><button data-go="/focus?m=50">50 min</button><button data-go="/focus?m=0">Stop</button></div></div>
 <div class="card"><h2>Busy light</h2><div class="g3"><button data-go="/busy?m=0">Auto</button><button class="r" data-go="/busy?m=1">Busy</button><button data-go="/busy?m=2">Free</button></div></div>
 <div class="card"><h2>Games</h2><div class="g" id="games"></div>
  <div class="g2" style="margin-top:8px"><button data-go="/game?g=99">Games menu</button><button data-go="/game?g=-1">Stop game</button></div>
  <p class="mut">Tilt to play, tap the case for actions, double tap to quit. Shake the clock to open the menu.</p></div>
 <div class="card"><h2>Sounds</h2><div class="g" id="snd"><span class="mut">No sounds on the card</span></div></div>
</section>
<section id="t1">
 <div class="card"><h2>To-do</h2>
  <form id="tf" class="rowl"><input type="text" id="nt" placeholder="Add a to-do" maxlength="80" aria-label="New to-do"><button class="p">Add</button></form>
  <ul id="tl"></ul><button data-go="/todo/clear" style="margin-top:10px">Clear finished</button></div>
</section>
<section id="t2"><div id="form"><div class="card mut">Loading settings...</div></div></section>
<section id="t3">
 <div class="card"><h2>WiFi</h2><p class="mut" id="wc"></p><ul id="wl"></ul>
  <div class="sub">Add another network</div>
  <form id="wf"><div class="rowl"><input type="text" id="ws" placeholder="Network name" maxlength="32" autocapitalize="off" autocorrect="off" aria-label="Network name"></div>
  <div class="rowl"><input type="password" id="wp" placeholder="Password" maxlength="64" style="flex:1" aria-label="Password"><button class="p">Save</button></div></form>
  <p class="mut">The clock remembers up to 8 networks and joins whichever is in range, strongest first.</p></div>
 <div class="card"><h2>Motion sensor</h2><p class="mut" id="mo">Checking...</p>
  <div id="mv" style="display:none"><div class="f"><span>The clock thinks it is</span><b id="m1"></b></div>
  <div class="f"><span>Gravity on screen<small>right, down, toward you</small></span><span id="m2" style="flex:none"></span></div>
  <div class="f"><span>Sensor reading<small>x, y, z as the chip reports them</small></span><span id="m3" style="flex:none"></span></div>
  <div class="sub">Calibrate the mounting</div>
  <p class="mut">Do this once if the screen turns the wrong way. 1: stand the clock straight upright, wide, and press the first button. 2: lay it on its back, screen facing up, and press the second.</p>
  <div class="g2"><button data-cal="1">1. Standing upright</button><button data-cal="2">2. On its back</button></div>
  <button data-cal="0" style="margin-top:8px;width:100%">Forget calibration</button></div></div>
 <div class="card"><h2>Outlook calendar</h2><p class="mut" id="ms"></p><div class="g2"><button data-go="/ms/login">Sign in</button><button data-go="/ms/logout">Sign out</button></div></div>
 <div class="card"><h2>Firmware</h2><p class="mut" id="fw"></p>
  <div class="g2"><button id="cu">Check for update</button><a class="btn" href="/update" style="text-align:center">Upload a file</a></div>
  <button data-go="/restart" style="margin-top:8px;width:100%">Restart the clock</button></div>
 <details class="card"><summary>Edit the settings file</summary><div class="in"><p class="mut">Everything, including the rarely changed options. Saving restarts the clock.</p>
  <textarea id="raw" spellcheck="false" aria-label="Settings file"></textarea><button class="p" id="rs" style="margin:8px 0 10px">Save file &amp; restart</button></div></details>
</section>
</main>
<div id="bar"><button class="p" id="sv">Save &amp; restart</button></div>
<nav><button class="on">Remote</button><button>To-do</button><button>Settings</button><button>System</button></nav>
<script>
const $=i=>document.getElementById(i),E=(t,p,...k)=>{const e=Object.assign(document.createElement(t),p||{});k.forEach(c=>e.append(c));return e};
let cfg=null,tab=0;
const H={};for(let h=0;h<24;h++)H[h]=(h%12||12)+(h<12?' AM':' PM');
const TZ={'PST8PDT,M3.2.0,M11.1.0':'Pacific','MST7MDT,M3.2.0,M11.1.0':'Mountain','MST7':'Arizona','CST6CDT,M3.2.0,M11.1.0':'Central','EST5EDT,M3.2.0,M11.1.0':'Eastern','AKST9AKDT,M3.2.0,M11.1.0':'Alaska','HST10':'Hawaii','GMT0BST,M3.5.0/1,M10.5.0':'UK','CET-1CEST,M3.5.0,M10.5.0/3':'Central Europe','IST-5:30':'India','JST-9':'Japan','AEST-10AEDT,M10.1.0,M4.1.0/3':'Sydney'};
const LG={nfl:'NFL',nba:'NBA',mlb:'MLB'};
// [path, label, type, options/min, max, hint]
const S=[
['Clock & display',[
 ['clock.portrait_style','Portrait clock face','sel',{stacked:'Stacked digits',analog:'Analog'}],
 ['clock.style','Clock face','sel',{classic:'Classic',big:'Big digits',nixie:'Nixie tubes',words:'Words',tetris:'Tetris'}],
 ['twelve_hour','Time format','sel',{true:'12-hour',false:'24-hour'}],
 ['timezone','Time zone','sel',TZ],
 ['brightness','Brightness','rng',10,255],
 ['night.start','Dim at night from','sel',H],['night.end','Back to full at','sel',H],
 ['night.brightness','Night brightness','rng',0,120],
 ['clock.dwell','Clock stays up for','sel',{10:'10 seconds',20:'20 seconds',30:'30 seconds',60:'1 minute',120:'2 minutes'}],
 ['card_dwell','Other screens stay for','sel',{5:'5 seconds',7:'7 seconds',10:'10 seconds',15:'15 seconds',30:'30 seconds'}],
 ['clock.every','Show the clock','sel',{1:'After every screen',2:'Every 2 screens',3:'Every 3 screens',5:'Every 5 screens'}]]],
['Weather',[
 ['weather.units','Temperature','sel',{F:'Fahrenheit',C:'Celsius'}],
 ['weather.lat','Latitude','num',0,0,'Your town, e.g. 34.05'],['weather.lon','Longitude','num',0,0,'West is negative, e.g. -118.24']]],
['Sports',[
 ['sports','Show games','sel',{all:'All chosen leagues',favorites:'My teams only',off:'No sports'}],
 ['favorites.nfl','My NFL teams','lst',0,0,'Abbreviations, e.g. LV, KC'],['favorites.nba','My NBA teams','lst'],['favorites.mlb','My MLB teams','lst'],
 ['show_all','Show every game in','lgs'],['celebrate','Celebrate my teams scoring in','lgs'],
 ['ufc.show','Next UFC event','sw'],['cards.standings','Standings','sw'],
 ['fantasy_players','Stat lines to follow (any league)','fan']]],
['Sound',[
 ['sound.enabled','Sound','sw'],['sound.volume','Volume','rng',0,21],['sound.quiet_at_night','Quiet at night','sw'],
 ['mic.clap_control','Clap twice for the next screen','sw',0,0,'Uses the built-in microphones'],
 ['mic.sensitivity','Microphone sensitivity','sel',{low:'Low',medium:'Medium',high:'High',max:'Very high'}]]],
['Fantasy football',[['fantasy_points','','fpts']]],
['Work & focus',[
 ['outlook.busy_light','Busy light during meetings','sw'],['outlook.teams_presence','Use Teams status','sw'],['outlook.flagged_email','Flagged emails as to-dos','sw'],
 ['outlook.sync_tasks','Sync Microsoft To Do','sw'],
 ['outlook.chime_minutes','Meeting chime','sel',{0:'Off',1:'1 minute before',2:'2 minutes before',5:'5 minutes before',10:'10 minutes before'}],
 ['focus.minutes','Focus session','sel',{15:'15 minutes',25:'25 minutes',45:'45 minutes',50:'50 minutes',60:'1 hour'}],
 ['focus.break','Break after','sel',{0:'No break',5:'5 minutes',10:'10 minutes',15:'15 minutes'}]]],
['Screens in the rotation',[
 ['cards','','chips',[['Info',{weather_details:'Weather',sun_moon:'Sun & moon',date_progress:'Year progress',on_this_day:'On this day',word_of_the_day:'Word of the day',dad_jokes:'Dad jokes',trivia:'Trivia',facts_quotes:'Facts & quotes',billboard:'Billboard #1s',national_day:'National day',hacker_news:'Hacker News',surf:'Surf report',iss_map:'Space station',planets:'Planets',fly_the_w:'Win flag'}],
  ['Clocks',{nixie_clock:'Nixie',big_clock:'Big digits',word_clock:'Words',tetris_clock:'Tetris',portrait_clock:'Tall clock',binary_clock:'Binary',sun_clock:'Sun arc',city_clock:'City skyline',world_clock:'World clock',sand_clock:'Sand'}],
  ['Sports',{win_probability:'Win probability',top_performers:'Top performers',next_game:'Next game',golf:'Golf leaderboard',fantasy_points:'Fantasy points'}],
  ['Ambient',{fireplace:'Fireplace',digital_rain:'Digital rain',aquarium:'Aquarium',bouncing_logo:'Bouncing logo',snake:'Snake',breakout:'Breakout',double_pendulum:'Pendulum',newtons_cradle:"Newton's cradle",water:'Water',images:'My pictures',beer:'Beer glass',hourglass:'Hourglass',lava_lamp:'Lava lamp',game_of_life:'Game of Life',visualizer:'Sound visualizer',planes:'Planes overhead'}]]],
 ['cards.bounce_text','Bouncing logo text','txt'],
 ['world_clocks','World clock cities','wcs'],
 ['planes.radius_km','Planes within','sel',{15:'15 km',25:'25 km',40:'40 km',60:'60 km'},0,'Around your weather location; from OpenSky Network'],['cards.rain_word','Word hidden in the rain','txt'],
 ['holidays.show','Holidays','sel',{all:'All holidays',federal:'Federal only',off:'Off'}]]],
['Countdowns',[['countdowns','','cds']]],
['Motion sensor',[
 ['motion.mount','Where it is mounted','sel',{panel:'On the LED panel',fin:'On the back cover'}],
 ['motion.orientation','Screen direction','sel',{auto:'Automatic',landscape:'Always wide',portrait:'Always tall'}],
 ['motion.taps','Tap the case to change screens','sw'],
 ['motion.tap_force','Tap strength needed','sel',{verylight:'Very light',light:'Light',firm:'Firm (hard knock)'}],['motion.flat_sleep','Screen off when lying flat','sw',0,0,'Only after calibrating under System'],
 ['motion.mirror','Flip left and right','sw',0,0,'If tall mode shows upside down'],['motion.pitch_invert','Flip lean in games','sw']]],
['Updates & display panel',[
 ['auto_update.enabled','Update automatically','sw'],['auto_update.hour','Check at','sel',H],
 ['panel.driver','Panel chip','sel',{SHIFTREG:'Standard',FM6126A:'FM6126A',ICN2038S:'ICN2038S',FM6124:'FM6124',MBI5124:'MBI5124'},0,'Change only if the screen is blank or garbled'],
 ['panel.flip','Turn the picture upside down','sw',0,0,'If the LED panel is installed the other way up'],
 ['panel.swap_green_blue','Swap green and blue','sw']]]];
const get=p=>p.split('.').reduce((o,k)=>o==null?o:o[k],cfg);
const put=(p,v)=>{const k=p.split('.'),l=k.pop();let o=cfg;k.forEach(x=>{if(typeof o[x]!='object'||o[x]==null)o[x]={};o=o[x]});o[l]=v};
const set=(p,v)=>{put(p,v);dirty()};
const dirty=()=>{$('bar').style.display=tab==2?'block':'none';$('sv').dataset.d=1};
function sw(on,fn,label){const i=E('input',{type:'checkbox',checked:!!on});i.setAttribute('aria-label',label);i.onchange=()=>fn(i.checked);return E('label',{className:'sw'},i,E('i'))}
function row(label,hint,ctl){const s=E('span',{},label);if(hint)s.append(E('small',{},hint));return E('div',{className:'f'},s,ctl)}
function field(d){const[p,label,t,a,b,hint]=d,v=get(p);
 if(t=='sel'){const s=E('select');s.setAttribute('aria-label',label);const o=Object.assign({},a),sv=String(v),ks=Object.keys(a);if(v!=null&&!(sv in o))o[sv]='Custom ('+sv+')';
  const kind=ks.every(k=>k=='true'||k=='false')?0:ks.every(k=>!isNaN(+k))?1:2;
  if(v==null)s.append(E('option',{value:'',textContent:'Default',selected:true}));
  for(const k in o)s.append(E('option',{value:k,textContent:o[k],selected:v!=null&&k==sv}));
  s.onchange=()=>{if(s.value!=='')set(p,kind==0?s.value=='true':kind==1?+s.value:s.value)};return row(label,hint,s)}
 if(t=='sw')return row(label,hint,sw(v,x=>set(p,x),label));
 if(t=='rng'){const r=E('input',{type:'range',min:a,max:b,value:v==null?a:v});r.setAttribute('aria-label',label);r.oninput=()=>set(p,+r.value);return row(label,hint,r)}
 if(t=='num'){const n=E('input',{type:'number',step:'any',value:v==null?'':v});n.setAttribute('aria-label',label);n.onchange=()=>set(p,+n.value||0);return row(label,hint,n)}
 if(t=='txt'){const n=E('input',{type:'text',value:v||'',maxLength:12});n.setAttribute('aria-label',label);n.onchange=()=>set(p,n.value);return row(label,hint,n)}
 if(t=='lst'){const n=E('input',{type:'text',value:(v||[]).join(', '),autocapitalize:'characters'});n.setAttribute('aria-label',label);n.onchange=()=>set(p,n.value.toUpperCase().split(/[\s,]+/).filter(x=>x));return row(label,hint,n)}
 if(t=='lgs'){let o=v;if(Array.isArray(o)){o={};v.forEach(x=>o[x]=true);put(p,o)}
  const c=E('div',{className:'chips'});for(const k in LG)c.append(chip(LG[k],(o||{})[k],x=>{const n=Object.assign({},get(p));n[k]=x;set(p,n)}));
  const w=E('div',{className:'f',style:'display:block'},E('span',{},label));w.append(c);return w}
 if(t=='chips'){const w=E('div');a.forEach(([name,m])=>{w.append(E('div',{className:'sub'},name));const c=E('div',{className:'chips'});
  for(const k in m)c.append(chip(m[k],get('cards.'+k)!==false,x=>set('cards.'+k,x)));w.append(c)});return w}
 if(t=='cds')return listEd(p,()=>({name:'',date:'',color:'#FFC000'}),(r,i,up)=>[tin(r.name,'Name',x=>up('name',x)),tin(r.date,'2026-12-25 or 12-25',x=>up('date',x)),
  Object.assign(E('input',{type:'color',value:/^#[0-9a-f]{6}$/i.test(r.color||'')?r.color:'#ffc000'}),{onchange(){up('color',this.value.toUpperCase())},title:'Color'})],'Add a countdown','Dates with no year repeat every year.');
 if(t=='fan'){const w=E('div',{className:'f',style:'display:block'},E('span',{},label));w.append(listEd(p,()=>({name:'',league:'nfl',team:''}),(r,i,up)=>{
  const s=E('select');for(const k in LG)s.append(E('option',{value:k,textContent:LG[k],selected:k==r.league}));s.onchange=()=>up('league',s.value);s.setAttribute('aria-label','League');
  const tm=tin(r.team,'Team',x=>up('team',x.toUpperCase()));tm.style.flex='0 0 70px';return[tin(r.name,'Player name',x=>up('name',x)),s,tm]},'Add a player'));return w}
 if(t=='wcs')return listEd(p,()=>({name:'',utc:0,dst:''}),(r,i,up)=>{const n=tin(r.name,'City',x=>up('name',x.toUpperCase()));n.style.flex='1';
  const o=E('input',{type:'number',step:'0.25',value:r.utc==null?0:r.utc});o.setAttribute('aria-label','Hours from UTC');o.style.flex='0 0 74px';o.title='Hours from UTC';o.onchange=()=>up('utc',+o.value||0);
  const d=E('select');d.setAttribute('aria-label','Daylight saving');for(const[k,v]of[['','No DST'],['us','US DST'],['eu','EU DST'],['au','AU DST']])d.append(E('option',{value:k,textContent:v,selected:(r.dst||'')==k}));d.style.minWidth='0';d.onchange=()=>up('dst',d.value);return[n,o,d]},'Add a city','Hours from UTC in standard time, e.g. -5 for New York, 5.5 for Mumbai. Up to 4 show.');
 if(t=='fpts')return fptsEd(p);
 return E('div')}
// ---- fantasy football: players to track (searched on ESPN from the phone), team names, week and the league's scoring
const FDEF={passYds:.04,passTD:5,passInt:-2,rushYds:.1,rushTD:6,recYds:.1,recTD:6,rec:.5,fumLost:-2,fgMade:3,xpMade:1,fgMiss:-2,xpMiss:-1,
 kickRetTD:6,puntRetTD:6,defTD:6,sack:1,intDef:1,fumRecovered:1,tackleSolo:0,tackleTotal:0,passDefended:0,
 dstSack:1,dstInt:2,dstFumRec:2,dstTD:6,dstSafety:2,dstRetTD:0,pass400:2,rush150:2,rec150:2};
const FLBL={passYds:'Passing yard',passTD:'Passing TD',passInt:'Interception thrown',rushYds:'Rushing yard',rushTD:'Rushing TD',recYds:'Receiving yard',recTD:'Receiving TD',
 rec:'Reception',fumLost:'Fumble lost',fgMade:'Field goal',xpMade:'Extra point',fgMiss:'Missed FG',xpMiss:'Missed XP',kickRetTD:'Kick return TD',puntRetTD:'Punt return TD',
 defTD:'Defensive TD (player)',sack:'Sack (player)',intDef:'Interception (player)',fumRecovered:'Fumble recovery (player)',tackleSolo:'Solo tackle',tackleTotal:'Tackle',passDefended:'Pass defended',
 dstSack:'D/ST sack',dstInt:'D/ST interception',dstFumRec:'D/ST fumble recovery',dstTD:'D/ST touchdown',dstSafety:'D/ST safety',dstRetTD:'D/ST return TD',
 pass400:'Bonus: 400 passing yards',rush150:'Bonus: 150 rushing yards',rec150:'Bonus: 150 receiving yards'};
const FPA=[[0,10],[6,7],[13,4],[20,1],[27,0],[34,-1],[999,-4]];
const NFL='ARI Cardinals Arizona|ATL Falcons Atlanta|BAL Ravens Baltimore|BUF Bills Buffalo|CAR Panthers Carolina|CHI Bears Chicago|CIN Bengals Cincinnati|CLE Browns Cleveland|DAL Cowboys Dallas|DEN Broncos Denver|DET Lions Detroit|GB Packers GreenBay|HOU Texans Houston|IND Colts Indianapolis|JAX Jaguars Jacksonville|KC Chiefs KansasCity|LV Raiders LasVegas|LAC Chargers|LAR Rams|MIA Dolphins Miami|MIN Vikings Minnesota|NE Patriots NewEngland|NO Saints NewOrleans|NYG Giants|NYJ Jets|PHI Eagles Philadelphia|PIT Steelers Pittsburgh|SF 49ers SanFrancisco|SEA Seahawks Seattle|TB Buccaneers TampaBay|TEN Titans Tennessee|WSH Commanders Washington'.split('|').map(x=>x.split(' '));
const fnorm=s=>String(s||'').toLowerCase().normalize('NFD').replace(/[̀-ͯ]/g,'').replace(/\b(jr|sr|ii|iii|iv|v)\b/g,'').replace(/[^a-z0-9]/g,'');
const isDst=p=>/^(DEF|DST|D\/ST|D)$/i.test(String(p||'').trim());
function dstTeam(name,team){const k=[fnorm(team),fnorm(name)];return NFL.find(t=>t.some(w=>k.includes(fnorm(w))))}
function fsearch(q){return fetch('https://site.web.api.espn.com/apis/common/v3/search?type=player&sport=football&league=nfl&limit=10&query='+encodeURIComponent(q))
 .then(r=>r.json()).then(d=>(d.items||[]).filter(i=>i.league=='nfl'&&i.isActive!==false).map(i=>({id:String(i.id),name:i.displayName,pos:(i.position&&i.position.abbreviation)||'',
 team:((i.teamRelationships||[])[0]||{}).core?((i.teamRelationships[0].core.abbreviation)||''):''})))}
function fptsEd(p,prevLog){const w=E('div');const fp=()=>{const o=get(p);return o&&typeof o=='object'?o:{}};
 const upd=(k,v)=>{const o=Object.assign({},fp());o[k]=v;set(p,o)};const players=()=>Array.isArray(fp().players)?fp().players:[];
 const msg=E('p',{className:'mut'});
 const add=(pl,side)=>{if(players().some(x=>x.side==side&&(x.id?x.id==pl.id:x.pos=='DST'&&x.team==pl.team)))return msg.textContent=pl.name+' is already on that team.';
  upd('players',players().concat([Object.assign({},pl,{side})]));list();msg.textContent='Added '+pl.name+'.'};
 // names, week
 w.append(row('My team','',tin(fp().me||'','My team',x=>upd('me',x))),row('Opponent','',tin(fp().opp||'','Opponent',x=>upd('opp',x))));
 const wk=E('select');wk.setAttribute('aria-label','Week');wk.append(E('option',{value:0,textContent:'This week'}));for(let i=1;i<=18;i++)wk.append(E('option',{value:i,textContent:'Week '+i}));
 wk.value=fp().week||0;wk.onchange=()=>upd('week',+wk.value);w.append(row('Week','The clock follows ESPN\'s current week unless you pick one',wk));
 // search
 w.append(E('div',{className:'sub'},'Add players'));
 const q=E('input',{type:'text',placeholder:'Player name',autocapitalize:'words'});q.setAttribute('aria-label','Player name');q.style.flex='1';
 const res=E('div');const go2=()=>{const v=q.value.trim();if(!v)return;res.textContent='Searching...';fsearch(v).then(l=>{res.textContent='';if(!l.length)res.textContent='No NFL player by that name.';
  l.forEach(pl=>res.append(E('div',{className:'rowl'},E('span',{style:'flex:1'},pl.name+' ',E('small',{className:'mut'},pl.pos+' '+pl.team)),
   E('button',{textContent:'+ Mine',onclick:()=>add(pl,'me')}),E('button',{textContent:'+ Theirs',onclick:()=>add(pl,'opp')}))))}).catch(()=>res.textContent='ESPN search did not answer. Check the phone\'s internet.')};
 const sb=E('button',{textContent:'Search',onclick:go2});q.onkeydown=e=>{if(e.key=='Enter'){e.preventDefault();go2()}};
 w.append(E('div',{className:'rowl'},q,sb),res);
 const ds=E('select');ds.setAttribute('aria-label','Team defence');NFL.forEach(t=>ds.append(E('option',{value:t[0],textContent:t[0]+' '+t[1]})));
 const dpl=()=>{const t=NFL.find(x=>x[0]==ds.value);return{id:'',name:t[0]+' D/ST',pos:'DST',team:t[0]}};
 w.append(E('div',{className:'rowl'},E('span',{style:'flex:1'},'Team defence'),ds,E('button',{textContent:'+ Mine',onclick:()=>add(dpl(),'me')}),E('button',{textContent:'+ Theirs',onclick:()=>add(dpl(),'opp')})));
 // import
 const ta=E('textarea',{placeholder:'[me] My Team\nJosh Allen, QB, BUF\nLions, DEF, DET\n\n[opp] Their Team\nTravis Kelce, TE, KC',rows:6});ta.setAttribute('aria-label','Roster to import');
 const log=E('pre',{className:'mut',style:'white-space:pre-wrap;font-size:12.5px;margin:6px 0'},prevLog||'');
 const imp=E('button',{textContent:'Import roster',onclick:async()=>{imp.disabled=true;log.textContent='';let side='me',out=[],bad=0;const o=Object.assign({},fp());
  for(const raw of ta.value.split('\n')){const line=raw.trim();if(!line||line[0]=='#')continue;const sec=line.match(/^\[(me|opp)\]\s*(.*)$/i);
   if(sec){side=sec[1].toLowerCase();if(sec[2])o[side=='me'?'me':'opp']=sec[2].trim();continue}
   const[name,pos,team]=line.split(',').map(x=>(x||'').trim());if(!name)continue;
   if(isDst(pos)){const t=dstTeam(name,team);if(t){out.push({id:'',name:t[0]+' D/ST',pos:'DST',team:t[0],side});log.textContent+='ok     '+name+' -> '+t[0]+' D/ST\n'}else{bad++;log.textContent+='MISS   '+name+' (no NFL team by that name)\n'}continue}
   let hits=[];try{hits=await fsearch(name)}catch(e){}
   if(team)hits=hits.filter(h=>h.team.toUpperCase()==team.toUpperCase());
   if(pos&&hits.length>1)hits=hits.filter(h=>h.pos.toUpperCase()==pos.toUpperCase()||(pos.toUpperCase()=='K'&&h.pos=='PK'));
   if(hits.length>1){const ex=hits.filter(h=>fnorm(h.name)==fnorm(name));if(ex.length)hits=ex}
   if(hits.length==1){out.push(Object.assign({},hits[0],{side}));log.textContent+='ok     '+name+' -> '+hits[0].name+' ('+hits[0].pos+' '+hits[0].team+')\n'}
   else{bad++;log.textContent+=(hits.length?'AMBIG  ':'MISS   ')+name+(hits.length?' -> '+hits.map(h=>h.pos+' '+h.team).join(', '):'')+'\n'}}
  o.players=out;set(p,o);w.replaceWith(fptsEd(p,log.textContent+(bad?'\n'+bad+' need help: add a position and team, e.g. James Cook, RB, BUF, then import again.':'\nEvery name found. Save to send it to the clock.')));
  }});
 w.append(E('div',{className:'sub'},'Or paste a whole roster'),E('p',{className:'mut'},'One player per line: name, then position and team if the name is common. Sections [me] and [opp]. OctoBoxVR\'s roster.txt works as is. Importing replaces the lists below.'),ta,imp,log);
 // the lists
 const lists=E('div');w.append(lists,msg);
 function list(){lists.textContent='';const l=players();for(const side of['me','opp']){lists.append(E('div',{className:'sub'},side=='me'?(fp().me||'My team'):(fp().opp||'Opponent')));
  const mine=l.map((x,i)=>[x,i]).filter(([x])=>(x.side||'me')==side);if(!mine.length)lists.append(E('p',{className:'mut'},'Nobody yet.'));
  mine.forEach(([x,i])=>{const sw2=E('button',{textContent:'⇄',title:'Move to the other team',onclick:()=>{const n=l.slice();n[i]=Object.assign({},x,{side:side=='me'?'opp':'me'});upd('players',n);list()}});
   const del=E('button',{textContent:'×',title:'Remove',onclick:()=>{const n=l.slice();n.splice(i,1);upd('players',n);list()}});sw2.setAttribute('aria-label','Move');del.setAttribute('aria-label','Remove');
   lists.append(E('div',{className:'rowl'},E('span',{style:'flex:1'},x.name+' ',E('small',{className:'mut'},x.pos+' '+x.team)),sw2,del))})}}
 list();
 // scoring
 const sc=E('details',{},E('summary',{},'Scoring rules'));const sd=E('div');sc.append(sd);
 const rules=()=>Object.assign({},FDEF,fp().scoring||{});
 const draw=()=>{sd.textContent='';const r=rules();for(const k in FLBL){const n=E('input',{type:'number',step:'any',value:r[k]});n.setAttribute('aria-label',FLBL[k]);n.style.width='90px';
   n.onchange=()=>{const o=Object.assign({},rules());o[k]=+n.value||0;upd('scoring',o)};sd.append(row(FLBL[k],'',n))}
  const pa=E('input',{type:'text',value:(fp().dst_points_allowed||FPA).map(t=>t[0]+':'+t[1]).join(', ')});pa.setAttribute('aria-label','D/ST points allowed');pa.style.width='100%';
  pa.onchange=()=>{const t=pa.value.split(',').map(x=>x.split(':').map(Number)).filter(x=>x.length==2&&!x.some(isNaN));if(t.length)upd('dst_points_allowed',t)};
  sd.append(E('div',{className:'f',style:'display:block'},E('span',{},'D/ST points allowed',E('small',{},'max points allowed : fantasy points, first match wins')),pa));
  sd.append(E('button',{textContent:'Reset to my league\'s rules',style:'margin:8px 0',onclick:()=>{const o=Object.assign({},fp());o.scoring=Object.assign({},FDEF);o.dst_points_allowed=FPA;set(p,o);draw()}}))};
 draw();w.append(sc);
 w.append(E('p',{className:'mut'},'Points use ESPN\'s live box scores. Field goals score a flat amount (ESPN does not give each kick\'s distance) and 2-point conversions are not counted.'));
 return w}
function tin(v,ph,fn){const i=E('input',{type:'text',value:v||'',placeholder:ph});i.setAttribute('aria-label',ph);i.onchange=()=>fn(i.value);return i}
function chip(label,on,fn){const i=E('input',{type:'checkbox',checked:!!on});i.onchange=()=>fn(i.checked);return E('label',{className:'chip'},i,E('span',{},label))}
function listEd(p,blank,cells,addText,note){const w=E('div');const draw=()=>{w.textContent='';const l=Array.isArray(get(p))?get(p):[];
 l.forEach((r,i)=>{const x=E('button',{textContent:'×',title:'Remove'});x.setAttribute('aria-label','Remove');x.onclick=()=>{const n=l.slice();n.splice(i,1);set(p,n);draw()};
  w.append(E('div',{className:'rowl'},...cells(r,i,(k,v)=>{const n=l.slice();n[i]=Object.assign({},r,{[k]:v});set(p,n);r=n[i]}),x))});
 const a=E('button',{textContent:addText});a.style.margin='6px 0 10px';a.onclick=()=>{set(p,l.concat([blank()]));draw()};w.append(a);if(note)w.append(E('p',{className:'mut'},note))};draw();return w}
function build(){const f=$('form');f.textContent='';S.forEach(([name,fs],n)=>{const d=E('details',{className:'card',open:n==0},E('summary',{},name)),i=E('div',{className:'in'});
 fs.forEach(x=>i.append(field(x)));d.append(i);f.append(d)});$('raw').value=JSON.stringify(cfg,null,1)}
function loadCfg(){return fetch('/config').then(r=>r.json()).then(c=>{cfg=c;build()}).catch(()=>{$('form').innerHTML='<div class="card mut">The settings file on the card could not be read. Fix it under System, Edit the settings file.</div>';fetch('/config').then(r=>r.text()).then(t=>$('raw').value=t)})}
function save(text){$('sv').disabled=$('rs').disabled=true;return fetch('/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'cfg='+encodeURIComponent(text)})
 .then(r=>{if(!r.ok)throw 0;$('st').textContent='Saved. The clock is restarting...';$('bar').style.display='none';delete $('sv').dataset.d;setTimeout(()=>location.reload(),9000)})
 .catch(()=>{alert('Not saved. Check the settings and try again.');$('sv').disabled=$('rs').disabled=false})}
$('sv').onclick=()=>save(JSON.stringify(cfg,null,1));
$('rs').onclick=()=>{try{JSON.parse($('raw').value)}catch(e){alert('That file has a mistake in it: '+e.message);return}save($('raw').value)};
const go=u=>fetch(u).then(()=>setTimeout(status,600));
document.addEventListener('click',e=>{const b=e.target.closest('[data-go]');if(!b)return;
 if(b.dataset.go=='/restart'&&!confirm('Restart the clock?'))return;go(b.dataset.go).then(()=>{if(b.dataset.go.startsWith('/todo'))todo()})});
document.querySelectorAll('nav button').forEach((b,i)=>b.onclick=()=>{tab=i;document.querySelectorAll('nav button').forEach((x,j)=>x.classList.toggle('on',i==j));
 document.querySelectorAll('section').forEach((x,j)=>x.classList.toggle('on',i==j));$('bar').style.display=i==2&&$('sv').dataset.d?'block':'none';scrollTo(0,0);if(i==1)todo();if(i==3){wifi();mot()}});
['Dodger','TiltyBird','MarbleRun','Catch','Breakout','Racer','Balance','Pong','Simon','Lander','Reaction'].forEach((g,i)=>$('games').append(E('button',{textContent:g,onclick:()=>go('/game?g='+i)})));
$('pin').onchange=function(){go('/pin?k='+encodeURIComponent(this.value))};
function todo(){fetch('/todo').then(r=>r.json()).then(l=>{const u=$('tl');u.textContent='';l.forEach((x,i)=>{
 if(x.source==='microsoft'){u.append(E('li',{},E('span',{},'MS To Do · '+x.t)));return}
 const c=E('input',{type:'checkbox',checked:!!x.d});c.setAttribute('aria-label','Done');c.onchange=()=>fetch('/todo/done?i='+i).then(todo);
 const b=E('button',{textContent:'×'});b.setAttribute('aria-label','Delete');b.onclick=()=>fetch('/todo/del?i='+i).then(todo);u.append(E('li',{className:x.d?'d':''},c,E('span',{},x.t),b))});
 if(!l.length)u.append(E('li',{className:'mut'},'Nothing yet'))})}
$('tf').onsubmit=e=>{e.preventDefault();const v=$('nt').value.trim();if(v)fetch('/todo/add?t='+encodeURIComponent(v)).then(()=>{$('nt').value='';todo()})};
function status(){fetch('/status').then(r=>r.json()).then(s=>{$('st').textContent=s.line||'Connected';$('ms').textContent=s.ms||'';if(!upd)$('fw').textContent='Version '+s.fw}).catch(()=>{$('st').textContent='Clock not reachable'});
 fetch('/cards').then(r=>r.json()).then(c=>{const p=$('pin');if(document.activeElement===p)return;p.textContent='';p.append(E('option',{value:'',textContent:'Rotate through everything'}));
  c.cards.forEach(x=>p.append(E('option',{value:x.k,textContent:'Only: '+x.l,selected:x.k==c.pinned})))}).catch(()=>{})}
function wifi(){fetch('/wifi').then(r=>r.json()).then(w=>{$('wc').textContent=w.current?'Connected to '+w.current:'Not connected';const u=$('wl');u.textContent='';
 w.saved.forEach(s=>{const b=E('button',{textContent:'Forget'});b.onclick=()=>{if(confirm('Forget '+s+'?'))fetch('/wifi/del?s='+encodeURIComponent(s)).then(wifi)};
  u.append(E('li',{},E('span',{},s+(s==w.current?' (in use)':'')),b))})}).catch(()=>{})}
const OR=['Upright, wide','Standing on its left edge','Upside down','Standing on its right edge'],f2=a=>a.map(v=>(v<0?'':'+')+v.toFixed(2)).join('  ');
function mot(){if(tab!=3)return;fetch('/motion').then(r=>r.json()).then(m=>{if(!m.present){$('mo').textContent='No motion sensor found. Check the four wires, then restart the clock.';$('mv').style.display='none';return}
 $('mv').style.display='';if(!mot.msg)$('mo').textContent=m.cal?'Calibrated.':'Not calibrated: using the "'+(m.mount=='fin'?'on the back cover':'on the LED panel')+'" setting.';
 $('m1').textContent=m.flat?'Lying flat':OR[m.orient];$('m2').textContent=f2(m.g);$('m3').textContent=f2(m.raw)}).catch(()=>{})}
document.addEventListener('click',e=>{const b=e.target.closest('[data-cal]');if(b)fetch('/motion/cal?s='+b.dataset.cal).then(r=>r.text()).then(t=>{mot.msg=1;$('mo').textContent=t;setTimeout(()=>mot.msg=0,8000)})});
setInterval(mot,500);
$('wf').onsubmit=e=>{e.preventDefault();const s=$('ws').value.trim();if(!s)return;fetch('/wifi/add',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
 body:'s='+encodeURIComponent(s)+'&p='+encodeURIComponent($('wp').value)}).then(r=>{if(!r.ok)alert('Could not save that network.');$('ws').value=$('wp').value='';wifi()})};
let upd=0;$('cu').onclick=()=>{const b=$('cu'),f=$('fw');upd=1;b.disabled=true;f.textContent='Asking for a newer version...';let n=0;fetch('/checkupdate').catch(()=>{});
 const t=setInterval(()=>{n++;const done=()=>{clearInterval(t);b.disabled=false;upd=0};fetch('/status').then(r=>r.json()).then(s=>{f.textContent='Version '+s.fw+(s.ota?' ('+Math.round(s.ota/1024)+' KB)':'');
  if(/Up to date|failed|off/.test(s.fw))done()}).catch(()=>{if(n>3)f.textContent='Restarting with the new version. Reload in a moment.'});if(n>150)done()},2000)};
fetch('/sounds').then(r=>r.json()).then(l=>{if(!l.length)return;const s=$('snd');s.textContent='';l.forEach(n=>s.append(E('button',{textContent:n,onclick:()=>fetch('/test?s='+encodeURIComponent(n))})))}).catch(()=>{});
addEventListener('beforeunload',e=>{if($('sv').dataset.d)e.preventDefault()});
loadCfg();todo();status();wifi();setInterval(status,5000);
</script></body></html>)HTML";
