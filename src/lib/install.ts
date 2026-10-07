import { lsGet, lsSet } from './storage'

interface BeforeInstallPromptEvent extends Event {
  prompt(): Promise<void>
  userChoice: Promise<{ outcome: 'accepted' | 'dismissed' }>
}

export type InstallState = 'hidden' | 'android' | 'ios'

const KEY_DISMISSED = 'tramalerte.installDismissed'

let state: InstallState = 'hidden'
let deferred: BeforeInstallPromptEvent | null = null
let onChange: (s: InstallState) => void = () => {}

function setState(s: InstallState): void {
  state = s
  onChange(s)
}

export function installState(): InstallState {
  return state
}

function isStandalone(): boolean {
  return matchMedia('(display-mode: standalone)').matches
    || (navigator as Navigator & { standalone?: boolean }).standalone === true
}

function isIos(): boolean {
  return /iPhone|iPad|iPod/.test(navigator.userAgent)
    || (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1)
}

export function initInstall(listener: (s: InstallState) => void): void {
  onChange = listener
  if (isStandalone() || lsGet(KEY_DISMISSED) === '1') return
  window.addEventListener('beforeinstallprompt', (e) => {
    e.preventDefault()
    deferred = e as BeforeInstallPromptEvent
    setState('android')
  })
  window.addEventListener('appinstalled', () => {
    deferred = null
    setState('hidden')
  })
  if (isIos()) setState('ios')
}

export async function promptInstall(): Promise<void> {
  const ev = deferred
  if (!ev) return
  deferred = null
  setState('hidden')
  try {
    await ev.prompt()
    const choice = await ev.userChoice
    if (choice.outcome === 'dismissed') lsSet(KEY_DISMISSED, '1')
  } catch { /* invite refusee par le navigateur */ }
}

export function dismissInstall(): void {
  setState('hidden')
  lsSet(KEY_DISMISSED, '1')
}
