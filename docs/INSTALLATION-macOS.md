# Installation sur macOS

Cette archive contient une version Universal 2 compatible avec les Mac Apple
Silicon et Intel, à partir de macOS 11.

## Standalone

Copier `Standalone/NDLR.app` dans le dossier `/Applications`, puis ouvrir
l’application.

## Audio Unit

Copier `AU/NDLR.component` dans :

```text
~/Library/Audio/Plug-Ins/Components/
```

Redémarrer ensuite le séquenceur afin qu’il actualise ses Audio Units.

## VST3

Copier `VST3/NDLR.vst3` dans :

```text
~/Library/Audio/Plug-Ins/VST3/
```

Relancer ou rescanner les plug-ins dans le séquenceur.

## Signature

Ce build de développement est signé localement de façon ad hoc et n’est pas
notarisé par Apple. macOS peut donc demander une confirmation de sécurité lors
de sa première ouverture.
