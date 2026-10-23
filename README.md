# Voix MIDI (VST3)

Plugin audio-effet : il écoute le micro, détecte les notes (chant, sifflement, fredonnement) et enregistre une boucle MIDI synchronisée sur le tempo d'Ableton.

## Compiler (sans rien installer)
1. Crée un dépôt GitHub et pousse ce dossier.
2. Onglet Actions > Build VST3 > télécharge l'artefact Windows ou macOS.
3. Copie `Voix MIDI.vst3` dans `C:\Program Files\Common Files\VST3` (Windows) ou `~/Library/Audio/Plug-Ins/VST3` (Mac).
   Mac : `xattr -dr com.apple.quarantine "Voix MIDI.vst3"` puis rescanne les plugins.

## Utiliser dans Ableton
1. Pose « Voix MIDI » comme effet sur une piste audio dont l'entrée est ton micro (monitoring sur In).
2. Lance la lecture, clique Enregistrer : l'enregistrement démarre à la prochaine mesure.
3. Glisse le bloc rose vers une piste MIDI pour créer le clip (ou Exporter .mid).

## Mode live (vers d'autres VST)
Ableton ne laisse pas un VST envoyer du MIDI vers une autre piste. Le plugin envoie donc les notes vers un port MIDI virtuel :
1. Installe loopMIDI (Tobias Erichsen) et crée un port.
2. Dans le plugin : coche « Mode live », choisis le port dans « Sortie MIDI » (bouton Actualiser si absent).
3. Ableton > Préférences > Link/Tempo/MIDI : active « Track » sur l'entrée de ce port.
4. Sur une piste MIDI avec un instrument, mets « MIDI From » sur ce port, monitoring sur In.
Latence d'environ 70 ms (détection de hauteur). La gamme et l'octave s'appliquent aussi en live.
