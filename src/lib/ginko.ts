import type { Passage } from './depart'

const BASE = 'https://api.ginko.voyage/'
const TIMEOUT_MS = 8000

export class GinkoError extends Error {
  readonly cleRefusee: boolean
  constructor(message: string, cleRefusee = false) {
    super(message)
    this.name = 'GinkoError'
    this.cleRefusee = cleRefusee
  }
}

interface Enveloppe<T> {
  ok: boolean
  msg?: string
  objets?: T
}

async function appel<T>(methode: string, params: Record<string, string | number | boolean>): Promise<T> {
  const body = new URLSearchParams()
  for (const [k, v] of Object.entries(params)) body.set(k, String(v))
  let res: Response
  try {
    res = await fetch(BASE + methode + '.do', { method: 'POST', body, signal: AbortSignal.timeout(TIMEOUT_MS) })
  } catch {
    throw new GinkoError('Serveur Ginko injoignable.')
  }
  if (!res.ok) throw new GinkoError('Serveur Ginko indisponible (' + res.status + ').')
  let env: Enveloppe<T>
  try {
    env = await res.json() as Enveloppe<T>
  } catch {
    throw new GinkoError('Réponse Ginko incompréhensible.')
  }
  if (!env.ok || env.objets === undefined) {
    const msg = env.msg || 'Réponse Ginko incompréhensible.'
    throw new GinkoError(msg, /cl[ée]/i.test(msg))
  }
  return env.objets
}

export async function getCleTemporaire(): Promise<string> {
  const o = await appel<{ key?: string }>('DR/getCleTemporaire', {})
  if (!o.key) throw new GinkoError('Pas de clé temporaire disponible.')
  return o.key
}

export interface Arret {
  id: string
  nom: string
  latitude: number
  longitude: number
}

export function getArrets(key: string): Promise<Arret[]> {
  return appel<Arret[]>('DR/getArrets', { apiKey: key })
}

export function getArretsProches(key: string, latitude: number, longitude: number): Promise<Arret[]> {
  return appel<Arret[]>('DR/getArretsProches', { apiKey: key, latitude, longitude })
}

export interface Ligne {
  id: string
  numLignePublic: string
  libellePublic: string
  modeTransport: number
  couleurFond: string
  couleurTexte: string
  variantes: { id: string; destination: string; sensAller: boolean }[]
}

export const MODE_TRAM = 1

export function getLignes(key: string): Promise<Ligne[]> {
  return appel<Ligne[]>('DR/getLignes', { apiKey: key })
}

interface TempsBrut {
  numLignePublic: string
  idLigne: string
  destination: string
  sensAller: boolean
  idArret: string
  tempsEnSeconde: number
  fiable: boolean
  couleurFond: string
  couleurTexte: string
}

export interface TempsLieu {
  nomExact: string
  passages: Passage[]
}

export async function getTempsLieu(key: string, nom: string): Promise<TempsLieu> {
  const o = await appel<{ nomExact?: string; listeTemps?: TempsBrut[] }>('TR/getTempsLieu', { apiKey: key, nom })
  const passages: Passage[] = (o.listeTemps ?? [])
    .filter((t) => Number.isFinite(t.tempsEnSeconde))
    .map((t) => ({
      ligne: String(t.numLignePublic ?? ''),
      idLigne: String(t.idLigne ?? ''),
      destination: String(t.destination ?? ''),
      sensAller: t.sensAller === true,
      idArret: String(t.idArret ?? ''),
      secondes: t.tempsEnSeconde,
      fiable: t.fiable !== false,
      couleurFond: /^[0-9a-f]{6}$/i.test(t.couleurFond ?? '') ? t.couleurFond : '334155',
      couleurTexte: /^[0-9a-f]{6}$/i.test(t.couleurTexte ?? '') ? t.couleurTexte : 'ffffff',
    }))
  return { nomExact: String(o.nomExact ?? nom), passages }
}
