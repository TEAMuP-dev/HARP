# Available Models for HARP

<!-- TODO - organize this list by task / application -->

The following models are always available for use within HARP, and can be found in the drop-down menu at the top of the main panel:

- **Stable Audio (text-to-audio)** \([stability/text-to-audio](https://stableaudio.com/user-guide/text-to-audio)\) can generate music, sound effects and/or soundscapes, based on a text description.

- **Stable Audio (audio-to-audio)** \([stability/audio-to-audio](https://stableaudio.com/user-guide/audio-to-audio)\) enables the transfer of style or creation of variations based on text and audio conditioning.

<!-- - **MelodyFlow** \([hugggof/melodyflow](https://huggingface.co/spaces/hugggof/melodyflow)\) performs text-guided music generation and editing. Given an audio file and a text prompt describing a desired sound, MelodyFlow transforms the given audio to impart characteristics of the desired sound. -->

- **Text2Midi** \([teamup-tech/text2midi-HARP3](https://huggingface.co/spaces/teamup-tech/text2midi-HARP3)\) generates MIDI files from textual descriptions.

- **Demucs** \([teamup-tech/demucs-gen-input-output-harp-v3](https://huggingface.co/spaces/teamup-tech/demucs-gen-input-output-harp-v3)\) performs source separation on music, splitting it into "Drums", "Bass", "Vocals", and "Instrumental" stems.

- **High Resolution Piano Transcription** \([teamup-tech/solo-piano-audio-to-midi-transcription](https://huggingface.co/spaces/teamup-tech/solo-piano-audio-to-midi-transcription)\) converts audio of solo piano playing into a corresponding MIDI file.

- **Anticipatory Music Transformer** \([teamup-tech/AMT_HARP3](https://huggingface.co/spaces/teamup-tech/AMT_HARP3)\) performs harmonization on MIDI inputs, generating additional notes to provide the harmony for a given melody.

<!-- - **Timbre-Trap** \([npruyne/timbre-trap](https://huggingface.co/spaces/npruyne/timbre-trap), [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)\) performs timbre removal on music audio, reducing "textural" characteristics while preserving pitch (harmony/melody). -->

- **VampNet** \([teamup-tech/vampnet-music-HARP-V3](https://huggingface.co/spaces/teamup-tech/vampnet-music-HARP-V3)\) generates variations or "vamps" on music audio, and offers a variety of controls for determining how the vamps diverge from the original input audio.

- **Harmonic/Percussive Source Separation** \([teamup-tech/harmonic-percussive-HARP-3](https://huggingface.co/spaces/teamup-tech/harmonic-percussive-HARP-3)\) performs source separation on music by splitting it into "harmonic" and "percussive" tracks, allowing for the extraction of drum-like elements.
