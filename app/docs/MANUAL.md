# REGENT-4

Play chords; four parts follow them. For the M-VAVE FM-1. One of six sequencer firmwares built from a survey of sequencer archetypes (see the companion survey). Built on fm1-base; makes its own sound and sends MIDI.

![REGENT-4 on the FM-1's screen](img/screen-conductor.png "REGENT-4 on the FM-1's screen")

## What it is for

**Archetype:** Harmonic conductor: Conductive Labs NDLR, chord tracks, Orba-style parts.

REGENT-4 lets someone who thinks in harmony make a whole arrangement from chord changes. The white
keys are the degrees of the key, so every chord is in key, and the black keys colour it (7th, sus2, sus4, add9, 6th,
major/minor flipped, flat five). A pad voice-leads through the changes, a bass plays a figure from the root, two
motifs arpeggiate the chord at their own rates, and a beat runs under them. A progression is recorded simply by
playing it with REC, then plays back with SEQ while you play over it or change key.

## What it does

- White keys 1-14: chords on degrees I-VII and an octave up; white 15: no chord; white 16: tap tempo.
- Black keys 1-7 held with a chord key (or pressed while one is held): 7, sus2, sus4, add9, 6, flip, b5. Black keys 8-11 mute the parts.
- PAD (hold, quarters, eighths, on change; voice-led, spread), BASS (8 figures), MOTIF A and B (12 figures + dice, 6 rates, 1-3 octaves), BEAT (6 grooves, fills before a change).
- Chords change at once, on the beat or at the bar; the next chord is shown while it waits.
- PROGRESSION of 16 chords with beats each: recorded by playing (REC), edited on its page (EDIT), played by SEQ.
- PRESETS changes key on the fly (SEL + PRESETS the scale). MIDI notes in choose the chord on that root.
- The chord's name (Am7, Fsus4) and numeral (vi, IV) large on the screen; the pad's voicing on a keyboard picture.

## The controls

Hold any button alone for a second for its help card on the screen.

| Control | What it does |
| --- | --- |
| white keys 1-14 | chords on the degrees |
| black keys 1-7 | colours |
| black keys 8-11 | mute PAD, BASS, MOTIF A, MOTIF B |
| white 15 / FX | no chord |
| white 16 | tap tempo |
| ALGO | the part; K1-K4 its four (figure, rate / octave, range / gate / spread, level) |
| PRESETS / SEL + PRESETS | key root / scale |
| SELECT | tempo |
| REC | record a progression by playing |
| SEQ | the progression plays |
| EDIT | the progression page |
| ARP | dice a motif |
| LFO | mute the part |
| OCT | the part's octave |

Common to all six: PLAY starts and stops, SAVE saves to the project slot, OCT- / OCT+ the octave.

## Screens

![The conductor screen](img/screen-conductor.png "Conductor")

![The chord screen](img/screen-chord.png "Chord")

![The progression screen](img/screen-progression.png "Progression")

![The setup screen](img/screen-setup.png "Setup")

## Install

Use the [installer page](../install/) in Chrome or Edge with the FM-1 connected by USB. Keep M-VAVE's stock firmware
and M-UPGRADE at hand: the installer can put it back. To build it yourself, see the
[source](https://github.com/davidpryan/regent-4).

## Status

0.1, untested on hardware. It builds for the FM-1 and passes its host-simulator tests; nothing has been run on a real
FM-1 yet.
