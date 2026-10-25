# Adding an API Provider

HARP reaches every model through a _client_, which translates the requests of HARP into the API of a
provider. [`GradioClient`](https://github.com/TEAMuP-dev/HARP/blob/main/src/clients/GradioClient.h)
serves any Gradio app, wherever it is hosted, and
[`StabilityClient`](https://github.com/TEAMuP-dev/HARP/blob/main/src/clients/providers/stability/StabilityClient.h)
calls Stability AI's REST API. A service with an API of its own needs a client of its own. This
guide describes what one involves, with `StabilityClient` as the reference.

Consider the alternative first. A [PyHARP](https://github.com/TEAMuP-dev/pyharp) app can call any
API from its processing function, and HARP needs no change to use it (see
[Using Third-Party APIs](https://harp.tools/content/hosting/third_party.html)). A client of
its own is worth adding when each user should bring their own key to the service, so that their
requests are billed to them, and when many users already hold an account with it.

## Table of Contents

- [How HARP Uses a Client](#how-harp-uses-a-client)
- [Name the Provider](#name-the-provider)
- [Describe the Models](#describe-the-models)
- [Write the Client](#write-the-client)
- [Register the Client](#register-the-client)
- [Generalize What Names Stability AI](#generalize-what-names-stability-ai)
- [Test It](#test-it)

## How HARP Uses a Client

A [`Model`](https://github.com/TEAMuP-dev/HARP/blob/main/src/Model.h) owns one client, chosen by the
path of the model, and calls it in this order:

1. `resolveCanonicalPath` reduces the path the user entered to the one form by which the provider
   knows the model.
2. `queryControls` returns the interface of the model: its card, its input tracks and controls, and its
   output tracks. `Model` parses it into the components HARP draws.
3. On `Process`, `uploadFile` is called for each input file, and `wrapPayloadElement` for each value,
   in the order of the inputs. `process` then sends the request, and returns the output files and
   labels.
4. On `Cancel`, `cancel` asks the provider to stop the job.

These calls run on the worker threads of the model tab, never on the message thread, so a client may
block on the network. The exception is `getBuiltInModelCards`, which calls `queryControls` for each
built-in model when the catalog of the Home tab is created, so for those models it has to answer from
embedded data (see [Describe the Models](#describe-the-models)). Every call that can fail returns an
`OpResult`.

## Name the Provider

Add the provider to the `Provider` enum in
[`src/utils/Providers.h`](https://github.com/TEAMuP-dev/HARP/blob/main/src/utils/Providers.h), and
its name, as users know it, to `getProviderName`. Errors, the settings window, and the Home tab show
that name.

## Describe the Models

A Gradio app describes its own interface, but most APIs do not, so the description of each model
ships with HARP as JSON, in the same form a PyHARP app returns from its `controls` endpoint.
[`text-to-audio.json`](https://github.com/TEAMuP-dev/HARP/blob/main/src/clients/providers/stability/models/text-to-audio.json)
is the reference:

```json
{
  "card": {
    "name": "Stable Audio 2.0 (Text to Audio)",
    "description": "Generates 44.1 kHz stereo audio from a text prompt with Stability AI's Stable Audio 2.0. Requires a Stability AI API key, added under Settings > API Keys, and each request spends credits from that account.",
    "author": "Stability AI",
    "tags": ["category:generation", "subcategory:holistic", "input:text", "output:audio/wav|mp3", "license:other"]
  },
  "inputs": [
    {
      "label": "Duration (s)",
      "info": "Length of the generated audio, in seconds.",
      "minimum": 1,
      "maximum": 190,
      "step": 1,
      "value": 10,
      "type": "slider"
    }
  ],
  "outputs": [
    {
      "label": "Generated Audio",
      "required": true,
      "type": "audio_track"
    }
  ]
}
```

- **`card`** follows the `ModelCard` of PyHARP. Write the tags out in full, `input:` and `output:` tags
  included, since nothing infers them here. The description should say that the model needs a key,
  and what each request costs.
- **`inputs`** and **`outputs`** list components in order, each with a `type`: `audio_track`,
  `midi_track`, `generic_file`, `slider`, `number_box`, `text_box`, `toggle`, or `dropdown` for an
  input, and a track or `json` for an output. The fields of each type are those of the matching
  class in the [`core.py`](https://github.com/TEAMuP-dev/pyharp/blob/main/pyharp/core.py)
  of PyHARP, _e.g._, `HarpSlider`.

Put the files under `src/clients/providers/<name>/models/`, and embed them with a binary data target
of their own in [`CMakeLists.txt`](https://github.com/TEAMuP-dev/HARP/blob/main/CMakeLists.txt),
linked to the application. Give the target its own `HEADER_NAME` and `NAMESPACE`, as
`model_taxonomy` does, so that its symbols cannot collide with those of `stability_controls`.

## Write the Client

Subclass [`Client`](https://github.com/TEAMuP-dev/HARP/blob/main/src/clients/Client.h) in
`src/clients/providers/<name>/<Name>Client.h`, and add the header to `target_sources` in
`CMakeLists.txt`, which lists every source explicitly.

**In the constructor**, set:

- `provider`, the enum value added above.
- `acceptHeader`, and `contentTypeJSONHeader` if requests carry JSON.
- `tokenValidationURL`, an endpoint that answers `200` to a request carrying a valid key, such as
  the account endpoint of the provider. `validateToken` checks a key there when a user adds one, and can
  be overridden where a provider checks keys differently.
- `tokenRegistrationURL`, the page where a user creates a key, which the settings window links.

**Implement:**

- `static bool matchesPathSpec(String modelPath)`, whether a path names one of the models of
  the provider, _e.g._, `stability/text-to-audio` or the full URL of its endpoint.
- `static StringArray getModelPaths()`, every model the provider offers, for the Home tab.
- `inferHostSlashModel`, `inferEndpointPath`, and `inferDocumentationPath`, which give the
  short form of a model, the URL that receives its requests, and the page HARP opens from the
  model card and from its error popups.
- `queryControls`, which returns the JSON of the model for the given path. `StabilityClient` reads it
  from binary data.
- `wrapPayloadElement`, which wraps each value before `Model` adds it to the payload.
  `StabilityClient` keeps the label of the component with each value, so that `process` can tell them
  apart.
- `process`, which sends the request and writes each result to a file. Name the file with
  `createResultFile`, which stamps it with the time it arrived, and append it to `outputFiles`. Fill
  `labels` if the provider returns annotations.

**Override where the provider needs it:**

- `uploadFile`, which by default passes the local path through, for a provider that takes files
  ahead of the request rather than inside it.
- `cancel`, which by default does nothing, for a provider that can stop a job.
- `canonicalizePath` and `resolveCanonicalPath`, for a provider whose models can be addressed in
  more than one way.

**Follow the conventions every client keeps:**

- **Open every connection with `createRequestStream`**, so that closing the tab of a model can abort
  its requests rather than wait for them.
- **Build headers with `getCommonHeaders` or `getJSONHeaders`**, which carry the key of the user as
  `Authorization: Bearer <key>`. A provider that expects the key elsewhere needs a hook for it in
  `Client`, not a copy of the header logic.
- **Send the key to the provider alone.** `GradioClient` withholds it from any host Hugging Face
  does not run, and a new client must be as strict about where its requests go.
- **Return failures through `fail(ClientError { ... })` or `fail(HttpError { ... })`**, which record
  the provider. HARP uses it to offer the right key when a request is refused with `401` or `403`.
  A failure with no fitting type gets a new error struct and `toUserMessage` overload in
  [`src/utils/Errors.h`](https://github.com/TEAMuP-dev/HARP/blob/main/src/utils/Errors.h).
- **Log at each request** with `DBG_AND_LOG("<Name>Client::<method>: ...")`, and pass headers through
  `toPrintableHeaders`, which redacts keys. Users attach their logs to public issues.

## Register the Client

[`src/utils/Clients.h`](https://github.com/TEAMuP-dev/HARP/blob/main/src/utils/Clients.h) decides
which client serves a path:

- Add a branch to `createClient`, which builds a client for a provider.
- Add a check to `createClientForPath` **before** the check for `GradioClient`, which is the
  catch-all for any path no other provider claims.
- Add the models of the provider to `getBuiltInModelCards`, so that they are listed on the Home tab
  without being entered as a custom path.

Then give users somewhere to put their key. Add the provider to the list of providers for which
`SharedAPIKeys` loads saved keys, in `Client.h`, and a `ProviderPage` for it to the `LoginTab`
constructor in
[`src/windows/settings/LoginTab.cpp`](https://github.com/TEAMuP-dev/HARP/blob/main/src/windows/settings/LoginTab.cpp).

## Generalize What Names Stability AI

Stability AI is the only provider besides Hugging Face so far, and a few places name it directly
rather than asking the client. A second provider has to generalize each of them, rather than add a
branch of its own beside it:

- `ModelCatalog` labels every built-in model `"Stability AI"`, and `CatalogEntry::isMetered` treats
  that label as the only provider that needs a key
  ([`src/utils/ModelCatalog.h`](https://github.com/TEAMuP-dev/HARP/blob/main/src/utils/ModelCatalog.h)).
- `ModelTabContainer::openFromHome` maps any metered model that is not on ZeroGPU to
  `Provider::Stability`, and words its notice for Stability AI
  ([`src/ModelTabContainer.h`](https://github.com/TEAMuP-dev/HARP/blob/main/src/ModelTabContainer.h)).
- The instructions on the Home tab for hiding metered models, and the last step of the welcome tutorial,
  name Stability AI in their text.

## Test It

There are no C++ unit tests, so build HARP and run it:

- Open each model from the Home tab, and by a custom path, and check that its card, controls, and
  tracks match its JSON.
- Add a valid key under `File > Settings > API Keys`, and check that the page reports it as valid.
  Add an invalid one, and check that it is refused.
- Process with a valid key, without a key, and with an invalid one. A refused request should offer
  to open the settings page of the provider, and the error should name the provider.
- Cancel a request in progress, close the tab of the model during one, and process with the network
  disconnected.
- Read the log, and check that no key appears in it.

Our [reviewing guidelines](https://github.com/TEAMuP-dev/HARP/blob/main/docs/REVIEWING.md) describe
the rest of what a pull request needs.
