import { registerSW } from 'virtual:pwa-register'
import './style.css'
import { initInstall } from './lib/install'
import { figer, initStore, onChange, state, type Direction } from './store'
import type { Passage } from './lib/depart'
import { initInstallUi, renderInstall } from './ui/install'
import { renderNext } from './ui/next'
import { initSettingsUi, renderSelection } from './ui/settings'
import { renderVerdict } from './ui/verdict'

registerSW({ immediate: true })

initInstallUi()
initInstall(renderInstall)
initSettingsUi()
onChange(() => {
  renderSelection()
  renderVerdict()
  renderNext()
})
initStore()

declare global {
  interface Window {
    __ta: {
      state: typeof state
      simuler: (secondes: number[], direction?: Direction) => void
    }
  }
}

window.__ta = {
  state,
  simuler(secondes, direction) {
    const d = direction ?? state.selection?.directions[0]
    if (!d) throw new Error('Choisis d\'abord une direction.')
    const passages: Passage[] = secondes.map((s) => ({
      ligne: d.ligne, idLigne: d.idLigne, destination: d.destination, sensAller: d.sensAller,
      idArret: 'simu', secondes: s, fiable: true, couleurFond: '00A5C2', couleurTexte: 'FFFFFF',
    }))
    figer(passages)
  },
}
