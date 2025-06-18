# Available Models for HARP

The following models are always available for use within HARP, and can be found in the `choose a model` drop-down menu at the top of the screen:

- **VampNet** \([hugggof/vampnet-music](https://huggingface.co/spaces/hugggof/vampnet-music)\) generates variations or "vamps" on music audio, and offers a variety of controls for determining how the vamps diverge from the original input audio.

- **MelodyFlow** \([hugggof/melodyflow](https://huggingface.co/spaces/hugggof/melodyflow)\) performs text-guided music generation and editing. Given an audio file and a text prompt describing a desired sound, MelodyFlow transforms the given audio to impart characteristics of the desired sound.
- **Demucs** \([cwitkowitz/demucs-cpu](https://huggingface.co/spaces/cwitkowitz/demucs-cpu)\) performs source separation on music audio, extracting or "isolating" a specified instrument type from a full recording.
- **The Anticipatory Music Transformer** \([lllindsey0615/pyharp_AMT](https://huggingface.co/spaces/lllindsey0615/pyharp_AMT)\) performs harmonization on MIDI inputs, generating additional notes to provide the harmony for a given melody.
- **Timbre-Trap** \([npruyne/timbre-trap](https://huggingface.co/spaces/npruyne/timbre-trap), [cwitkowitz/timbre-trap](https://huggingface.co/spaces/cwitkowitz/timbre-trap)\) performs timbre removal on music audio, reducing "textural" characteristics while preserving pitch (harmony/melody).
- **Harmonic/Percussive Source Separation** \([xribene/harmonic_percussive](https://huggingface.co/spaces/xribene/harmonic_percussive_v5)\) performs source separation on music audio by splitting it into "harmonic" and "percussive" tracks, allowing for the extraction of drum-like elements.
