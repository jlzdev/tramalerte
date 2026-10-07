import { evaluer, normaliserReglages, type Passage, type Reglages, type Verdict } from './lib/depart'
import { avecCle } from './lib/cle'
import { getTempsLieu, GinkoError } from './lib/ginko'
import { lsDel, lsJson, lsSet } from './lib/storage'

export interface Direction {
  idLigne: string
  sensAller: boolean
  ligne: string
  destination: string
}

export interface Selection {
  nom: string
  tram: boolean
  directions: Direction[]
}

const KEY_SELECTION = 'tramalerte.selection'
const KEY_REGLAGES = 'tramalerte.reglages'
const INTERVALLE_MS = 30_000
const INTERVALLE_PROCHE_MS = 15_000
const PROCHE_SEC = 300
export const PERIME_MS = 120_000

export function cleDirection(d: { idLigne: string; sensAller: boolean }): string {
  return d.idLigne + '|' + (d.sensAller ? 'A' : 'R')
}

function nettoyer(s: unknown, max = 60): string {
  return String(s ?? '').replace(/[\p{Cc}]/gu, '').trim().slice(0, max)
}

function chargerSelection(): Selection | null {
  const s = lsJson<Partial<Selection>>(KEY_SELECTION)
  const nom = nettoyer(s?.nom)
  if (!s || !nom) return null
  const directions = Array.isArray(s.directions)
    ? s.directions
      .filter((d) => d && typeof d === 'object' && nettoyer(d.idLigne))
      .map((d) => ({
        idLigne: nettoyer(d.idLigne, 10),
        sensAller: d.sensAller === true,
        ligne: nettoyer(d.ligne, 10),
        destination: nettoyer(d.destination),
      }))
    : []
  return { nom, tram: s.tram === true, directions }
}

export const state = {
  selection: chargerSelection(),
  reglages: normaliserReglages(lsJson<Record<string, unknown>>(KEY_REGLAGES) ?? {}),
  passagesTous: [] as Passage[],
  fetchedAt: null as number | null,
  erreur: null as string | null,
  rafraichissement: false,
  nowMs: Date.now(),
  fige: false,
}

type Listener = () => void
const listeners = new Set<Listener>()

export function onChange(fn: Listener): void {
  listeners.add(fn)
}

function notifier(): void {
  for (const fn of listeners) fn()
}

export function passagesChoisis(): Passage[] {
  const sel = state.selection
  if (!sel) return []
  const cles = new Set(sel.directions.map(cleDirection))
  return state.passagesTous.filter((p) => cles.has(cleDirection(p)))
}

export function verdict(): Verdict {
  const fetchedAt = state.fetchedAt ?? state.nowMs
  return evaluer(passagesChoisis(), state.nowMs, fetchedAt, state.reglages)
}

export function setSelection(sel: Selection | null): void {
  state.selection = sel
  if (sel) lsSet(KEY_SELECTION, JSON.stringify(sel))
  else lsDel(KEY_SELECTION)
  state.passagesTous = []
  state.fetchedAt = null
  state.erreur = null
  notifier()
  void rafraichir(true)
}

export function setDirections(directions: Direction[]): void {
  if (!state.selection) return
  state.selection = { ...state.selection, directions }
  lsSet(KEY_SELECTION, JSON.stringify(state.selection))
  notifier()
}

export function setReglages(partiel: Partial<Reglages>): void {
  state.reglages = normaliserReglages({ ...state.reglages, ...partiel })
  lsSet(KEY_REGLAGES, JSON.stringify(state.reglages))
  notifier()
}

let derniereTentative = 0
let enCours: Promise<void> | null = null

export function rafraichir(force = false): Promise<void> {
  if (enCours) return enCours
  const sel = state.selection
  if (!sel || state.fige) return Promise.resolve()
  if (!force && document.hidden) return Promise.resolve()
  derniereTentative = Date.now()
  state.rafraichissement = true
  notifier()
  enCours = avecCle((key) => getTempsLieu(key, sel.nom))
    .then((r) => {
      state.passagesTous = r.passages
      state.fetchedAt = Date.now()
      state.erreur = null
    })
    .catch((e: unknown) => {
      state.erreur = e instanceof Error ? e.message : 'Erreur inconnue.'
      if (e instanceof GinkoError && e.cleRefusee) {
        state.passagesTous = []
        state.fetchedAt = null
      }
    })
    .finally(() => {
      state.rafraichissement = false
      enCours = null
      state.nowMs = Date.now()
      notifier()
    })
  return enCours
}

function intervalle(): number {
  const c = verdict().cible
  return c && c.resteSec < PROCHE_SEC ? INTERVALLE_PROCHE_MS : INTERVALLE_MS
}

function tick(): void {
  state.nowMs = Date.now()
  if (state.selection && !state.fige && !document.hidden && state.nowMs - derniereTentative >= intervalle()) {
    void rafraichir()
  }
  notifier()
}

export function figer(passages: Passage[]): void {
  state.fige = true
  state.passagesTous = passages
  state.fetchedAt = Date.now()
  state.erreur = null
  notifier()
}

export function initStore(): void {
  document.addEventListener('visibilitychange', () => {
    if (!document.hidden) void rafraichir()
  })
  setInterval(tick, 1000)
  void rafraichir(true)
}
