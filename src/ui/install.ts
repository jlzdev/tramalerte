import { dismissInstall, promptInstall, type InstallState } from '../lib/install'
import { el } from './dom'

export function renderInstall(s: InstallState): void {
  const box = el('install')
  box.classList.toggle('hidden', s === 'hidden')
  box.classList.toggle('flex', s !== 'hidden')
  el('install-go').classList.toggle('hidden', s !== 'android')
  el('install-aide').textContent = s === 'ios'
    ? 'Bouton Partager, puis "Sur l\'écran d\'accueil".'
    : 'Comme une appli, sans passer par un store.'
}

export function initInstallUi(): void {
  el('install-go').addEventListener('click', () => { void promptInstall() })
  el('install-non').addEventListener('click', dismissInstall)
}
