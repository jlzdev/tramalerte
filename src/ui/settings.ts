import { avecCle, cle, enregistrerCle, URL_CLE_ESSAI } from '../lib/cle'
import { LIMITES, type Reglages } from '../lib/depart'
import { getArrets, getArretsProches, getLignes, MODE_TRAM, type Arret, type Ligne } from '../lib/ginko'
import { cleDirection, rafraichir, setDirections, setReglages, setSelection, state, type Direction } from '../store'
import { el, h } from './dom'

let arretsCache: Promise<Arret[]> | null = null
let lignesCache: Promise<Ligne[]> | null = null
let lignes: Ligne[] = []

function arrets(): Promise<Arret[]> {
  arretsCache ??= avecCle(getArrets).catch((e: unknown) => { arretsCache = null; throw e })
  return arretsCache
}

function chargerLignes(): Promise<Ligne[]> {
  lignesCache ??= avecCle(getLignes)
    .then((l) => { lignes = l; return l })
    .catch((e: unknown) => { lignesCache = null; throw e })
  return lignesCache
}

function normaliser(s: string): string {
  return s.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().trim()
}

interface Candidat {
  nom: string
  tram: boolean
}

function regrouper(liste: Arret[]): Candidat[] {
  const map = new Map<string, Candidat>()
  for (const a of liste) {
    const c = map.get(a.nom) ?? { nom: a.nom, tram: false }
    if (a.id.startsWith('t_')) c.tram = true
    map.set(a.nom, c)
  }
  return [...map.values()]
}

function afficherResultats(cands: Candidat[], info: string): void {
  const box = el('arret-resultats')
  box.replaceChildren()
  for (const c of cands.slice(0, 10)) {
    const b = h('button', { type: 'button', class: 'btn' }, c.nom + (c.tram ? ' 🚊' : ''))
    b.addEventListener('click', () => choisirArret(c))
    box.append(b)
  }
  el('arret-info').textContent = info
}

function message(e: unknown): string {
  return e instanceof Error ? e.message : 'Erreur inconnue.'
}

async function chercher(q: string): Promise<void> {
  const nq = normaliser(q)
  if (nq.length < 2) {
    afficherResultats([], 'Tape au moins 2 lettres.')
    return
  }
  el('arret-info').textContent = 'Recherche...'
  try {
    const tous = regrouper(await arrets())
    const trouve = tous.filter((c) => normaliser(c.nom).includes(nq))
    trouve.sort((a, b) => Number(normaliser(b.nom).startsWith(nq)) - Number(normaliser(a.nom).startsWith(nq)) || a.nom.localeCompare(b.nom, 'fr'))
    afficherResultats(trouve, trouve.length ? '' : 'Aucun arrêt ne correspond.')
  } catch (e) {
    afficherResultats([], message(e))
  }
}

function autourDeMoi(): void {
  if (!navigator.geolocation) {
    afficherResultats([], 'Géolocalisation indisponible.')
    return
  }
  el('arret-info').textContent = 'Position...'
  navigator.geolocation.getCurrentPosition(
    async (pos) => {
      try {
        const proches = await avecCle((k) => getArretsProches(k, pos.coords.latitude, pos.coords.longitude))
        const cands = regrouper(proches)
        afficherResultats(cands, cands.length ? 'Du plus proche au plus loin.' : 'Aucun arrêt à proximité.')
      } catch (e) {
        afficherResultats([], message(e))
      }
    },
    () => afficherResultats([], 'Position refusée ou indisponible.'),
    { timeout: 10_000, maximumAge: 60_000 },
  )
}

function choisirArret(c: Candidat): void {
  setSelection({ nom: c.nom, tram: c.tram, directions: [] })
  el('arret-resultats').replaceChildren()
  el('arret-info').textContent = ''
  el<HTMLInputElement>('arret-q').value = ''
  if (c.tram) void chargerLignes().then(renderDirections).catch(() => {})
}

function directionsPossibles(): Direction[] {
  const sel = state.selection
  if (!sel) return []
  const map = new Map<string, Direction>()
  if (sel.tram) {
    for (const l of lignes) {
      if (l.modeTransport !== MODE_TRAM) continue
      for (const v of l.variantes) {
        const d = { idLigne: l.id, sensAller: v.sensAller, ligne: l.numLignePublic, destination: v.destination }
        map.set(cleDirection(d), d)
      }
    }
  }
  for (const p of state.passagesTous) {
    const d = { idLigne: p.idLigne, sensAller: p.sensAller, ligne: p.ligne, destination: p.destination }
    if (!map.has(cleDirection(d))) map.set(cleDirection(d), d)
  }
  for (const d of sel.directions) {
    if (!map.has(cleDirection(d))) map.set(cleDirection(d), d)
  }
  return [...map.values()]
}

