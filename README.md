# Tram alerte

Dans combien de temps je dois partir pour attraper le prochain tram ? L'appli Ginko donne le
temps d'attente à l'arrêt, celle-ci soustrait le temps de marche et affiche le temps qu'il
reste pour partir de chez soi, avec un verdict en couleur : vert "tu as le temps", orange
"prépare-toi", rouge "pars maintenant". Les passages trop proches pour être attrapés sont
listés en gris, le verdict porte sur le premier tram atteignable.

Page unique, 100 % statique, en ligne sur https://jlzdev.github.io/tramalerte/,
installable sur l'écran d'accueil depuis le menu du navigateur (PWA). Réseau Ginko
(Besançon) uniquement.

## Données

Temps réel [Ginko](https://api.ginko.voyage/) (Keolis Besançon Mobilités), licence ODbL.
L'API est gratuite mais demande une clé : soit la clé d'essai du jour (valable 24 h, à
recopier depuis `DR/getCleTemporaire.do`), soit une clé définitive demandée par mail à
ginko.support-ssi@keolis.com (objet "Demande de clé API"). La clé se colle dans les réglages
de l'appli et reste dans le navigateur (localStorage), elle n'est jamais dans ce dépôt ni
dans le site publié. L'arrêt, les directions et les temps de marche restent aussi dans le
navigateur, rien n'est collecté.

Méthodes utilisées (POST `https://api.ginko.voyage/<methode>.do`, paramètre `apiKey`,
réponse `{ ok, objets }` ou `{ ok: false, msg }`) :

- `TR/getTempsLieu` (`nom`) : prochains passages temps réel à tous les quais d'un arrêt,
  champs `tempsEnSeconde`, `fiable`, `numLignePublic`, `idLigne`, `destination`, `sensAller`.
- `DR/getArrets` : liste des arrêts pour la recherche par nom (les quais tram ont un id `t_`).
- `DR/getArretsProches` (`latitude`, `longitude`) : arrêts autour de la position.
- `DR/getLignes` : lignes et variantes, pour proposer les directions tram même quand aucun
  passage n'est annoncé.

Les méthodes JSON répondent avec `Access-Control-Allow-Origin: *`, sauf
`DR/getCleTemporaire` (d'où le copier-coller de la clé d'essai).

## Algorithme

Réglages : `depart` (minutes entre la décision de partir et l'arrivée à l'arrêt, chaussures
et marche comprises, défaut 5), `marge` (sécurité, défaut 1). Pour chaque passage :

```
tram  = tempsEnSeconde - (maintenant - heure de la réponse)
reste = tram - (depart + marge) * 60
```

| Etat | Condition | Affichage |
|---|---|---|
| raté | `reste < 0` | gris "trop tard", ne porte pas le verdict |
| pars maintenant | `0 <= reste < 1 min` | rouge, décompte à la seconde |
| prépare-toi | `1 min <= reste < 4 min` | orange, décompte à la seconde |
| tu as le temps | au-delà | vert, minutes restantes, heure de départ et heure du tram |
| pas de tram annoncé | aucun passage atteignable ou erreur | gris, message explicite |

Le décompte tourne localement à la seconde, Ginko est réinterrogé toutes les 30 s (15 s quand
le départ est à moins de 5 min), jamais quand l'onglet est caché. Les horaires non fiables
(`fiable: false`) sont signalés "horaire théorique". Cette logique vit dans
[src/lib/depart.ts](src/lib/depart.ts), sans dépendance au navigateur, et
`npm run check:depart` la vérifie sur des scénarios.

## Développement

```bash
npm install
npm run dev
npm run build        # tsc --noEmit puis vite build
npm run check:depart # scénarios de la logique de départ
```

Dans la console du navigateur, `__ta.simuler([300, 1500])` fige l'appli sur des passages
simulés (secondes avant le tram) pour voir les états sans attendre un vrai tram.

## Déploiement

Chaque push sur `main` lance [deploy.yml](.github/workflows/deploy.yml) (scénarios, build,
GitHub Pages). Pas de version ni de tag, c'est un outil perso.

## Et après : un petit écran ESP32

La même logique tient en une requête HTTPS et trois champs. Un ESP32 avec une clé
définitive peut appeler `POST https://api.ginko.voyage/TR/getTempsLieu.do` avec
`apiKey=...&nom=<arrêt>`, filtrer `listeTemps` sur `idLigne` et `sensAller`, puis
appliquer les formules ci-dessus pour afficher le temps avant de partir.
