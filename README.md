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
# Voix-MIDI-vst3
