// Page web de la station, servie par l'ESP32. Elle lit /donnees toutes les minutes
// et trace les courbes elle-même, sans bibliothèque externe (marche sans Internet).
#pragma once
#include <Arduino.h>

const char PAGE_WEB[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Station météo</title>
<style>
:root{--fond:#f4f6f8;--carte:#fff;--texte:#1d2329;--doux:#6b7682;--trait:#e2e6ea}
@media (prefers-color-scheme:dark){:root{--fond:#14181c;--carte:#1e242a;--texte:#e8ecef;--doux:#8d98a3;--trait:#2c343b}}
body{margin:0;font-family:system-ui,sans-serif;background:var(--fond);color:var(--texte)}
main{max-width:720px;margin:auto;padding:16px}
h1{font-size:1.3em;margin:0 0 12px}
.grille{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.carte{background:var(--carte);border-radius:12px;padding:12px 14px}
.etiquette{color:var(--doux);font-size:.85em}
.valeur{font-size:1.8em;font-weight:600;margin-top:2px}
.courbe{margin-top:10px}
canvas{width:100%;height:140px;display:block}
.bas{color:var(--doux);font-size:.8em;margin-top:12px}
</style></head><body><main>
<h1>Station météo</h1>
<div class="grille">
 <div class="carte"><div class="etiquette">Dedans</div><div class="valeur"><span id="t">-</span> <small id="h"></small></div></div>
 <div class="carte"><div class="etiquette">Dehors</div><div class="valeur"><span id="te">-</span> <small id="he"></small></div></div>
 <div class="carte"><div class="etiquette">Pression</div><div class="valeur" id="p">-</div></div>
 <div class="carte"><div class="etiquette">Tendance sur 3 h</div><div class="valeur" id="tend" style="font-size:1.2em">-</div></div>
 <div class="carte"><div class="etiquette">Sonde extérieure</div><div class="valeur" id="sonde" style="font-size:1.2em">-</div></div>
</div>
<div class="carte courbe"><div class="etiquette">Température sur 24 h (°C) : <b style="color:#e4572e">dedans</b>, <b style="color:#2e86de">dehors</b></div><canvas id="cT"></canvas></div>
<div class="carte courbe"><div class="etiquette">Humidité sur 24 h (%) : <b style="color:#e4572e">dedans</b>, <b style="color:#2e86de">dehors</b></div><canvas id="cH"></canvas></div>
<div class="carte courbe"><div class="etiquette">Pression sur 24 h (hPa)</div><canvas id="cP"></canvas></div>
<div class="bas" id="maj"></div>
</main><script>
const COUL=['#e4572e','#2e86de'];
function heure(s){if(!s)return'';const d=new Date(s*1000);return d.toLocaleTimeString('fr-FR',{hour:'2-digit',minute:'2-digit'})}
function f(v,d,u){return v===null?'-':v.toFixed(d)+u}
function trace(id,series,temps){
 const c=document.getElementById(id),r=devicePixelRatio||1,x=c.getContext('2d');
 const W=c.width=c.clientWidth*r,H=c.height=c.clientHeight*r;x.clearRect(0,0,W,H);
 const vals=series.flat().filter(v=>v!==null);if(vals.length<2){x.fillStyle='#888';x.font=12*r+'px sans-serif';x.fillText('Pas encore assez de mesures',8,H/2);return}
 let mn=Math.min(...vals),mx=Math.max(...vals);if(mx-mn<1){mn-=.5;mx+=.5}
 const g=42*r,hb=16*r,n=series[0].length,st=getComputedStyle(document.body);
 x.font=11*r+'px sans-serif';x.fillStyle=st.getPropertyValue('--doux');x.strokeStyle=st.getPropertyValue('--trait');x.lineWidth=1;
 for(let i=0;i<=2;i++){const v=mn+(mx-mn)*i/2,y=H-hb-(H-hb-6*r)*i/2;x.beginPath();x.moveTo(g,y);x.lineTo(W,y);x.stroke();x.fillText(v.toFixed(mx-mn<10?1:0),2,y+4*r)}
 x.fillText(heure(temps[0]),g,H-2*r);const fin=heure(temps[n-1]);x.fillText(fin,W-x.measureText(fin).width,H-2*r);
 series.forEach((s,k)=>{x.strokeStyle=COUL[k];x.lineWidth=2*r;x.beginPath();let suite=false;
  s.forEach((v,i)=>{if(v===null){suite=false;return}const px=g+(W-g)*(n>1?i/(n-1):0),py=H-hb-(H-hb-6*r)*(v-mn)/(mx-mn);suite?x.lineTo(px,py):x.moveTo(px,py);suite=true});x.stroke()})
}
async function rafraichir(){
 try{const d=await (await fetch('/donnees')).json(),a=d.actuel,hi=d.historique;
  document.getElementById('t').textContent=f(a.t,1,' °C');
  document.getElementById('h').textContent=f(a.h,0,' %');
  document.getElementById('te').textContent=f(a.te,1,' °C');
  document.getElementById('he').textContent=f(a.he,0,' %');
  document.getElementById('sonde').textContent=a.sondeIlYA===null?'en attente':(a.te===null?'muette !':'batterie '+a.batterie.toFixed(2)+' V')+(a.sondeIlYA===null?'':' · il y a '+Math.round(a.sondeIlYA/60)+' min');
  document.getElementById('p').textContent=f(a.p,0,' hPa');
  document.getElementById('tend').textContent=a.tendance+(a.var===null?'':' ('+(a.var>0?'+':'')+a.var.toFixed(1)+' hPa)');
  const temps=hi.map(p=>p[0]);
  trace('cT',[hi.map(p=>p[1]),hi.map(p=>p[4])],temps);trace('cH',[hi.map(p=>p[2]),hi.map(p=>p[5])],temps);trace('cP',[hi.map(p=>p[3])],temps);
  document.getElementById('maj').textContent='Mis à jour à '+new Date().toLocaleTimeString('fr-FR')+(a.prevision?' · '+a.prevision:'');
 }catch(e){document.getElementById('maj').textContent='Station injoignable, nouvel essai dans une minute'}
}
rafraichir();setInterval(rafraichir,60000);
</script></body></html>)rawliteral";
