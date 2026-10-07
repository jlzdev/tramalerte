import { fmtHM, libelles, type Etat } from '../lib/depart'
import { PERIME_MS, state, verdict } from '../store'
import { el } from './dom'

const ETATS: Etat[] = ['cours', 'prepare', 'tranquille', 'inconnu']

export function renderVerdict(): void {
  const box = el('verdict')
  const sel = state.selection
  let etat: Etat = 'inconnu'
  let titre = 'Choisis ton arrêt'
  let compte = '--'
  let detail = 'Dans les réglages, juste en dessous.'
  if (sel && !sel.directions.length) {
    titre = 'Choisis tes directions'
    detail = 'Dans les réglages, coche les trams ou bus qui te vont.'
  } else if (sel && state.fetchedAt === null) {
    titre = state.erreur ? 'Pas d\'horaire' : 'Chargement'
    detail = state.erreur ?? 'Interrogation de Ginko...'
  } else if (sel) {
    const v = verdict()
    const l = libelles(v, state.nowMs)
    etat = v.etat
    titre = l.titre
    compte = l.compte
    detail = l.detail
    if (v.etat === 'inconnu') detail = state.erreur ?? 'Aucun passage annoncé pour tes directions.'
    const age = state.nowMs - (state.fetchedAt ?? state.nowMs)
    if (age > PERIME_MS) {
      detail += ' Données de ' + fmtHM(state.fetchedAt ?? state.nowMs) + ', à prendre avec des pincettes.'
    }
  }
  for (const e of ETATS) box.classList.toggle('v-' + e, e === etat)
  el('verdict-titre').textContent = titre
  el('verdict-compte').textContent = compte
  el('verdict-detail').textContent = detail
}
