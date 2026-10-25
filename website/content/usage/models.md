# Using Hosted Models

The models on the Home tab of HARP are hosted by us, as Hugging Face Spaces of the
[TEAMuP organization](https://huggingface.co/teamup-tech), or by Stability AI. They need nothing
beyond HARP itself. This page runs one of them, then explains the badges on a model card, API keys,
and what to expect from a Space that has been idle.

## Example: Separating Stems with Demucs

[Demucs](https://github.com/facebookresearch/demucs) separates a mix into drums, bass, vocals, and
everything else. It runs on a GPU through ZeroGPU, which Hugging Face lends to each request.

1. **Open the model.** On the `Home` tab, search for `demucs`, or select the `Separation` category,
   and click the Demucs card. The first time you open a ZeroGPU model without a Hugging Face key,
   HARP explains the GPU allowance on which it draws (see [API Keys](#api-keys)). Click `Open` to
   continue.
2. **Wait for it to load.** The status area shows `QUERYING_CONTROLS` while HARP reads the interface
   of the model, then `READY`. A Space that has been idle takes a minute or two to wake first.
3. **Add a song.** Drag an audio file (WAV, AIFF, FLAC, OGG, or MP3) onto the `Input Audio` track,
   or click its folder icon to choose one.
4. **Choose a variant.** The `Demucs Model` dropdown offers `htdemucs`, which sounds best,
   `mdx_extra`, which is faster at a small cost in quality, and `mdx_extra_q`, a quantized
   `mdx_extra` that is the lightest of the three and sounds noticeably worse.
5. **Process.** Select `Process`. A typical song takes well under a minute once the Space is awake.
6. **Use the stems.** The `Drums`, `Bass`, `Vocals`, and `Other` output tracks fill in as the
   result arrives. Play them, save them, drag them into your DAW, or drag one into the media
   clipboard to use as the input of another model, _e.g._, the vocals into Whisper for a
   transcript.

<!-- TODO: screenshot. The Demucs tab after processing a song, with all four stems rendered in their
     output tracks.
![Demucs after separating a song into four stems](/images/demucs-stems.png)
-->

[Example 2](/content/pyharp_docs/example_demucs) walks through the code of this model.

## Badges

Each model card carries badges, which are worth reading before relying on a model:

- **License**: the terms of the code and weights of the model, _e.g._, `MIT` or `CC-BY-NC-4.0`. A
  non-commercial license (`NC`) means the output cannot be used commercially. Where code and weights
  are licensed differently, the more restrictive license is shown.
- **ZeroGPU**: the model runs on a GPU lent to each request, which spends a daily allowance.
- **Stability AI**: the model runs on the servers of Stability AI, and needs a key from them.
- **Availability**: `Sleeping` or `Starting` for a Space that will take a while to load, and
  `Unavailable`, `Try again`, or `Failed to load` once a load has failed.

<!-- TODO: screenshot. A row of model cards on the Home tab, showing a license badge, a ZeroGPU
     badge, a Sleeping badge, and the Stability AI badge.
![Model cards with license, ZeroGPU, Sleeping, and Stability AI badges](/images/model-badges.png)
-->

<!--@include: @/../README.md#licenses-->

## API Keys

<!--@include: @/../README.md#api-keys-->

The allowance for each kind of account is listed under
[Plans and Hardware](/content/hosting/spaces#plans-and-hardware). A Hugging Face key also opens any
private Space that its account can see. `File > Settings > API Keys` links to where each service
issues keys, and checks a key when it is added.

<!-- TODO: screenshot. File > Settings > API Keys, with the Hugging Face page selected and a key
     entered (obscured).
![The API Keys settings, with a Hugging Face key entered](/images/settings-api-keys.png)
-->

## Sleeping Spaces and Errors

A Space goes to sleep when nobody has used it for a while, and opening it wakes it. HARP waits
while it starts, which takes a minute or two, and the model then loads as usual.

When a request fails, HARP says why in a popup, and offers what helps: adding an API key, opening
the page of the model to check its status, opening the log, or reporting the problem through a
prefilled GitHub issue. A quota error clears once the allowance recovers, or at once with a key.

## Highlights

The Home tab is the full list. A few models to try first:

| Model | Task | Input | Output |
| :-- | :-- | :-- | :-- |
| [Demucs](https://huggingface.co/spaces/teamup-tech/Demucs) | Stem separation | Audio | Audio |
| [Mel-Band RoFormer](https://huggingface.co/spaces/teamup-tech/Mel-Band-RoFormer) | Vocal and instrumental separation | Audio | Audio |
| [S-KEY](https://huggingface.co/spaces/teamup-tech/S-KEY) | Key detection | Audio | Labels |
| [Beat This!](https://huggingface.co/spaces/teamup-tech/Beat-This) | Beat and downbeat tracking | Audio | Labels |
| [Transkun](https://huggingface.co/spaces/teamup-tech/Transkun) | Piano transcription | Audio | MIDI |
| [Anticipatory Music Transformer](https://huggingface.co/spaces/teamup-tech/Anticipatory-Music-Transformer) | MIDI accompaniment | MIDI | MIDI |
| [text2midi](https://huggingface.co/spaces/teamup-tech/text2midi) | MIDI generation | Text | MIDI |
| [MusicGen](https://huggingface.co/spaces/teamup-tech/MusicGen) | Music generation | Text, audio | Audio |
| [VampNet](https://huggingface.co/spaces/teamup-tech/VampNet) | Variations on a recording | Audio | Audio |
| [DeepFilterNet](https://huggingface.co/spaces/teamup-tech/DeepFilterNet) | Speech denoising | Audio | Audio |
| [Whisper](https://huggingface.co/spaces/teamup-tech/Whisper) | Speech transcription | Audio | Text file |
| [MIDI Synthesizer](https://huggingface.co/spaces/teamup-tech/MIDI-Synthesizer) | MIDI rendering | MIDI | Audio |

To add a model of your own, see [Deploying Models with PyHARP](/content/pyharp_docs/setup).
