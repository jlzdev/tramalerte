import { fmtDuree, fmtHM } from '../lib/depart'
import { state, verdict } from '../store'
import { el, h } from './dom'

export function renderNext(): void {
  const ul = el('next')
  const sel = state.selection
  ul.replaceChildren()
  if (!sel || !sel.directions.length) {
    ul.append(h('li', { class: 'text-dim' }, 'Rien à afficher tant que l\'arrêt et les directions ne sont pas choisis.'))
  } else {
    const v = verdict()
    if (!v.candidats.length) {
      ul.append(h('li', { class: 'text-dim' }, state.fetchedAt === null && !state.erreur ? 'Chargement...' : 'Aucun passage annoncé.'))
    }
    for (const c of v.candidats.slice(0, 4)) {
      const p = c.passage
      const badge = h('span', {
        class: 'inline-block min-w-9 rounded-md px-1.5 py-0.5 text-center text-xs font-bold',
        style: 'background:#' + p.couleurFond + ';color:#' + p.couleurTexte,
      }, p.ligne)
      const rate = c.etat === 'rate'
      const quand = rate
        ? 'trop tard, tram dans ' + fmtDuree(c.tramSec, true)
        : 'pars à ' + fmtHM(Math.max(state.nowMs, c.departMs)) + ', tram à ' + fmtHM(c.tramMs) + ' (' + fmtDuree(c.tramSec) + ')'
      const li = h('li', { class: 'flex items-center gap-2.5' + (rate ? ' opacity-50' : '') },
        badge,
        h('span', { class: 'min-w-0 flex-1' },
          h('span', { class: 'font-semibold' }, p.destination),
          h('span', { class: 'block text-xs text-dim' }, quand + (p.fiable ? '' : ', horaire théorique')),
        ),
      )
      ul.append(li)
    }
  }
  const etat = el('etat')
  if (!sel) etat.textContent = ''
  else if (state.fige) etat.textContent = 'Mode simulation, rafraîchissement suspendu.'
  else if (state.erreur) etat.textContent = 'Erreur Ginko : ' + state.erreur
  else if (state.fetchedAt) etat.textContent = 'Temps réel Ginko mis à jour à ' + fmtHM(state.fetchedAt) + (state.rafraichissement ? ', rafraîchissement...' : '.')
  else etat.textContent = ''
}
