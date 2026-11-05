# Using Hosted Models

The models on HARP's Home tab are hosted by us, as Hugging Face Spaces of the
[TEAMuP organization](https://huggingface.co/teamup-tech), or by Stability AI. They need nothing
beyond HARP itself. This page runs one of them, then explains the badges on a model card, API keys,
and what to expect from a Space that has been idle.

## Separating Stems with Demucs

[Demucs](https://github.com/facebookresearch/demucs) separates a mix into drums, bass, vocals, and
everything else. It runs on a GPU through ZeroGPU, which allocates one to each request.

1. **Open the model.** On the `Home` tab, search for `demucs`, or select the `Separation` category,
   and click the Demucs card. The first time you open a ZeroGPU model without a Hugging Face key,
   HARP explains the GPU allowance it spends (see [API Keys](#api-keys)). Click `Open` to continue.
2. **Wait for it to load.** The status area shows `QUERYING_CONTROLS` while HARP reads the interface
   of the model, then `READY`. A Space that has been idle takes a minute or two to wake first.
3. **Add a song.** Drag an audio file (WAV, AIFF, FLAC, OGG, or MP3) onto the `Input Audio` track,
   or click its folder icon to choose one.
4. **Choose a variant.** The `Demucs Model` dropdown offers `htdemucs`, which has the highest
   quality, `mdx_extra`, which is faster at a small cost in quality, and `mdx_extra_q`, a quantized
   `mdx_extra` that is the lightest of the three and noticeably lower in quality.
5. **Process.** Select `Process`. A typical song takes well under a minute once the Space is awake.
6. **Use the stems.** The `Drums`, `Bass`, `Vocals`, and `Other` output tracks fill in as the
   result arrives. Play them, save them, drag them into your DAW, or drag one into the media
   clipboard to use as the input of another model, _e.g._, the vocals into Whisper for a
   transcript.

![Demucs after separating a song into four stems](/images/demucs-stems.png)

[Example 2](/pyharp/example_demucs) walks through the code of this model.

## Badges

Each model card carries badges that state its license, its hardware, and its availability:

- **License**: the terms of the code and weights of the model, _e.g._, `MIT` or `CC-BY-NC-4.0`. A
  non-commercial license (`NC`) means the output cannot be used commercially. Where code and weights
  are licensed differently, the more restrictive license is shown.
- **ZeroGPU**: the model runs on a GPU allocated to each request, which spends the daily allowance
  of the caller.
- **Stability AI**: the model runs on Stability AI's servers, and needs a Stability AI key.
- **Availability**: `Sleeping` or `Starting` for a Space that has to start before it loads, and
  `Unavailable`, `Try again`, or `Failed to load` once a load has failed.

![Model cards with license, ZeroGPU, Sleeping, and Stability AI badges](/images/model-badges.png)

<!--@include: @/../README.md#licenses-->

## API Keys

<!--@include: @/../README.md#api-keys-->

The allowance for each kind of account is listed under
[Plans and Hardware](/hosting/spaces#plans-and-hardware). A Hugging Face key also opens any
private Space that its account can see. `File > Settings > API Keys` links to where each service
issues keys, and checks a key when it is added.

![The API Keys settings, with a Hugging Face key entered](/images/settings-api-keys.png)

## Sleeping Spaces and Errors

A Space sleeps after a period without requests, and opening it wakes it. Until the Space has
started, which takes a minute or two, HARP reports that it is still starting and marks its card
`Try again`. Open the model again once that time has passed.

When a request fails, HARP reports the cause in a popup, with the actions that apply: adding an API
key, opening the page of the model to check its status, opening the log, or reporting the problem
through a prefilled GitHub issue. A quota error clears once the allowance resets, or immediately
with a key.

## Highlights

HARP's Home tab lists every model we host, by category. Notable models include the following.

**Separation**

- [Demucs](https://huggingface.co/spaces/teamup-tech/Demucs) splits a mix into drums, bass, vocals,
  and other.
- [BS-RoFormer](https://huggingface.co/spaces/teamup-tech/BS-RoFormer) splits a mix into six stems,
  with guitar and piano among them.
- [LarsNet](https://huggingface.co/spaces/teamup-tech/LarsNet) splits a drum mix into kick, snare,
  toms, hi-hat, and cymbals.
- [AudioSep](https://huggingface.co/spaces/teamup-tech/AudioSep) extracts any sound described in
  text, such as a dog barking.

**Analysis**

- [S-KEY](https://huggingface.co/spaces/teamup-tech/S-KEY) finds the key, as a label over the audio.
- [Beat This!](https://huggingface.co/spaces/teamup-tech/Beat-This) marks beats and downbeats, and
  [Chordino](https://huggingface.co/spaces/teamup-tech/Chordino) labels the chords.
- [SongFormer](https://huggingface.co/spaces/teamup-tech/SongFormer) marks the sections of a song.
- [Transkun](https://huggingface.co/spaces/teamup-tech/Transkun) transcribes solo piano to MIDI, and
  [Omnizart](https://huggingface.co/spaces/teamup-tech/Omnizart) transcribes other instruments,
  drums, vocals, and chords.
- [Whisper](https://huggingface.co/spaces/teamup-tech/Whisper) transcribes speech to a text file.

**Generation**

- [MusicGen](https://huggingface.co/spaces/teamup-tech/MusicGen) generates music from text,
  following an optional melody.
- [ACE-Step](https://huggingface.co/spaces/teamup-tech/ACE-Step) generates a song with vocals from
  lyrics and a style.
- [text2midi](https://huggingface.co/spaces/teamup-tech/text2midi) generates MIDI from text, and
  [Anticipatory Music Transformer](https://huggingface.co/spaces/teamup-tech/Anticipatory-Music-Transformer)
  writes an accompaniment to a MIDI part.
- [AnyAccomp](https://huggingface.co/spaces/teamup-tech/AnyAccomp) writes an accompaniment to a
  vocal or lead recording.
- [VampNet](https://huggingface.co/spaces/teamup-tech/VampNet) makes variations on a recording, and
  [Stable Audio 3](https://huggingface.co/spaces/teamup-tech/Stable-Audio-3) restyles, inpaints, or
  continues one from a prompt.

**Synthesis**

- [MIDI Synthesizer](https://huggingface.co/spaces/teamup-tech/MIDI-Synthesizer) renders MIDI with
  a General MIDI soundfont, and [DDSP-Piano](https://huggingface.co/spaces/teamup-tech/DDSP-Piano)
  and [DDSP-Guitar](https://huggingface.co/spaces/teamup-tech/DDSP-Guitar) render it with expressive
  instrument models.
- [Kokoro](https://huggingface.co/spaces/teamup-tech/Kokoro) synthesizes speech from text, and
  [OmniVoice](https://huggingface.co/spaces/teamup-tech/OmniVoice) synthesizes it in the voice of a
  reference recording.
- [SoulX-Singer](https://huggingface.co/spaces/teamup-tech/SoulX-Singer) synthesizes singing in the
  voice of a reference recording.

**Effects and Enhancement**

- [micro-TCN](https://huggingface.co/spaces/teamup-tech/micro-TCN) emulates an LA-2A compressor,
  and [PANAMA](https://huggingface.co/spaces/teamup-tech/PANAMA) a guitar amplifier.
- [RemFX](https://huggingface.co/spaces/teamup-tech/RemFX) removes effects from a recording.
- [DeepFilterNet](https://huggingface.co/spaces/teamup-tech/DeepFilterNet) denoises speech, and
  [U-DREAM](https://huggingface.co/spaces/teamup-tech/U-DREAM) removes room reverb.
- [AudioSR](https://huggingface.co/spaces/teamup-tech/AudioSR) upsamples any audio to 48 kHz, and
  [Apollo](https://huggingface.co/spaces/teamup-tech/Apollo) restores audio degraded by MP3 encoding.

**Production**

- [Matchering](https://huggingface.co/spaces/teamup-tech/Matchering) masters a track to match a
  reference.
- [SonicMaster](https://huggingface.co/spaces/teamup-tech/SonicMaster) restores and masters by text
  instruction.
- [FxNorm-Automix](https://huggingface.co/spaces/teamup-tech/FxNorm-Automix) mixes up to four stems.

To add a model of your own, see [Deploying Models with PyHARP](/pyharp/setup).
