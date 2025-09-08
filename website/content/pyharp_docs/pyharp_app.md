# PyHARP Essentials

PyHARP provides a lightweight wrapper for [Gradio](https://www.gradio.app/) applications to ensure compatibility with HARP. A PyHARP application is structured as follows:

```
application_name/
└── app.py
└── requirements.txt
└── README.md
```

The `app.py` file defines the application through a __model card__ describing the underlying model, a list of __Gradio interactive components__ specifying the interface, and a __processing function__ for handling file input and output. We'll discuss these more in the following sections.

The `requirements.txt` file defines dependencies for the application, which are installed automatically if the application is hosted as a HuggingFace Space.

The `README.md` file provides additional information for hosting the application as a HuggingFace Space in its header. This file is not necessary if you only want to run your application locally. For more details, see the [HuggingFace Space configuration documentation](https://huggingface.co/docs/hub/spaces-config-reference).

### Running a PyHARP Application

PyHARP ships with a number of example HARP-compatible Gradio applications in the `examples/` directory. Once you have [installed PyHARP](/content/pyharp_docs/install.html), you can try running one of the example applications locally:

1. **Navigate your terminal to** `pyharp/examples/pitch_shifter/`
2. **Install dependencies:** `pip install -r requirements.txt`
3. **Run**: `python app.py`

Your terminal should display something like this:

```
* Running on local URL:  http://127.0.0.1:7860
* Running on public URL: https://8661b0cf18d5cf17ec.gradio.live

This share link expires in 1 week. For free permanent hosting and GPU upgrades, run `gradio deploy` from the terminal in the working directory to deploy to Hugging Face Spaces (https://huggingface.co/spaces)
```

Copy the public URL -- in this example, `https://8661b0cf18d5cf17ec.gradio.live` -- and open up HARP. As shown below, we can link HARP to our application by selecting `custom path...` from the model drop-down menu and pasting our URL.

<div style="max-width:720px;margin:1.5rem auto;">
  <video controls preload="metadata" width="100%">
    <source src="/content/images/pitch_shifter_example_pyharp.mp4" type="video/mp4" autoplay muted loop playsinline preload="metadata">
    Your browser doesn’t support the video tag.
  </video>
</div>





