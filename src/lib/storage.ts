export function lsGet(key: string): string | null {
  try { return localStorage.getItem(key) } catch { return null }
}

export function lsSet(key: string, value: string): void {
  try { localStorage.setItem(key, value) } catch { /* stockage indisponible */ }
}

export function lsDel(key: string): void {
  try { localStorage.removeItem(key) } catch { /* stockage indisponible */ }
}

export function lsJson<T>(key: string): T | null {
  try { return JSON.parse(lsGet(key) ?? 'null') as T | null } catch { return null }
}