let directionsRendues = ''

export function renderDirections(): void {
  const sel = state.selection
  const bloc = el('directions-bloc')
  bloc.classList.toggle('hidden', !sel)
  if (!sel) { directionsRendues = ''; return }
  const possibles = directionsPossibles()
  const choisies = new Set(sel.directions.map(cleDirection))
  const box = el('directions')
  const signature = JSON.stringify(possibles)
  if (signature !== directionsRendues) {
    directionsRendues = signature
    box.replaceChildren()
    if (!possibles.length) {
      box.append(h('p', { class: 'text-xs text-dim' }, state.fetchedAt === null ? 'Chargement des directions...' : 'Aucun passage annoncé à cet arrêt pour l\'instant, les directions apparaîtront au prochain passage.'))
    }
    for (const d of possibles) {
      const key = cleDirection(d)
      const input = h('input', { type: 'checkbox', class: 'h-4 w-4 accent-accent', 'data-direction': key })
      input.addEventListener('change', () => {
        const actuelles = state.selection?.directions ?? []
        const sans = actuelles.filter((x) => cleDirection(x) !== key)
        setDirections(input.checked ? [...sans, d] : sans)
      })
      box.append(h('label', { class: 'choix' }, input, h('span', { class: 'font-bold' }, d.ligne), h('span', {}, '→ ' + d.destination)))
    }
  }
  for (const input of box.querySelectorAll<HTMLInputElement>('input[data-direction]')) {
    input.checked = choisies.has(input.dataset.direction ?? '')
  }
}

export function renderCle(): void {
  const c = cle()
  el('cle-etat').textContent = c
    ? 'Clé enregistrée dans ce navigateur' + (c.at ? ' le ' + new Date(c.at).toLocaleDateString('fr-FR', { day: 'numeric', month: 'long' }) : '') + '.'
    : 'Aucune clé enregistrée, l\'appli ne peut rien demander à Ginko.'
  el('cle-oublier').classList.toggle('hidden', !c)
  el<HTMLAnchorElement>('cle-essai').href = URL_CLE_ESSAI
}

export function renderSelection(): void {
  const sel = state.selection
  const p = el('selection')
  if (!sel) p.textContent = 'Aucun arrêt choisi.'
  else if (!sel.directions.length) p.textContent = sel.nom + ', aucune direction cochée.'
  else p.textContent = sel.nom + ' · ' + sel.directions.map((d) => d.ligne + ' → ' + d.destination).join(', ')
  renderDirections()
}

export function initSettingsUi(): void {
  const section = el('reglages')
  const bouton = el('modifier')
  const afficher = (ouvert: boolean) => {
    section.classList.toggle('hidden', !ouvert)
    bouton.textContent = ouvert ? 'Fermer' : 'Réglages'
  }
  const ouvrirSiBesoin = () => afficher(!state.selection || !state.selection.directions.length)
  ouvrirSiBesoin()
  window.addEventListener('pageshow', ouvrirSiBesoin)
  bouton.addEventListener('click', () => {
    const ouvert = section.classList.contains('hidden')
    afficher(ouvert)
    if (ouvert) section.scrollIntoView({ behavior: 'smooth', block: 'start' })
  })

  el('arret-form').addEventListener('submit', (ev) => {
    ev.preventDefault()
    void chercher(el<HTMLInputElement>('arret-q').value)
  })
  el('arret-geo').addEventListener('click', autourDeMoi)

  for (const k of Object.keys(LIMITES) as (keyof Reglages)[]) {
    const input = el<HTMLInputElement>(k)
    input.value = String(state.reglages[k])
    input.addEventListener('change', () => {
      setReglages({ [k]: Number(input.value) })
      input.value = String(state.reglages[k])
    })
  }

  el('cle-form').addEventListener('submit', (ev) => {
    ev.preventDefault()
    const input = el<HTMLInputElement>('cle')
    enregistrerCle(input.value)
    input.value = ''
    renderCle()
    void rafraichir(true)
  })
  el('cle-oublier').addEventListener('click', () => {
    enregistrerCle('')
    renderCle()
    void rafraichir(true)
  })
  renderCle()
  if (state.selection?.tram) void chargerLignes().then(renderDirections).catch(() => {})
}
