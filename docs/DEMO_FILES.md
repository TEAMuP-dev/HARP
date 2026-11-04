# Demo Files

The demo files are short inputs for trying the models on the Home tab of HARP. They are packaged
as `HARP-demo-files.zip`, an asset of the
[4.0.0 release](https://github.com/TEAMuP-dev/HARP/releases/tag/v4.0.0) linked from the
[download page](https://harp.tools/download/), and this file is the README inside the package.

Drag a file onto the input track of a model, or add it to the media clipboard with the
`File > Open` menu item, and drag a result from one model onto the input track of another to
chain them.

The recordings are excerpts of works released into the public domain or under a Creative Commons
license, cut to at most 30 seconds and faded, and every WAV file is 16-bit at 44.1 kHz. Some files
belong together: a piano recording comes with the score and the MIDI of the same piece, a reading
comes clean and with noise added, and a band in MIDI comes with its melody alone and with a
prompt built from it. The source and license of each file are listed under [Sources](#sources).

## A Piano Piece, Its Score, and Its MIDI

The Aria of the Goldberg Variations, BWV 988, by J.S. Bach.

- **`piano_bach_aria_30s.wav`**: the opening, about the first seven bars, played by Kimiko
  Ishizaka, in stereo.
  Try Transkun or High-Resolution Piano Transcription (audio to MIDI, to compare with the MIDI
  below), S-KEY (key), Audio Inpainting Diffusion, and AudioSR.
- **`piano_bach_variation1_30s.wav`**: the first 30 seconds of Variation 1 from the same
  recording, fast two-voice writing, for transcription under harder conditions.
  Try Transkun, Beat This!, and Chordino.
- **`score_bach_aria.musicxml`**: the first eight bars of the Aria as a MusicXML piano score.
  Try DExter (an expressive performance of the score, as MIDI, to compare with the recording).
- **`piano_bach_aria.mid`**: the same eight bars as MIDI, at a fixed tempo with no dynamics.
  Try MIDI Synthesizer (rendering), DDSP-Piano (rendering with a piano model), and Multitrack
  Music Transformer (continuation).

## A Reading, Clean and Noisy

The first three stanzas of "Eldorado" by Edgar Allan Poe, read by Shurtagal for LibriVox, in
mono.

- **`speech_eldorado_27s.wav`**: the reading.
  Try Whisper (transcription), VoiceCraft (editing), and OmniVoice, Vevo2, or IndexTTS2 (as the
  reference voice).
- **`speech_eldorado_noisy_27s.wav`**: the same reading mixed with cafeteria noise at a
  signal-to-noise ratio of 6 dB.
  Try DeepFilterNet or VoiceFixer (enhancement), then Whisper on the result and on the noisy input.

## A Band as MIDI

Sixteen seconds of music for a small band, composed for HARP, as MIDI.

- **`band_16s.mid`**: five tracks, with the drums on channel 10.
  Try MIDI Synthesizer (rendering), BACHI (chords), and Multitrack Music Transformer
  (continuation).
- **`melody_16s.mid`**: the lead line alone, on one track.
  Try MIDI-DDSP and MIDI Synthesizer (rendering).
- **`prompt_band_melody_16s.mid`**: the first five seconds of the band under the whole lead line,
  which is on Voice Oohs and continues alone after the band stops.
  Try Anticipatory Music Transformer with the `Melody Instrument` dropdown set to
  `53. Voice Oohs`. It keeps the band and writes an accompaniment under the rest of the melody.

## On Their Own

- **`song_once_tomorrow_30s.wav`**: "Once Tomorrow" by Josh Woodward, a song for voice, guitars,
  and drums, from the groove of the intro to the end of the first verse, in stereo.
  Try Demucs or BS-RoFormer (stems), Beat This! or BeatNet (beats), Chordino (chords), S-KEY
  (key), SongFormer (structure), and Whisper (lyrics).
- **`song_rampi_rampi_30s.wav`**: "Rampi Rampi" by Turku, Nomads of the Silk Road, an
  instrumental Turkish dance tune played on the saz, a long-necked lute, over layered hand
  percussion, in stereo.
  Try Beat This! or BeatNet (beats), S-KEY (key), Demucs (how an ensemble without a drum kit
  separates), and VampNet (variations).
- **`singing_dona_nobis_pacem_22s.wav`**: the canon "Dona nobis pacem", sung with vibrato by a
  trained singer alone in a studio, in mono.
  Try GAME (singing transcription), AnyAccomp (an accompaniment for the voice), and SoulX-Singer
  (as the reference voice).
- **`drums_rock_24s.wav`**: a rock drum kit, played live, in stereo.
  Try Beat This! or BeatNet (beats), LarsNet (kick, snare, toms, hi-hat, and cymbals), and TRIA
  (rhythm prompts).

## Sources

Each recording was cut to the excerpt given here and faded at its ends.

- **`piano_bach_aria_30s.wav`** and **`piano_bach_variation1_30s.wav`**: J.S. Bach, Goldberg
  Variations, BWV 988, played by Kimiko Ishizaka in the
  [Open Goldberg Variations](https://archive.org/details/OpenGoldbergVariations) (2012), the Aria
  from 0:01 to 0:31 and Variation 1 from 0:00 to 0:30.
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- **`score_bach_aria.musicxml`** and **`piano_bach_aria.mid`**: the first eight bars of the
  [Mutopia Project](https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=979) typesetting of
  the Aria by JD Erickson (2013), converted from its LilyPond MIDI with music21.
  [Creative Commons Attribution-ShareAlike 3.0](https://creativecommons.org/licenses/by-sa/3.0/).
- **`speech_eldorado_27s.wav`** and **`speech_eldorado_noisy_27s.wav`**: "Eldorado" by Edgar
  Allan Poe (1849), read by Shurtagal for LibriVox in
  [Edgar Allan Poe Poems](https://librivox.org/edgar-allan-poe-poems-by-edgar-allan-poe/), the
  first three stanzas, without the announcement. The text and the recording are in the public
  domain. The noise is "Crowd at Cafeteria" by sonically_sound on
  [Freesound](https://freesound.org/s/625112/),
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).
- **`band_16s.mid`**, **`melody_16s.mid`**, and **`prompt_band_melody_16s.mid`**: composed for
  testing HARP.
- **`song_once_tomorrow_30s.wav`**: "Once Tomorrow" by Josh Woodward, from the album Breadcrumbs
  (2009), 0:13 to 0:43.
  [Creative Commons Attribution 3.0 United States](https://creativecommons.org/licenses/by/3.0/us/).
  The file came from Wikimedia Commons:
https://commons.wikimedia.org/wiki/File:Josh_Woodward_-_Breadcrumbs_-_09_-_Once_Tomorrow.ogg
- **`song_rampi_rampi_30s.wav`**: "Rampi Rampi" by Turku, Nomads of the Silk Road, from the album
  Alleys of Istanbul, 1:00 to 1:30.
  [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/). The file came
  from Wikimedia Commons:
https://commons.wikimedia.org/wiki/File:Turku_Nomads_of_the_Silk_Road_-_05_-_-Rampi_Rampi.ogg
- **`singing_dona_nobis_pacem_22s.wav`**: "Dona nobis pacem" sung with vibrato by singer female5
  of [VocalSet](https://zenodo.org/records/1442513) (Wilkins, Seetharaman, Wahl, and Pardo,
  2018), the whole take, with the silence around it cut.
  [Creative Commons Attribution 4.0](https://creativecommons.org/licenses/by/4.0/).
- **`drums_rock_24s.wav`**: "Rock Drum Solo #2" by trevormoye95 on
  [Freesound](https://freesound.org/s/569929/), the first 24 seconds.
  [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/).

## Updating the Package

Attach a new `HARP-demo-files.zip` to the latest release, point `demoFiles` in
`website/download/release.data.mjs` at that release, and keep this page in step with the
contents, since the README in the package is a copy of it.
