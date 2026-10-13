# Hosting PyHARP Apps in the Cloud (HuggingFace Spaces)
The share links Gradio generates expire after a week. To keep an endpoint active and share it with other users, host your PyHARP app on [HuggingFace Spaces](https://huggingface.co/docs/hub/spaces-overview) (similar hosting services are also available). Our [deployment guidelines](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md) describe the conventions the Spaces we host follow, from licensing and tags to hardware.

### Gradio Endpoints
This is the most convenient solution for hosting a PyHARP app. If you are a Hugging Face PRO subscriber, you can use [ZeroGPU](https://huggingface.co/docs/hub/spaces-zerogpu) to dynamically allocate GPU resources according to user requests without any additional charges. Non-PRO users can select from CPU environments or paid GPU options.

1. Create a new [HuggingFace Space](https://huggingface.co/new-space).
2. Choose Gradio as the SDK along with the blank template.
3. Select the desired hardware option.
4. Create the space and clone the initialized repository locally:
```bash
git clone https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>
```
5. Add your files to the repository, commit, then push to the `main` branch:
```bash
git add .
git commit -m "initial commit"
git push -u origin main
```
6. Configure the following repository files:
   - `README.md`
  
     Set __sdk_version__ to __6.24.0__, the recommended version of `gradio`. HARP requires at least `6.13.0`, since earlier versions do not forward the reason a job failed.

   - `requirements.txt`

     Place all of the required **pip** packages in this file. It should also include `pyharp`, pinned to the commit of a release rather than to a tag or branch, which can move:
     ```
     # pyharp v0.4.1
     git+https://github.com/TEAMuP-dev/pyharp.git@9a31acb35563c0bcf5a93f968d8285c7c3180aba
     ```
     Do not include `gradio` in this file, since `sdk_version` sets it.

   - `packages.txt`
     
     Place any necessary **apt-get install** debian packages in this file. Some models may require these.

### Docker Endpoints
Some models may have been developed with older versions of Python, and attempting to deploy them would lead to dependency issues. For example, the `numpy.float` and `numpy.int` deprecation in `numpy==1.24` breaks older packages such as `madmom`. Therefore, we may need to patch any corresponding source files during the deployment process. However, this is not supported by the highly-modularized Gradio SDK.

Using Docker endpoints can help circumvent these issues. Docker will allow you to customize the deployment, which makes room for any necessary patches. Note however that ZeroGPU is not available for Docker spaces, meaning you must pay to use GPU resources with this option.

1. Create a new [HuggingFace Space](https://huggingface.co/new-space).
2. Choose Docker as the SDK along with the blank template.
3. Select the desired hardware option.
4. Create the space and clone the initialized repository locally:
```bash
git clone https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>
```
5. Add your files to the repository, commit, then push to the `main` branch:
```bash
git add .
git commit -m "initial commit"
git push -u origin main
```
6. Configure the following repository files:
    - `README.md`

      Set **app_port** to any valid `<PORT>`.

   - `requirements.txt`

     Place all of the required **pip** packages in this file. It should also include the recommended version of `gradio`, and `pyharp` pinned to the commit of a release:
     ```
     gradio==6.24.0
     # pyharp v0.4.1
     git+https://github.com/TEAMuP-dev/pyharp.git@9a31acb35563c0bcf5a93f968d8285c7c3180aba
     ```

   - `packages.txt`
     
     Place any necessary **apt-get install** debian packages in this file. Some models may require these.

    - `Dockerfile`

      Installs the required **pip** and **apt-get** packages, and supports manual patching (_e.g._ of `madmom` in the following example):
      ```Docker
      FROM python:3.10-slim # Set python version

      WORKDIR /app
      COPY packages.txt /app/packages.txt

      # System dependencies for building packages from source
      RUN apt-get update
      RUN xargs apt-get install -y --no-install-recommends < /app/packages.txt
      RUN rm -rf /var/lib/apt/lists/*

      COPY requirements.txt /app/requirements.txt
      # Disable build isolation so Cython installed in the environment is visible at build time
      ENV PIP_NO_BUILD_ISOLATION=1
      RUN pip install --no-cache-dir -U pip wheel Cython
      RUN pip install --no-cache-dir setuptools==80.9.0
      RUN pip install --no-cache-dir -r /app/requirements.txt
      RUN pip install --no-cache-dir --no-build-isolation madmom

      # Patch madmom package
      COPY patch_madmom.py /app/scripts/patch_madmom.py # Script to patch madmom source files
      RUN python /app/scripts/patch_madmom.py
      RUN python -c "import madmom; print('madmom import OK')"

      # Copy remainder of the repo
      COPY . /app

      # HF Spaces route traffic to <PORT>
      # Gradio should listen accordingly
      ENV PORT=<PORT> # <PORT> in README.md
      EXPOSE <PORT>

      # Run the app
      CMD ["python", "app.py"]
      ```

---
Here are a few tips and best practices when dealing with HuggingFace Spaces:
- Spaces operate based off of the files in the `main` branch
- An [access token](https://huggingface.co/docs/hub/security-tokens) may be required to push commits to HuggingFace Spaces
- A `.gitignore` file should be added to maintain repository orderliness (_e.g._, to ignore `src/_outputs`)
- Pin versions for packages such as `numpy`, `torch`, and `torchaudio` to avoid unexpected build issues caused by their latest versions. ZeroGPU requires `torch>=2.8.0`

For more information, please refer to the official documentation from Hugging Face about [Spaces](https://huggingface.co/docs/hub/spaces).

## Accessing Within HARP
PyHARP apps deployed to HuggingFace will begin running at `https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>`. The shorthand `<USERNAME>/<SPACE_NAME>` can also be used within HARP to reference the endpoint. The two deployment methods above produce identical UIs and functionality.
