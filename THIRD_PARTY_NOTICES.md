# Third-Party Notices

HARP is released under the [BSD 3-Clause License](LICENSE). It builds on the work listed below, each
part of which remains under its own license. The code under `src/external/` is kept as it was
received, so the attribution it calls for is given here and in the `About HARP` window of the app.

## Bundled with the App

| Work | Use in HARP | License |
| --- | --- | --- |
| [JUCE](https://juce.com) 8 | Application framework, in the `JUCE/` submodule | [AGPLv3](https://www.gnu.org/licenses/agpl-3.0.en.html) or the [JUCE licence](https://juce.com/legal/juce-8-licence/) |
| [Font Awesome](https://fontawesome.com/v4/) 4.7.0 by Dave Gandy | Icon font, embedded in `src/external/fontawesome/data/` | [SIL OFL 1.1](https://scripts.sil.org/OFL) |
| [Fontaudio](https://github.com/fefanto/fontaudio) 1.0 by `@fefanto` | Icon font and its helper, in `src/external/fontaudio/` | Icons [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), fonts [SIL OFL 1.1](https://scripts.sil.org/OFL), code [MIT](https://opensource.org/licenses/MIT) |
| [magic_enum](https://github.com/Neargye/magic_enum) 0.9.6 by Daniil Goncharov | Enum names, in `src/external/magic_enum.hpp` | [MIT](https://opensource.org/licenses/MIT) |

JUCE itself bundles further libraries, among them the FLAC and Ogg Vorbis codecs, zlib, libpng,
jpeglib, HarfBuzz, and SheenBidi. `JUCE/LICENSE.md` lists them with their licenses.

## Adapted Code

| Work | Use in HARP | License |
| --- | --- | --- |
| [danlin_modules](https://github.com/danlin/danlin_modules) by `@danlin` | The icon helper in `src/external/fontawesome/src/` | [MIT](https://opensource.org/licenses/MIT) |
| [Piano-Roll-Editor](https://github.com/Sjhunt93/Piano-Roll-Editor) by `@Sjhunt93` | The piano roll in `src/media/pianoroll/` | [MIT](https://opensource.org/licenses/MIT) |
| [JUCE tutorials](https://juce.com/tutorials/tutorial_synth_using_midi_input/) | The sine synthesizer in `src/media/pianoroll/SynthAudioSource.h` | [ISC](https://www.isc.org/licenses/) |
| [Pamplejuce](https://github.com/sudara/pamplejuce) by `@sudara` | The build workflow in `.github/workflows/build.yml` | [MIT](https://opensource.org/licenses/MIT) |

## Adding to This List

Vendored code goes under `src/external/` unmodified, and its attribution goes here. A work that asks
for attribution in the product itself, as CC BY does, is also named in the `About HARP` window
(`src/windows/AboutWindow.h`). The media in the demo package is credited in
[docs/DEMO_FILES.md](docs/DEMO_FILES.md), and each hosted model carries its own license on its card.
