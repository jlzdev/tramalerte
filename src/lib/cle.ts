import { GinkoError } from './ginko'
import { lsDel, lsJson, lsSet } from './storage'

const KEY_CLE = 'tramalerte.cle'

export const URL_CLE_ESSAI = 'https://api.ginko.voyage/DR/getCleTemporaire.do'

export interface Cle {
  key: string
  at: number
}

export function cle(): Cle | null {
  const c = lsJson<Partial<Cle>>(KEY_CLE)
  if (!c || typeof c.key !== 'string' || !c.key) return null
  return { key: c.key, at: Number.isFinite(c.at) ? Number(c.at) : 0 }
}

export function enregistrerCle(brut: string): void {
  const key = brut.replace(/[^\w-]/g, '').slice(0, 128)
  if (key) lsSet(KEY_CLE, JSON.stringify({ key, at: Date.now() } satisfies Cle))
  else lsDel(KEY_CLE)
}

export async function avecCle<T>(fn: (key: string) => Promise<T>): Promise<T> {
  const c = cle()
  if (!c) throw new GinkoError('Aucune clé API, colle-la dans les réglages.', true)
  try {
    return await fn(c.key)
  } catch (e) {
    if (e instanceof GinkoError && e.cleRefusee) throw new GinkoError('Clé API refusée ou expirée, colle une nouvelle clé dans les réglages.', true)
    throw e
  }
}
