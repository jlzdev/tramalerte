export interface Passage {
  ligne: string
  idLigne: string
  destination: string
  sensAller: boolean
  idArret: string
  secondes: number
  fiable: boolean
  couleurFond: string
  couleurTexte: string
}

export interface Reglages {
  departMin: number
  margeMin: number
}

export const REGLAGES_DEFAUT: Reglages = { departMin: 5, margeMin: 1 }

export const LIMITES: Record<keyof Reglages, [number, number]> = {
  departMin: [1, 45],
  margeMin: [0, 10],
}

export function normaliserReglages(brut: Record<string, unknown>): Reglages {
  const out = { ...REGLAGES_DEFAUT }
  const ancien = Number(brut.marcheMin) + Number(brut.preparationMin)
  const source: Record<string, unknown> = brut.departMin === undefined && Number.isFinite(ancien) ? { ...brut, departMin: ancien } : brut
  for (const k of Object.keys(LIMITES) as (keyof Reglages)[]) {
    const v = Number(source[k])
    const [min, max] = LIMITES[k]
    if (Number.isFinite(v)) out[k] = Math.min(max, Math.max(min, Math.round(v)))
  }
  return out
}

export type EtatCandidat = 'rate' | 'cours' | 'prepare' | 'tranquille'
export type Etat = 'cours' | 'prepare' | 'tranquille' | 'inconnu'

export interface Candidat {
  passage: Passage
  etat: EtatCandidat
  resteSec: number
  tramSec: number
  departMs: number
  tramMs: number
}

export interface Verdict {
  etat: Etat
  cible: Candidat | null
  candidats: Candidat[]
}

export const COURS_FENETRE_SEC = 60
export const PREPARE_FENETRE_SEC = 180

export function evaluer(passages: Passage[], nowMs: number, fetchedAtMs: number, r: Reglages): Verdict {
  const ecoule = Math.max(0, (nowMs - fetchedAtMs) / 1000)
  const avance = (r.departMin + r.margeMin) * 60
  const candidats: Candidat[] = passages
    .map((p) => {
      const tramSec = p.secondes - ecoule
      const resteSec = tramSec - avance
      let etat: EtatCandidat = 'tranquille'
      if (resteSec < 0) etat = 'rate'
      else if (resteSec < COURS_FENETRE_SEC) etat = 'cours'
      else if (resteSec < COURS_FENETRE_SEC + PREPARE_FENETRE_SEC) etat = 'prepare'
      return { passage: p, etat, resteSec, tramSec, departMs: nowMs + resteSec * 1000, tramMs: nowMs + tramSec * 1000 }
    })
    .sort((a, b) => a.tramSec - b.tramSec)
  const cible = candidats.find((c) => c.etat !== 'rate') ?? null
  const etat: Etat = cible && cible.etat !== 'rate' ? cible.etat : 'inconnu'
  return { etat, cible, candidats }
}

export function fmtHM(ms: number): string {
  const d = new Date(ms)
  const m = d.getMinutes()
  return d.getHours() + 'h' + (m ? String(m).padStart(2, '0') : '')
}

export function fmtDuree(sec: number, precis = false): string {
  const s = Math.max(0, Math.round(sec))
  const m = Math.floor(s / 60)
  if (!precis) return m + ' min'
  if (m === 0) return s + ' s'
  return m + ' min ' + String(s % 60).padStart(2, '0')
}

export interface Libelles {
  titre: string
  compte: string
  detail: string
}

export function libelles(v: Verdict, nowMs: number): Libelles {
  const c = v.cible
  if (!c) return { titre: 'Pas de tram annoncé', compte: '--', detail: '' }
  const p = c.passage
  const theorique = p.fiable ? '' : ' (horaire théorique)'
  const tram = p.ligne + ' vers ' + p.destination
  if (c.etat === 'cours') {
    return {
      titre: 'Pars maintenant',
      compte: fmtDuree(c.resteSec, true),
      detail: 'Tram ' + tram + ' dans ' + fmtDuree(c.tramSec, true) + theorique + '.',
    }
  }
  if (c.etat === 'prepare') {
    return {
      titre: 'Prépare-toi',
      compte: fmtDuree(c.resteSec, true),
      detail: 'Départ à ' + fmtHM(c.departMs) + ', tram ' + tram + ' à ' + fmtHM(c.tramMs) + theorique + '.',
    }
  }
  return {
    titre: 'Tu as le temps',
    compte: fmtDuree(c.resteSec),
    detail: 'Départ à ' + fmtHM(Math.max(nowMs, c.departMs)) + ', tram ' + tram + ' à ' + fmtHM(c.tramMs) + theorique + '.',
  }
}
