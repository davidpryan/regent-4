# REGENT-4

REGENT-4 lets someone who thinks in harmony make a whole arrangement from chord changes. The white keys are the degrees of the key, so every chord is in key, and the black keys colour it (7th, sus2, sus4, add9, 6th, major/minor flipped, flat five). A pad voice-leads through the changes, a bass plays a figure from the root, two motifs arpeggiate the chord at their own rates, and a beat runs under them. A progression is recorded simply by playing it with REC, then plays back with SEQ while you play over it or change key.

![REGENT-4 on the FM-1's screen](manual/img/screen-conductor.png "REGENT-4 on the FM-1's screen")

Version {{VERSION}}. **Untested on a real FM-1 so far:** it builds, passes its tests in fm1-base's simulator, and
runs in the browser emulator. Installing is at your own risk; the installer can put M-VAVE's own firmware back.

- [Try it in the browser](emu/): the same code the FM-1 runs, with sound. No FM-1 needed.
- [Install](install/): from Chrome or Edge, with the FM-1 connected by USB.
- [Manual](manual/): what it is for, the controls, the screens.
- [Download {{PKG}}](firmware/{{PKG}}): for the command-line installer, `python3 base/tools/fm1_install.py {{PKG}}`.
- [Source](https://github.com/davidpryan/regent-4): MPL-2.0. Built on fm1-base and felucca-bsp, from Felucca by Leo
  Kuroshita, Hügelton Instruments. One of six sequencer firmwares built from a survey of sequencer archetypes.

M-VAVE and FM-1 are trademarks of their owners. REGENT-4 is not affiliated with M-VAVE.
