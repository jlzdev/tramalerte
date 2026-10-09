#pragma once

#include <pgmspace.h>

static const char PAGE_PORTAIL[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Tram alerte</title>
<style>
:root{color-scheme:dark}
body{margin:0;padding:12px;background:#0e1420;color:#e8edf5;font:16px system-ui,sans-serif}
main{max-width:480px;margin:auto;display:flex;flex-direction:column;gap:14px}
h1{font-size:20px;margin:0}
p{margin:0}
.card{background:#18212f;border:1px solid #223048;border-radius:14px;padding:14px;display:flex;flex-direction:column;gap:8px}
.hdr{font-size:13px;font-weight:600;text-transform:uppercase;letter-spacing:.06em;color:#8fa1b8}
.dim{color:#8fa1b8;font-size:13px}
input{width:100%;box-sizing:border-box;background:#0e1420;color:#e8edf5;border:1px solid #2a3648;border-radius:10px;padding:9px 11px;font-size:16px}
input:focus{outline:none;border-color:#4aa8ff}
button{cursor:pointer;background:#1f2a3b;color:#e8edf5;border:1px solid #2a3648;border-radius:10px;padding:9px 12px;font-size:15px;font-weight:600}
button.accent{background:#4aa8ff;color:#0e1420;border-color:#4aa8ff}
button:disabled{opacity:.5}
.row{display:flex;gap:8px}
.row input{flex:1}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
label{display:flex;flex-direction:column;gap:4px;font-size:13px;color:#8fa1b8}
.choix{display:flex;align-items:center;gap:10px;border:1px solid #2a3648;border-radius:10px;padding:8px 10px;font-size:15px;color:#e8edf5}
.choix b{min-width:26px}
.res{display:flex;flex-wrap:wrap;gap:8px}
.hidden{display:none}
.ok{color:#6ee7a0}
.err{color:#ff8a7a}
a{color:#4aa8ff}
</style>
</head>
<body>
<main>
<header>
<h1>Tram alerte</h1>
<p id="etat" class="dim">Chargement...</p>
</header>

<section class="card">
<div class="hdr">Clé API Ginko</div>
<p id="cle-etat" class="dim"></p>
<div class="row">
<input id="cle" type="password" placeholder="Colle ta clé ici" autocomplete="off">
<button type="button" id="cle-voir">Voir</button>
</div>
<p class="dim">Sans clé définitive, <a href="https://api.ginko.voyage/DR/getCleTemporaire.do" target="_blank" rel="noopener">la clé d'essai du jour</a> dépanne 24 h : copie la valeur de "key" et colle-la ici. La clé reste dans l'afficheur.</p>
</section>

<section class="card">
<div class="hdr">Arrêt de départ</div>
<p id="arret-actuel" class="dim"></p>
<form id="arret-form" class="row">
<input id="arret-q" type="search" placeholder="Nom de l'arrêt" autocomplete="off" enterkeyhint="search">
<button type="submit" class="accent">Chercher</button>
<button type="button" id="arret-geo" title="Autour de moi">◎</button>
</form>
<p id="arret-info" class="dim"></p>
<div id="arret-resultats" class="res"></div>
</section>

<section class="card hidden" id="directions-bloc">
<div class="hdr">Trams et bus qui me vont</div>
<p class="dim">Coche toutes les directions qui t'emmènent à bon port, l'afficheur prend le premier qui passe.</p>
<div id="directions" style="display:flex;flex-direction:column;gap:6px"></div>
</section>

<section class="card">
<div class="hdr">Temps</div>
<div class="grid">
<label>Temps pour partir vers l'arrêt (min)<input id="departMin" type="number" inputmode="numeric" min="1" max="45"></label>
<label>Début de la nuit (heure)<input id="nuitDebut" type="number" inputmode="numeric" min="0" max="24"></label>
<label>Fin de la nuit (heure)<input id="nuitFin" type="number" inputmode="numeric" min="0" max="24"></label>
</div>
<p class="dim">La nuit, l'afficheur montre l'heure et la météo et ne consulte plus Ginko. Mets les deux heures à la même valeur pour désactiver.</p>
</section>

<button type="button" id="enregistrer" class="accent">Enregistrer sur l'afficheur</button>
<p id="retour" class="dim"></p>
</main>
<script>
const API = 'https://api.ginko.voyage/';
const $ = (id) => document.getElementById(id);
let etat = { cle: '', arret: '', tram: false, directions: [], departMin: 5, nuitDebut: 23, nuitFin: 6, latitude: 0, longitude: 0 };
let arretsCache = null, lignesCache = null, passagesArret = [];

async function ginko(methode, params) {
  const body = new URLSearchParams({ ...params, apiKey: $('cle').value.trim() || etat.cle });
  const res = await fetch(API + methode + '.do', { method: 'POST', body, signal: AbortSignal.timeout(10000) });
  const env = await res.json();
  if (!env.ok) throw new Error(env.msg || 'Réponse Ginko incompréhensible');
  return env.objets;
}

const normaliser = (s) => s.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().trim();
const cleDir = (d) => d.idLigne + '|' + (d.sensAller ? 'A' : 'R');

function regrouper(liste) {
  const map = new Map();
  for (const a of liste) {
    const c = map.get(a.nom) || { nom: a.nom, tram: false, latitude: a.latitude, longitude: a.longitude };
    if (a.id.startsWith('t_')) c.tram = true;
    map.set(a.nom, c);
  }
  return [...map.values()];
}

function afficherResultats(cands, info) {
  const box = $('arret-resultats');
  box.replaceChildren();
  for (const c of cands.slice(0, 10)) {
    const b = document.createElement('button');
    b.type = 'button';
    b.textContent = c.nom + (c.tram ? ' 🚊' : '');
    b.addEventListener('click', () => choisirArret(c));
    box.append(b);
  }
  $('arret-info').textContent = info;
}

async function chercher(q) {
  const nq = normaliser(q);
  if (nq.length < 2) return afficherResultats([], 'Tape au moins 2 lettres.');
  $('arret-info').textContent = 'Recherche...';
  try {
    arretsCache ||= regrouper(await ginko('DR/getArrets', {}));
    const trouve = arretsCache.filter((c) => normaliser(c.nom).includes(nq));
    trouve.sort((a, b) => Number(normaliser(b.nom).startsWith(nq)) - Number(normaliser(a.nom).startsWith(nq)) || a.nom.localeCompare(b.nom, 'fr'));
    afficherResultats(trouve, trouve.length ? '' : 'Aucun arrêt ne correspond.');
  } catch (e) { arretsCache = null; afficherResultats([], e.message); }
}

function autourDeMoi() {
  if (!navigator.geolocation) return afficherResultats([], 'Géolocalisation indisponible.');
  $('arret-info').textContent = 'Position...';
  navigator.geolocation.getCurrentPosition(async (pos) => {
    try {
      const proches = regrouper(await ginko('DR/getArretsProches', { latitude: pos.coords.latitude, longitude: pos.coords.longitude }));
      afficherResultats(proches, proches.length ? 'Du plus proche au plus loin.' : 'Aucun arrêt à proximité.');
    } catch (e) { afficherResultats([], e.message); }
  }, () => afficherResultats([], 'Position refusée ou indisponible.'), { timeout: 10000, maximumAge: 60000 });
}

async function choisirArret(c) {
  etat.arret = c.nom; etat.tram = c.tram; etat.directions = []; etat.latitude = c.latitude || 0; etat.longitude = c.longitude || 0;
  $('arret-resultats').replaceChildren(); $('arret-info').textContent = ''; $('arret-q').value = '';
  renderArret();
  await chargerDirections();
}

async function chargerDirections() {
  $('directions-bloc').classList.toggle('hidden', !etat.arret);
  if (!etat.arret) return;
  $('directions').textContent = 'Chargement des directions...';
  try {
    const [temps, lignes] = await Promise.all([
      ginko('TR/getTempsLieu', { nom: etat.arret }),
      etat.tram ? (lignesCache ||= ginko('DR/getLignes', {})) : Promise.resolve([]),
    ]);
    passagesArret = temps.listeTemps || [];
    const map = new Map();
    for (const l of lignes) if (l.modeTransport === 1) for (const v of l.variantes) {
      const d = { idLigne: String(l.id), sensAller: v.sensAller, ligne: l.numLignePublic, destination: v.destination };
      map.set(cleDir(d), d);
    }
    for (const p of passagesArret) {
      const d = { idLigne: String(p.idLigne), sensAller: p.sensAller, ligne: p.numLignePublic, destination: p.destination };
      if (!map.has(cleDir(d))) map.set(cleDir(d), d);
    }
    for (const d of etat.directions) if (!map.has(cleDir(d))) map.set(cleDir(d), d);
    renderDirections([...map.values()]);
  } catch (e) { $('directions').textContent = e.message; }
}

function renderDirections(possibles) {
  const box = $('directions');
  box.replaceChildren();
  if (!possibles.length) { box.textContent = 'Aucun passage annoncé à cet arrêt pour l\'instant.'; return; }
  const choisies = new Set(etat.directions.map(cleDir));
  for (const d of possibles) {
    const label = document.createElement('label'); label.className = 'choix';
    const input = document.createElement('input'); input.type = 'checkbox'; input.style.width = 'auto'; input.checked = choisies.has(cleDir(d));
    input.addEventListener('change', () => {
      etat.directions = etat.directions.filter((x) => cleDir(x) !== cleDir(d));
      if (input.checked) etat.directions.push(d);
      renderArret();
    });
    const b = document.createElement('b'); b.textContent = d.ligne;
    const s = document.createElement('span'); s.textContent = '→ ' + d.destination;
    label.append(input, b, s); box.append(label);
  }
}

function renderArret() {
  $('arret-actuel').textContent = etat.arret ? 'Arrêt : ' + etat.arret + (etat.directions.length ? ' · ' + etat.directions.map((d) => d.ligne + ' → ' + d.destination).join(', ') : ' (aucune direction cochée)') : 'Aucun arrêt choisi.';
}

function renderCle() {
  $('cle-etat').textContent = etat.cle ? 'Une clé est enregistrée dans l\'afficheur. Laisse le champ vide pour la garder.' : 'Aucune clé enregistrée, l\'afficheur ne peut rien demander à Ginko.';
}

async function charger() {
  try {
    const c = await (await fetch('/config', { cache: 'no-store' })).json();
    etat = { ...etat, ...c };
    $('etat').textContent = 'Afficheur sur ' + c.ip;
    for (const k of ['departMin', 'nuitDebut', 'nuitFin']) $(k).value = etat[k];
    renderCle(); renderArret();
    if (etat.arret && etat.cle) chargerDirections();
  } catch (e) { $('etat').textContent = 'Afficheur injoignable : ' + e.message; }
}

async function enregistrer() {
  const b = $('enregistrer'); b.disabled = true; $('retour').textContent = 'Enregistrement...'; $('retour').className = 'dim';
  const corps = { arret: etat.arret, tram: etat.tram, directions: etat.directions, latitude: etat.latitude, longitude: etat.longitude };
  for (const k of ['departMin', 'nuitDebut', 'nuitFin']) corps[k] = Number($(k).value);
  const cle = $('cle').value.trim();
  if (cle) corps.cle = cle;
  try {
    const res = await fetch('/enregistrer', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(corps) });
    if (!res.ok) throw new Error(await res.text());
    if (cle) { etat.cle = cle; $('cle').value = ''; renderCle(); }
    $('retour').textContent = 'Enregistré, l\'afficheur se met à jour.'; $('retour').className = 'ok';
  } catch (e) { $('retour').textContent = 'Échec : ' + e.message; $('retour').className = 'err'; }
  b.disabled = false;
}

$('arret-form').addEventListener('submit', (ev) => { ev.preventDefault(); chercher($('arret-q').value); });
$('arret-geo').addEventListener('click', autourDeMoi);
$('cle-voir').addEventListener('click', () => { const i = $('cle'); i.type = i.type === 'password' ? 'text' : 'password'; });
$('enregistrer').addEventListener('click', enregistrer);
charger();
</script>
</body>
</html>
)HTML";
