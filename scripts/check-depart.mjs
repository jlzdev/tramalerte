import { evaluer, fmtDuree, fmtHM, libelles, normaliserReglages, REGLAGES_DEFAUT } from '../src/lib/depart.ts'

let echecs = 0
function attendu(nom, ok, info = '') {
  console.log((ok ? 'OK   ' : 'ECHEC') + ' ' + nom + (info ? '  ' + info : ''))
  if (!ok) echecs++
}

const now = Date.UTC(2026, 9, 10, 7, 30, 0)
const passage = (secondes, extra = {}) => ({
  ligne: 'T1', idLigne: '101', destination: 'Chalezeule', sensAller: true, idArret: 't_batt1',
  secondes, fiable: true, couleurFond: '00A5C2', couleurTexte: 'FFFFFF', ...extra,
})
const r = REGLAGES_DEFAUT

const cas = [
  [120, 'inconnu'],
  [359, 'inconnu'],
  [360, 'cours'],
  [419, 'cours'],
  [420, 'prepare'],
  [599, 'prepare'],
  [600, 'tranquille'],
  [1500, 'tranquille'],
]
for (const [sec, etat] of cas) {
  const v = evaluer([passage(sec)], now, now, r)
  attendu('tram dans ' + sec + ' s -> ' + etat, v.etat === etat, 'obtenu ' + v.etat)
  if (etat === 'inconnu') attendu('  ... et le passage est liste comme rate', v.candidats[0].etat === 'rate')
}

{
  const v = evaluer([passage(120), passage(1320)], now, now, r)
  attendu('premier rate, verdict sur le second', v.etat === 'tranquille' && v.cible.passage.secondes === 1320)
  attendu('le rate reste liste en premier', v.candidats[0].etat === 'rate' && v.candidats.length === 2)
}

{
  const v = evaluer([passage(1320), passage(300)], now, now, r)
  attendu('tri par heure de tram', v.candidats[0].passage.secondes === 300)
}

{
  const v = evaluer([passage(400)], now + 90_000, now, r)
  attendu('decompte local : 400 s il y a 90 s = rate', v.etat === 'inconnu' && Math.round(v.candidats[0].tramSec) === 310, 'etat ' + v.etat)
  const v2 = evaluer([passage(600)], now + 90_000, now, r)
  attendu('decompte local, reste = 600 - 90 - 360 = 150 -> prepare', Math.round(v2.cible.resteSec) === 150 && v2.etat === 'prepare', 'reste ' + v2.cible.resteSec)
}

{
  const v = evaluer([], now, now, r)
  attendu('aucun passage -> inconnu', v.etat === 'inconnu' && v.cible === null)
  const l = libelles(v, now)
  attendu('libelle inconnu', l.titre === 'Pas de tram annoncé' && l.compte === '--')
}

{
  const v = evaluer([passage(900, { fiable: false })], now, now, r)
  const l = libelles(v, now)
  attendu('horaire theorique mentionne', l.detail.includes('(horaire théorique)'), l.detail)
  attendu('libelle tranquille', l.titre === 'Tu as le temps' && l.compte === '9 min', l.compte)
}

{
  const v = evaluer([passage(390)], now, now, r)
  const l = libelles(v, now)
  attendu('libelle cours', l.titre === 'Pars maintenant' && l.compte === '30 s' && l.detail.startsWith('Tram T1 vers Chalezeule dans 6 min 30'), JSON.stringify(l))
}

{
  const v = evaluer([passage(520)], now, now, r)
  const l = libelles(v, now)
  attendu('libelle prepare', l.titre === 'Prépare-toi' && l.compte === '2 min 40', JSON.stringify(l))
}

attendu('fmtDuree 45 s', fmtDuree(45, true) === '45 s')
attendu('fmtDuree 125 precis', fmtDuree(125, true) === '2 min 05')
attendu('fmtDuree 125 coarse', fmtDuree(125) === '2 min')
attendu('fmtDuree negatif', fmtDuree(-5, true) === '0 s')
{
  const d = new Date(2026, 9, 10, 9, 5)
  attendu('fmtHM 9h05', fmtHM(d.getTime()) === '9h05')
  const d2 = new Date(2026, 9, 10, 23, 0)
  attendu('fmtHM 23h', fmtHM(d2.getTime()) === '23h')
}

{
  const n = normaliserReglages({ departMin: '7', margeMin: 'abc' })
  attendu('normaliserReglages', n.departMin === 7 && n.margeMin === 1, JSON.stringify(n))
  const m = normaliserReglages({ marcheMin: 3, preparationMin: 2, margeMin: 2 })
  attendu('migration marche + preparation -> depart', m.departMin === 5 && m.margeMin === 2, JSON.stringify(m))
  const g = normaliserReglages({ departMin: 99 })
  attendu('borne haute depart', g.departMin === 45, JSON.stringify(g))
}

console.log(echecs ? echecs + ' echec(s)' : 'Tout est bon')
process.exit(echecs ? 1 : 0)
