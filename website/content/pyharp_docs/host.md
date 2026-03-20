# Hosting PyHARP Apps in the Cloud

Automatically generated Gradio endpoints are only available for 72 hours. If you'd like to keep the endpoint active and share it with other users, you can use [HuggingFace Spaces](https://huggingface.co/docs/hub/spaces-overview) (similar hosting services are also available) to host your PyHARP app indefinitely:

## Gradio Endpoints
Gradio endpoints are the most convenient solution for a PyHARP application. It will allow you access to GPU computation with ZeroGPU hardware if you are a PRO subscriber, which is free from additional charges.

1. Create a new [HuggingFace Space](https://huggingface.co/new-space), and choose Gradio as the Space SDK.
2. Choose the Blank template.
3. You can choose ZeroGPU hardware if you are a PRO subscriber and your application requires GPU computation. You can also customize the hardware.
4. Clone the initialized repository locally:
```bash
git clone https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>
```
5. Add your files to the repository, commit, then push to the `main` branch:
```bash
git add .
git commit -m "initial commit"
git push -u origin main
```
6. Here are the suggested configurations in the related files.
   - `README.md`
  
     Set __sdk_version__ to __5.28.0__. This is the recommended version, as HARP may not work with the very latest or earlier versions of Gradio.

   - `requirements.txt`

     This is the place where you put all your required **pip** packages. You should put in the latest PyHARP:
     ```
     git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0
     ```
     If you are using Gradio endpoints, you don't have to include the gradio package here.

   - `packages.txt`
     
     This is the place to put **apt-get install** debian packages, which are required by some models.

## Docker Endpoints
PyHARP requires `gradio==5.28.0`, which requires `python>=3.10`. Some previous models are developed with `python=3.9` or prior version, which may cause issues when upgrading python or other packages. For example, the `numpy.float` and `numpy.int` deprecation in numpy version `1.24` breaks some old packages. Therefore, we may need to run patch fixes during deployment to modify the affected files in the package. However, this is not supported by the highly-modularized Gradio spaces.

Using Docker endpoints can help you fix this issue. The Docker will allow you to customize the deployment, which will make room for potential patches and fixes. However, the ZeroGPU hardware will not be available for Docker spaces and you need to pay for the GPU usage.

1. Create a new [HuggingFace Space](https://huggingface.co/new-space), and choose Docker as the Space SDK.
2. Choose the Blank template.
3. Clone the initialized repository locally:
```bash
git clone https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>
```
4. Add your files to the repository, commit, then push to the `main` branch:
```bash
git add .
git commit -m "initial commit"
git push -u origin main
```
5. Configurations
    - `README.md`

      Set **app_port** to `<PORT>`.

    - `requirements.txt` and `packages.txt`

      Similar in the Gradio endpoint setting, they are for **pip** and **apt-get** packages. You are going to install them through the `Dockerfile`, which will be introduced in the next step.
      
      For `requirements.txt`, you should include:
      ```
      gradio==5.28.0
      git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0
      ```

    - `Dockerfile`

      Here is an example `Dockerfile` configuration, which will install the general packages and fix the `madmom` package.

      ```Docker
      FROM python:3.10-slim # Set python version

      WORKDIR /app
      COPY packages.txt /app/packages.txt

      # System deps for building packages from source
      RUN apt-get update
      RUN xargs apt-get install -y --no-install-recommends < /app/packages.txt
      RUN rm -rf /var/lib/apt/lists/*

      COPY requirements.txt /app/requirements.txt
      # disable build isolation so Cython installed in the environment is visible at build time
      ENV PIP_NO_BUILD_ISOLATION=1
      RUN pip install --no-cache-dir -U pip wheel Cython
      RUN pip install --no-cache-dir setuptools==80.9.0
      RUN pip install --no-cache-dir -r /app/requirements.txt
      RUN pip install --no-cache-dir --no-build-isolation madmom

      # madmom patch
      COPY patch_madmom.py /app/scripts/patch_madmom.py # A patch fix in the repo
      RUN python /app/scripts/patch_madmom.py
      RUN python -c "import madmom; print('madmom import OK')"

      # Copy the rest of the repo
      COPY . /app

      # HF Spaces routes traffic to $PORT. Gradio should listen on it.
      ENV PORT=<PORT> # <PORT> in README.md
      EXPOSE <PORT>

      # Run the app
      CMD ["python", "app.py"]
      ```

---
Your PyHARP app will then begin running at `https://huggingface.co/spaces/<USERNAME>/<SPACE_NAME>`. The shorthand `<USERNAME>/<SPACE_NAME>` can also be used within HARP to reference the endpoint. The app deployed by the two methods will have the same UI and functionality.

Here are a few tips and best-practices when dealing with HuggingFace Spaces:
- Spaces operate based off of the files in the `main` branch
- An [access token](https://huggingface.co/docs/hub/security-tokens) may be required to push commits to HuggingFace Spaces
- A `.gitignore` file should be added to maintain repository orderliness (_e.g._, to ignore `src/_outputs`)

For more information, please refer to the offical document from Hugging Face about [Spaces](https://huggingface.co/docs/hub/spaces).
