from __future__ import annotations

import ast
import hashlib
import json
import os
import queue
import re
import shutil
import socket
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, Iterable, List, Mapping, Optional, Tuple
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


JSON = Dict[str, Any]
HUGGING_FACE_BASE = "https://huggingface.co"
GITHUB_API_BASE = "https://api.github.com"
GITHUB_RAW_BASE = "https://raw.githubusercontent.com"
KNOWN_HYPHENATED_SPACE_ORGS = ("teamup-tech",)

# pyharp v0.3.0 hard-pins gradio==5.28.0. HARP wrappers must therefore align the
# Space on this exact gradio version (both the README sdk_version that Hugging
# Face force-installs and any gradio pin in requirements.txt), or the build fails
# with a ResolutionImpossible conflict.
HARP_GRADIO_VERSION = "5.28.0"
PYHARP_REQUIREMENT = "git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0"

HARP_FRIENDLY_TASKS = {
    "audio-to-audio",
    "audio-classification",
    "automatic-speech-recognition",
    "text-to-audio",
    "text-to-speech",
}

PERMISSIVE_LICENSES = {
    "apache-2.0",
    "mit",
    "bsd",
    "bsd-2-clause",
    "bsd-3-clause",
    "cc-by-4.0",
    "cc-by-sa-4.0",
    "openrail",
    "openrail++",
}

NONCOMMERCIAL_LICENSES = {
    "cc-by-nc-4.0",
    "cc-by-nc-sa-4.0",
    "cc-by-nc-nd-4.0",
}

WEIGHT_FILE_SUFFIXES = (".bin", ".safetensors", ".pt", ".ckpt", ".onnx")
_SR_RE = re.compile(r"(\d{2,3})[ ._-]?k(?:hz| ?Hz)", re.IGNORECASE)
_CHANNELS_RE = re.compile(r"\b(mono|stereo)\b", re.IGNORECASE)
_LOCAL_URL_RE = re.compile(r"https?://(?:127\.0\.0\.1|0\.0\.0\.0|localhost):\d+")
_PUBLIC_URL_RE = re.compile(r"https?://[\w.-]+\.gradio\.live")


class EndpointProbeError(RuntimeError):
    """Raised when a candidate endpoint does not expose HARP controls."""


class VenvSetupError(RuntimeError):
    """Raised when building the isolated smoke-test venv fails (e.g. pip error)."""


class DeploySpaceError(RuntimeError):
    """Raised when deploying a generated package to a Hugging Face Space fails."""


@dataclass
class SpaceCandidate:
    """A Hugging Face Space that may be packageable for HARP."""

    id: str
    author: str = ""
    name: str = ""
    sdk: str = ""
    license: str = ""
    likes: int = 0
    private: bool = False
    gated: bool = False
    tags: List[str] = field(default_factory=list)
    url: str = ""
    endpoint_url: str = ""

    @classmethod
    def from_api(cls, payload: Mapping[str, Any]) -> "SpaceCandidate":
        space_id = str(payload.get("id") or payload.get("repo_id") or "")
        author, _, name = space_id.partition("/")
        card_data = payload.get("cardData") if isinstance(payload.get("cardData"), dict) else {}
        sdk = str(card_data.get("sdk") or payload.get("sdk") or "")
        license_name = str(card_data.get("license") or payload.get("license") or "")
        tags = [str(tag) for tag in payload.get("tags", []) if tag]

        return cls(
            id=space_id,
            author=author,
            name=name,
            sdk=sdk,
            license=license_name,
            likes=int(payload.get("likes") or 0),
            private=bool(payload.get("private")),
            gated=bool(payload.get("gated")),
            tags=tags,
            url=f"{HUGGING_FACE_BASE}/spaces/{space_id}" if space_id else "",
            endpoint_url=HarpEndpointClient.infer_endpoint_url(space_id) if space_id else "",
        )

    def looks_open_source(self) -> bool:
        return not self.private and not self.gated

    def looks_gradio(self) -> bool:
        return self.sdk.lower() == "gradio" or "gradio" in {tag.lower() for tag in self.tags}


@dataclass
class ModelPackage:
    """A normalized, reviewable package for one HARP-compatible model."""

    model_path: str
    source_url: str
    endpoint_url: str
    documentation_url: str
    scraped_at: str
    card: JSON
    inputs: List[JSON]
    outputs: List[JSON]
    space: Optional[SpaceCandidate] = None
    raw_controls: JSON = field(default_factory=dict)

    def to_json(self) -> JSON:
        data = asdict(self)
        if self.space is None:
            data["space"] = None
        return data


@dataclass
class ModelCandidate:
    """A raw Hugging Face model repo that may be wrapped for HARP."""

    id: str
    author: str = ""
    pipeline_tag: str = ""
    library_name: str = ""
    license: str = ""
    downloads: int = 0
    likes: int = 0
    private: bool = False
    gated: bool = False
    tags: List[str] = field(default_factory=list)
    files: List[str] = field(default_factory=list)
    url: str = ""

    @classmethod
    def from_api(cls, payload: Mapping[str, Any]) -> "ModelCandidate":
        repo_id = str(payload.get("id") or payload.get("modelId") or payload.get("repo_id") or "")
        author, _, _name = repo_id.partition("/")
        card_data = payload.get("cardData") if isinstance(payload.get("cardData"), dict) else {}
        siblings = payload.get("siblings") if isinstance(payload.get("siblings"), list) else []
        files = [
            str(item.get("rfilename"))
            for item in siblings
            if isinstance(item, Mapping) and item.get("rfilename")
        ]

        return cls(
            id=repo_id,
            author=str(payload.get("author") or author),
            pipeline_tag=str(payload.get("pipeline_tag") or card_data.get("pipeline_tag") or ""),
            library_name=str(payload.get("library_name") or card_data.get("library_name") or ""),
            license=str(card_data.get("license") or payload.get("license") or ""),
            downloads=int(payload.get("downloads") or 0),
            likes=int(payload.get("likes") or 0),
            private=bool(payload.get("private")),
            gated=bool(payload.get("gated")),
            tags=[str(tag) for tag in payload.get("tags", []) if tag],
            files=files,
            url=f"{HUGGING_FACE_BASE}/{repo_id}" if repo_id else "",
        )

    def to_card(self, readme: str = "") -> JSON:
        return {
            "meta": {
                "id": self.id,
                "author": self.author,
                "pipeline_tag": self.pipeline_tag,
                "library_name": self.library_name,
                "license": self.license,
                "tags": self.tags,
                "downloads": self.downloads,
                "likes": self.likes,
                "private": self.private,
                "gated": self.gated,
                "url": self.url,
            },
            "files": self.files,
            "readme": readme,
        }


@dataclass
class GeneratedAppPackage:
    """Files for a generated pyharp wrapper around a raw Hugging Face model."""

    repo_id: str
    task: str
    framework: str
    score: JSON
    io: JSON
    app_py: str
    requirements: str
    readme: str
    packages_txt: str
    manifest: JSON


@dataclass
class SmokeTestResult:
    """Outcome of launching a generated package and probing its endpoint."""

    ok: bool
    endpoint_url: str = ""
    startup_seconds: Optional[float] = None
    controls_ok: bool = False
    error: str = ""

    def to_json(self) -> JSON:
        return asdict(self)


def classify_task(card: Mapping[str, Any]) -> str:
    """Classify a Hugging Face model card into a HARP-relevant task bucket."""

    meta = _model_meta(card)
    pipeline_tag = str(meta.get("pipeline_tag") or "")
    if pipeline_tag in HARP_FRIENDLY_TASKS:
        return pipeline_tag

    tags = {str(tag).lower() for tag in meta.get("tags", []) if tag}
    for candidate in HARP_FRIENDLY_TASKS:
        if candidate in tags:
            return candidate

    blob = " ".join(sorted(tags)) + " " + str(card.get("readme") or "").lower()
    if "source separation" in blob or "stem" in blob or "audio enhancement" in blob:
        return "audio-to-audio"
    if "speech recognition" in blob or "transcribe" in blob:
        return "automatic-speech-recognition"
    if "music generation" in blob or "text-to-music" in blob:
        return "text-to-audio"
    if "text to speech" in blob or "tts" in tags:
        return "text-to-speech"
    if "midi" in blob:
        return "midi"
    return "unknown"


def evaluate_license(license_name: str) -> JSON:
    """Classify a license string for automatic packaging decisions."""

    normalized = license_name.strip().lower()
    if not normalized:
        return {
            "status": "missing",
            "license": "",
            "is_blocking": False,
            "reason": "license missing; manual review required",
        }
    if normalized in NONCOMMERCIAL_LICENSES:
        return {
            "status": "noncommercial",
            "license": normalized,
            "is_blocking": True,
            "reason": "non-commercial license blocks automatic packaging",
        }
    if normalized in PERMISSIVE_LICENSES:
        return {
            "status": "permissive",
            "license": normalized,
            "is_blocking": False,
            "reason": "permissive license",
        }
    return {
        "status": "unknown",
        "license": normalized,
        "is_blocking": False,
        "reason": "license is not recognized; manual review required",
    }


def extract_io_signature(card: Mapping[str, Any]) -> JSON:
    """Infer simple audio I/O hints from model metadata and README text."""

    meta = _model_meta(card)
    text = str(card.get("readme") or "")
    signature: JSON = {
        "sample_rate_hz": None,
        "channels": None,
        "fixed_length_s": None,
        "needs_text_input": "text-to" in str(meta.get("pipeline_tag") or ""),
        "source": "regex",
    }

    sr_match = _SR_RE.search(text)
    if sr_match:
        signature["sample_rate_hz"] = int(sr_match.group(1)) * 1000

    ch_match = _CHANNELS_RE.search(text)
    if ch_match:
        signature["channels"] = ch_match.group(1).lower()

    return signature


def score_compatibility(card: Mapping[str, Any]) -> JSON:
    """Score whether a raw Hugging Face model is a good HARP wrapper candidate."""

    meta = _model_meta(card)
    files = [str(file_name) for file_name in card.get("files", [])]
    readme = str(card.get("readme") or "")
    task = classify_task(card)
    license_result = evaluate_license(str(meta.get("license") or ""))

    blockers: List[str] = []
    notes: List[str] = []
    score = 0.5

    if task == "unknown":
        blockers.append("task_not_audio")
    elif task == "midi":
        score += 0.05
        notes.append("MIDI-related; useful for HARP but needs a MIDI template")
    elif task == "audio-to-audio":
        score += 0.25
        notes.append("audio-to-audio is the cleanest pyharp template fit")
    else:
        score += 0.05
        notes.append(f"{task} is relevant but needs a specialized template")

    if license_result["is_blocking"]:
        blockers.append(f"license_{license_result['status']}:{license_result['license']}")
    elif license_result["status"] == "permissive":
        score += 0.15
        notes.append(f"permissive license: {license_result['license']}")
    else:
        notes.append(license_result["reason"])

    has_config = any(file_name.endswith("config.json") for file_name in files)
    has_weights = any(file_name.endswith(WEIGHT_FILE_SUFFIXES) for file_name in files)
    if has_config and has_weights:
        score += 0.05
        notes.append("standard model layout with config and weights")
    elif files and not has_weights:
        blockers.append("no_weight_files")
    elif not files:
        notes.append("file list missing; cannot verify weights")

    if len(readme) > 500:
        score += 0.05
        notes.append("substantial README/model card")
    else:
        notes.append("README/model card is sparse")

    return {
        "score": round(max(0.0, min(1.0, score)), 3),
        "blockers": blockers,
        "rationale": "; ".join(notes),
        "task": task,
        "license": license_result,
    }


SUPPORTED_GENERATION_FRAMEWORKS = ("speechbrain",)


def detect_inference_framework(card: Mapping[str, Any]) -> str:
    """Detect the Python framework needed to run a raw Hugging Face audio model.

    Different ``audio-to-audio`` models load through completely different APIs,
    so there is no single universal inference call. We only claim support for
    frameworks that expose a clean, documented ``from_pretrained``/``from_hparams``
    inference path; everything else is reported as ``unknown`` so the caller can
    refuse to emit a wrapper it cannot actually run.
    """

    meta = _model_meta(card)
    library = str(meta.get("library_name") or "").strip().lower()
    tags = {str(tag).strip().lower() for tag in meta.get("tags", []) if tag}
    blob = " ".join(sorted(tags)) + " " + str(card.get("readme") or "").lower()

    if library == "speechbrain" or "speechbrain" in tags or "speechbrain" in blob:
        return "speechbrain"
    if library == "asteroid" or "asteroid" in tags:
        return "asteroid"
    if library == "transformers" or "transformers" in tags:
        return "transformers"
    return "unknown"


def _speechbrain_kind(card: Mapping[str, Any]) -> str:
    """Pick the SpeechBrain inference interface that matches the model."""

    meta = _model_meta(card)
    blob = " ".join(str(tag).lower() for tag in meta.get("tags", []) if tag)
    blob += " " + str(meta.get("id") or "").lower()
    blob += " " + str(card.get("readme") or "").lower()

    enhancement_markers = ("enhance", "denois", "metricgan", "mtl-mimic", "dereverb")
    if any(marker in blob for marker in enhancement_markers):
        return "enhancement"
    return "separation"


def render_pyharp_app(card: Mapping[str, Any], signature: Optional[Mapping[str, Any]] = None) -> str:
    """Render a starter pyharp ``app.py`` for supported raw Hugging Face models.

    Only frameworks in :data:`SUPPORTED_GENERATION_FRAMEWORKS` produce a wrapper.
    Emitting code we cannot run (the previous behavior, which called the
    nonexistent ``pipeline("audio-to-audio")`` task) is worse than refusing, so
    unsupported models raise :class:`NotImplementedError`.
    """

    task = classify_task(card)
    if task != "audio-to-audio":
        raise NotImplementedError(
            f"No app.py template is available for task '{task}' yet."
        )

    framework = detect_inference_framework(card)
    if framework == "speechbrain":
        return _render_speechbrain_app(card, signature)

    raise NotImplementedError(
        f"No runnable app.py template for framework '{framework}'. "
        f"Supported frameworks: {', '.join(SUPPORTED_GENERATION_FRAMEWORKS)}. "
        "Wire up a model-specific template, or package the model's existing "
        "Gradio Space with the `package` command instead."
    )


def _render_speechbrain_app(
    card: Mapping[str, Any],
    signature: Optional[Mapping[str, Any]] = None,
) -> str:
    """Render a SpeechBrain-based pyharp wrapper using real inference APIs."""

    meta = _model_meta(card)
    sig = dict(signature or extract_io_signature(card))
    repo_id = str(meta.get("id") or "unknown/unknown")
    model_name = repo_id.split("/")[-1].replace("_", " ").replace("-", " ").title()
    author = str(meta.get("author") or repo_id.split("/", 1)[0] or "unknown")
    description = _short_description(card)
    tags = meta.get("tags") if isinstance(meta.get("tags"), list) else []
    kind = _speechbrain_kind(card)
    # SpeechBrain separation checkpoints (e.g. SepFormer/WSJ0-2mix) are usually
    # 8 kHz; enhancement checkpoints are usually 16 kHz. The README sample rate,
    # when present, wins over these defaults.
    default_sr = 8000 if kind == "separation" else 16000
    target_sr = sig.get("sample_rate_hz") or default_sr

    header = f'''from __future__ import annotations

import tempfile

import gradio as gr
import torch
import torchaudio

from pyharp import ModelCard, build_endpoint


REPO_ID = {json.dumps(repo_id)}
TARGET_SAMPLE_RATE = {json.dumps(target_sr)}
DEVICE = "cuda" if torch.cuda.is_available() else "cpu"

model_card = ModelCard(
    name={json.dumps(model_name)},
    description={json.dumps(description)},
    author={json.dumps(author)},
    tags={json.dumps(tags)},
)
'''

    if kind == "enhancement":
        body = '''

from speechbrain.inference.enhancement import SpectralMaskEnhancement

enhancer = SpectralMaskEnhancement.from_hparams(
    source=REPO_ID,
    savedir=tempfile.mkdtemp(prefix="harp_speechbrain_"),
    run_opts={"device": DEVICE},
)


def process_fn(input_audio_path: str) -> str:
    enhanced = enhancer.enhance_file(input_audio_path)
    if enhanced.dim() == 1:
        enhanced = enhanced.unsqueeze(0)
    else:
        enhanced = enhanced[:1]

    output = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    output.close()
    torchaudio.save(output.name, enhanced.detach().cpu(), TARGET_SAMPLE_RATE)
    return output.name


OUTPUT_LABEL = "Enhanced Audio"
OUTPUT_INFO = "Speech enhanced by " + REPO_ID
'''
    else:
        body = '''

from speechbrain.inference.separation import SepformerSeparation

separator = SepformerSeparation.from_hparams(
    source=REPO_ID,
    savedir=tempfile.mkdtemp(prefix="harp_speechbrain_"),
    run_opts={"device": DEVICE},
)


def process_fn(input_audio_path: str) -> str:
    # est_sources has shape [batch, time, n_sources]; return the first source.
    est_sources = separator.separate_file(path=input_audio_path)
    first_source = est_sources[0, :, 0].detach().cpu().unsqueeze(0)

    output = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
    output.close()
    torchaudio.save(output.name, first_source, TARGET_SAMPLE_RATE)
    return output.name


OUTPUT_LABEL = "Separated Source"
OUTPUT_INFO = "First separated source from " + REPO_ID
'''

    footer = '''

with gr.Blocks() as demo:
    input_components = [
        gr.Audio(type="filepath", label="Input Audio").harp_required(True),
    ]
    output_components = [
        gr.Audio(type="filepath", label=OUTPUT_LABEL).set_info(OUTPUT_INFO),
    ]
    build_endpoint(
        model_card=model_card,
        input_components=input_components,
        output_components=output_components,
        process_fn=process_fn,
    )

demo.queue().launch(share=True, show_error=False, pwa=True)
'''

    return header + body + footer


def _requirements_for_framework(framework: str) -> str:
    base = [
        "git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0",
        "gradio>=4.0",
    ]
    if framework == "speechbrain":
        base += ["speechbrain>=1.0.0", "torch", "torchaudio", "soundfile"]
    return "\n".join(base + [""])


def build_generated_app_package(card: Mapping[str, Any]) -> GeneratedAppPackage:
    """Build in-memory files for a generated pyharp wrapper.

    Raises :class:`NotImplementedError` (via :func:`render_pyharp_app`) when the
    model's framework has no runnable template, so we never write a wrapper that
    is known to fail at startup.
    """

    task = classify_task(card)
    framework = detect_inference_framework(card)
    score = score_compatibility(card)
    signature = extract_io_signature(card)
    app_py = render_pyharp_app(card, signature)
    repo_id = str(_model_meta(card).get("id") or "unknown/unknown")
    requirements = _requirements_for_framework(framework)
    readme = _render_generated_space_readme(card, task)
    packages_txt = "\n".join(["ffmpeg", "libsndfile1", ""]) if task == "audio-to-audio" else ""
    manifest = {
        "repo_id": repo_id,
        "task": task,
        "framework": framework,
        "score": score,
        "io": signature,
        "entry": "app.py",
        "space_layout": "huggingface-gradio",
        "generated": True,
    }
    return GeneratedAppPackage(
        repo_id=repo_id,
        task=task,
        framework=framework,
        score=score,
        io=signature,
        app_py=app_py,
        requirements=requirements,
        readme=readme,
        packages_txt=packages_txt,
        manifest=manifest,
    )


class HuggingFaceSpaceScraper:
    """Small Hugging Face Hub API client for Space discovery."""

    def __init__(self, timeout: float = 30.0, token: Optional[str] = None):
        self.timeout = timeout
        self.token = token

    def discover(
        self,
        query: str = "",
        *,
        author: str = "",
        tags: Iterable[str] = (),
        limit: int = 25,
        full: bool = True,
    ) -> List[SpaceCandidate]:
        params: Dict[str, Any] = {"limit": max(1, limit), "full": str(full).lower()}
        if query:
            params["search"] = query
        if author:
            params["author"] = author
        for tag in tags:
            # Hugging Face accepts repeated filter parameters.
            params.setdefault("filter", [])
            params["filter"].append(tag)

        url = f"{HUGGING_FACE_BASE}/api/spaces?{urlencode(params, doseq=True)}"
        payload = self._get_json(url)
        if not isinstance(payload, list):
            raise ValueError(f"Expected a list from Hugging Face Spaces API, got {type(payload)}")

        candidates = [SpaceCandidate.from_api(item) for item in payload if isinstance(item, dict)]
        return [candidate for candidate in candidates if candidate.id]

    def get_space(self, space_id: str) -> Optional[SpaceCandidate]:
        try:
            payload = self._get_json(f"{HUGGING_FACE_BASE}/api/spaces/{quote(space_id, safe='/')}")
        except HTTPError as exc:
            if exc.code == 404:
                return None
            raise
        if not isinstance(payload, dict):
            return None
        return SpaceCandidate.from_api(payload)

    def get_model_card(self, repo_id: str) -> Optional[JSON]:
        normalized_repo_id = _normalize_hf_model_repo_id(repo_id)
        try:
            payload = self._get_json(f"{HUGGING_FACE_BASE}/api/models/{quote(normalized_repo_id, safe='/')}")
        except HTTPError as exc:
            if exc.code == 404:
                return None
            raise

        if not isinstance(payload, dict):
            return None

        candidate = ModelCandidate.from_api(payload)
        return candidate.to_card(self.get_model_readme(normalized_repo_id))

    def get_model_readme(self, repo_id: str) -> str:
        url = f"{HUGGING_FACE_BASE}/{quote(repo_id, safe='/')}/raw/main/README.md"
        request = Request(url, headers=self._headers())
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return response.read().decode("utf-8", errors="replace")
        except HTTPError as exc:
            if exc.code == 404:
                return ""
            raise

    def get_space_file(
        self,
        space_id: str,
        filename: str = "app.py",
        revision: str = "main",
    ) -> Optional[str]:
        """Download a raw file from a Hugging Face Space, or None if absent."""

        url = (
            f"{HUGGING_FACE_BASE}/spaces/{quote(space_id, safe='/')}"
            f"/raw/{quote(revision)}/{quote(filename)}"
        )
        request = Request(url, headers=self._headers())
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return response.read().decode("utf-8", errors="replace")
        except HTTPError as exc:
            if exc.code == 404:
                return None
            raise

    def _headers(self) -> Dict[str, str]:
        headers = {"User-Agent": "model-agent/0.1"}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        return headers

    def _get_json(self, url: str) -> Any:
        request = Request(url, headers=self._headers())
        with urlopen(request, timeout=self.timeout) as response:
            return json.loads(response.read().decode("utf-8"))


class GitHubRepoScraper:
    """Minimal read-only GitHub client used to ground LLM recipe generation.

    Mirrors :class:`HuggingFaceSpaceScraper`, but for source that lives on
    GitHub rather than a Hugging Face Space. It uses the public REST API plus
    ``raw.githubusercontent.com``, so no ``git`` binary is required. An optional
    token (``GITHUB_TOKEN`` / ``GH_TOKEN``) raises the unauthenticated rate
    limit (60 req/h) and allows private repos.
    """

    def __init__(self, timeout: float = 30.0, token: Optional[str] = None):
        self.timeout = timeout
        self.token = (
            token
            or os.environ.get("GITHUB_TOKEN")
            or os.environ.get("GH_TOKEN")
            or None
        )

    def _headers(self, *, api: bool) -> Dict[str, str]:
        headers = {"User-Agent": "model-agent/0.1"}
        if api:
            headers["Accept"] = "application/vnd.github+json"
            headers["X-GitHub-Api-Version"] = "2022-11-28"
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        return headers

    def _get_json(self, url: str) -> Any:
        request = Request(url, headers=self._headers(api=True))
        with urlopen(request, timeout=self.timeout) as response:
            return json.loads(response.read().decode("utf-8"))

    def get_repo_info(self, owner: str, repo: str) -> Optional[JSON]:
        """Repo metadata (default_branch, topics, license, ...), or None if 404."""

        try:
            payload = self._get_json(f"{GITHUB_API_BASE}/repos/{quote(owner)}/{quote(repo)}")
        except HTTPError as exc:
            if exc.code == 404:
                return None
            raise
        return payload if isinstance(payload, dict) else None

    def list_tree(self, owner: str, repo: str, ref: str) -> List[str]:
        """Return every blob path in the repo tree at ``ref`` (recursive)."""

        url = (
            f"{GITHUB_API_BASE}/repos/{quote(owner)}/{quote(repo)}"
            f"/git/trees/{quote(ref)}?recursive=1"
        )
        try:
            payload = self._get_json(url)
        except HTTPError as exc:
            if exc.code == 404:
                return []
            raise
        tree = payload.get("tree") if isinstance(payload, dict) else None
        if not isinstance(tree, list):
            return []
        return [
            str(item.get("path"))
            for item in tree
            if isinstance(item, dict) and item.get("type") == "blob" and item.get("path")
        ]

    def get_file(self, owner: str, repo: str, ref: str, path: str) -> Optional[str]:
        """Download one raw text file from the repo at ``ref``, or None if absent."""

        url = (
            f"{GITHUB_RAW_BASE}/{quote(owner)}/{quote(repo)}"
            f"/{quote(ref)}/{quote(path)}"
        )
        request = Request(url, headers=self._headers(api=False))
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return response.read().decode("utf-8", errors="replace")
        except HTTPError as exc:
            if exc.code == 404:
                return None
            raise


class HarpEndpointClient:
    """Client for the HARP controls endpoint exposed by pyharp apps."""

    def __init__(self, timeout: float = 120.0):
        self.timeout = timeout

    @staticmethod
    def infer_host_slash_model(model_path: str) -> str:
        path = model_path.strip().rstrip("/")
        if path.startswith("https://huggingface.co/spaces/"):
            return path.removeprefix("https://huggingface.co/spaces/")
        if path.startswith("https://") and path.endswith(".hf.space"):
            host = path.removeprefix("https://").removesuffix(".hf.space")
            # Short Space URLs flatten "org/model" into "org-model". HARP has
            # existing Spaces under a hyphenated org, so handle those before
            # the generic split.
            for org in KNOWN_HYPHENATED_SPACE_ORGS:
                prefix = f"{org}-"
                if host.startswith(prefix):
                    return f"{org}/{host.removeprefix(prefix)}"
            parts = host.split("-", 1)
            if len(parts) == 2:
                return f"{parts[0]}/{parts[1]}"
        return path

    @staticmethod
    def infer_endpoint_url(model_path: str) -> str:
        path = model_path.strip().rstrip("/")
        if path.startswith("http://localhost") or re.match(r"^http://\d+\.\d+\.\d+\.\d+:\d+", path):
            return path
        if path.startswith("https://") and path.endswith(".gradio.live"):
            return path
        if path.startswith("https://") and path.endswith(".hf.space"):
            return f"{path}/"
        host_slash_model = HarpEndpointClient.infer_host_slash_model(path)
        if "/" in host_slash_model:
            host, model = host_slash_model.split("/", 1)
            return f"https://{host}-{model.replace('_', '-')}.hf.space/"
        return path

    @staticmethod
    def infer_documentation_url(model_path: str) -> str:
        path = model_path.strip().rstrip("/")
        if path.startswith("https://huggingface.co/spaces/"):
            return path
        host_slash_model = HarpEndpointClient.infer_host_slash_model(path)
        if "/" in host_slash_model:
            return f"{HUGGING_FACE_BASE}/spaces/{host_slash_model}"
        return HarpEndpointClient.infer_endpoint_url(model_path)

    def fetch_space_id(self, model_path: str) -> str:
        """Query the live Gradio config for the authoritative ``author/name``.

        Short ``*.hf.space`` URLs flatten ``author/model`` to ``author-model`` and
        turn ``_`` into ``-``, so the canonical id cannot be recovered by string
        manipulation alone (the same ambiguity HARP's C++ client resolves). The
        Gradio config endpoint reports the real ``space_id``; we try both the
        modern and legacy config paths.
        """

        endpoint = self.infer_endpoint_url(model_path).rstrip("/")
        last_error: Optional[EndpointProbeError] = None
        for config_path in ("gradio_api/config", "config"):
            try:
                text = self._get_text(f"{endpoint}/{config_path}")
            except EndpointProbeError as exc:
                last_error = exc
                continue

            try:
                config = json.loads(text)
            except json.JSONDecodeError:
                continue

            if isinstance(config, dict):
                space_id = config.get("space_id")
                if isinstance(space_id, str) and "/" in space_id:
                    return space_id

        if last_error is not None:
            raise last_error
        return ""

    def resolve_canonical_path(self, model_path: str) -> str:
        """Return the canonical ``author/name`` for a model path.

        Full ``huggingface.co/spaces/...`` URLs already carry the exact id and are
        trusted as-is. For abbreviated paths and short ``*.hf.space`` URLs we ask
        the endpoint for its real ``space_id`` so documentation/source links keep
        the correct ``_`` vs ``-`` spelling, falling back to the best-effort
        string inference when the config is unavailable.
        """

        guess = self.infer_host_slash_model(model_path)
        if model_path.strip().rstrip("/").startswith("https://huggingface.co/spaces/"):
            return guess

        try:
            space_id = self.fetch_space_id(model_path)
        except EndpointProbeError:
            space_id = ""
        return space_id or guess

    def fetch_controls(self, model_path: str) -> JSON:
        endpoint = self.infer_endpoint_url(model_path).rstrip("/")
        call_url = f"{endpoint}/gradio_api/call/controls"
        event = self._post_json(call_url, {"data": []})
        event_id = event.get("event_id") if isinstance(event, dict) else None
        if not event_id:
            raise EndpointProbeError(f"{model_path} did not return a Gradio event_id")

        response_text = self._get_text(f"{call_url}/{quote(str(event_id))}")
        data = self._parse_gradio_response(response_text)
        if not data or not isinstance(data[0], dict):
            raise EndpointProbeError(f"{model_path} did not return a HARP controls object")

        controls = data[0]
        for key in ("card", "inputs", "outputs"):
            if key not in controls:
                raise EndpointProbeError(f"{model_path} controls are missing '{key}'")
        return controls

    def _post_json(self, url: str, payload: JSON) -> Any:
        body = json.dumps(payload).encode("utf-8")
        request = Request(
            url,
            data=body,
            headers={
                "Accept": "*/*",
                "Content-Type": "application/json",
                "User-Agent": "model-agent/0.1",
            },
            method="POST",
        )
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except (HTTPError, URLError, TimeoutError, socket.timeout) as exc:
            raise EndpointProbeError(f"POST {url} failed: {exc}") from exc

    def _get_text(self, url: str) -> str:
        request = Request(url, headers={"Accept": "*/*", "User-Agent": "model-agent/0.1"})
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return response.read().decode("utf-8")
        except (HTTPError, URLError, TimeoutError, socket.timeout) as exc:
            raise EndpointProbeError(f"GET {url} failed: {exc}") from exc

    @staticmethod
    def _parse_gradio_response(response_text: str) -> List[Any]:
        response_text = response_text.strip()
        if not response_text:
            return []

        try:
            parsed = json.loads(response_text)
            return parsed if isinstance(parsed, list) else [parsed]
        except json.JSONDecodeError:
            pass

        data_lines = []
        for line in response_text.splitlines():
            line = line.strip()
            if line.startswith("data:"):
                value = line.removeprefix("data:").strip()
                if value and value != "[DONE]":
                    data_lines.append(value)

        for line in reversed(data_lines):
            try:
                parsed = json.loads(line)
            except json.JSONDecodeError:
                continue
            return parsed if isinstance(parsed, list) else [parsed]

        raise EndpointProbeError("Could not parse Gradio response payload")


class HarpModelAgent:
    """Coordinates scraping, probing, and writing package artifacts."""

    def __init__(
        self,
        scraper: Optional[HuggingFaceSpaceScraper] = None,
        endpoint_client: Optional[HarpEndpointClient] = None,
        github_scraper: Optional[GitHubRepoScraper] = None,
    ):
        self.scraper = scraper or HuggingFaceSpaceScraper()
        self.endpoint_client = endpoint_client or HarpEndpointClient()
        self.github_scraper = github_scraper or GitHubRepoScraper()

    def discover_open_gradio_spaces(self, **kwargs: Any) -> List[SpaceCandidate]:
        return [
            candidate
            for candidate in self.scraper.discover(**kwargs)
            if candidate.looks_open_source() and candidate.looks_gradio()
        ]

    def build_generated_app_package_for_repo(self, repo_id: str) -> GeneratedAppPackage:
        normalized_repo_id = _normalize_hf_model_repo_id(repo_id)
        card = self.scraper.get_model_card(normalized_repo_id)
        if card is None:
            raise ValueError(f"Hugging Face model repo not found: {normalized_repo_id}")

        score = score_compatibility(card)
        if score["blockers"]:
            raise ValueError(f"Model cannot be packaged automatically: {', '.join(score['blockers'])}")

        return build_generated_app_package(card)

    def package_model(self, model_path: str, *, include_space_metadata: bool = True) -> ModelPackage:
        controls = self.endpoint_client.fetch_controls(model_path)
        # Resolve the canonical author/name so documentation and source links use
        # the real "_" vs "-" spelling rather than a lossy string guess.
        canonical_path = self.endpoint_client.resolve_canonical_path(model_path)
        has_canonical = "/" in canonical_path
        space = self.scraper.get_space(canonical_path) if include_space_metadata and has_canonical else None
        card = controls.get("card") if isinstance(controls.get("card"), dict) else {}
        inputs = controls.get("inputs") if isinstance(controls.get("inputs"), list) else []
        outputs = controls.get("outputs") if isinstance(controls.get("outputs"), list) else []
        documentation_url = (
            f"{HUGGING_FACE_BASE}/spaces/{canonical_path}"
            if has_canonical
            else self.endpoint_client.infer_documentation_url(model_path)
        )

        return ModelPackage(
            model_path=canonical_path,
            source_url=f"{HUGGING_FACE_BASE}/spaces/{canonical_path}" if has_canonical else "",
            endpoint_url=self.endpoint_client.infer_endpoint_url(model_path),
            documentation_url=documentation_url,
            scraped_at=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            card=card,
            inputs=inputs,
            outputs=outputs,
            space=space,
            raw_controls=controls,
        )

    def write_package(self, package: ModelPackage, output_dir: Path) -> Path:
        folder = output_dir / _slug(package.model_path)
        folder.mkdir(parents=True, exist_ok=True)

        _write_json(folder / "manifest.json", package.to_json())
        _write_json(folder / "controls.json", package.raw_controls)
        (folder / "README.md").write_text(_render_readme(package), encoding="utf-8")
        return folder

    def write_generated_app_package(
        self,
        package: GeneratedAppPackage,
        output_dir: Path,
    ) -> Path:
        folder = output_dir / _slug(package.repo_id)
        folder.mkdir(parents=True, exist_ok=True)

        (folder / "app.py").write_text(package.app_py, encoding="utf-8")
        (folder / "requirements.txt").write_text(package.requirements, encoding="utf-8")
        (folder / "README.md").write_text(package.readme, encoding="utf-8")
        (folder / "packages.txt").write_text(package.packages_txt, encoding="utf-8")
        metadata_dir = folder / ".harp"
        metadata_dir.mkdir(parents=True, exist_ok=True)
        _write_json(metadata_dir / "manifest.json", package.manifest)
        return folder

    def smoke_test_package(
        self,
        package_dir: Path,
        *,
        python_executable: Optional[str] = None,
        startup_timeout_s: float = 180.0,
    ) -> SmokeTestResult:
        """Launch a generated ``app.py`` and verify it exposes HARP controls.

        WARNING: this executes the generated wrapper, which downloads and runs
        third-party model code. Only call it after manual review or inside a
        sandbox/venv. It is opt-in and never invoked by discovery or packaging.

        The launched process is always terminated before returning.
        """

        folder = Path(package_dir)
        app_py = folder / "app.py"
        if not app_py.exists():
            return SmokeTestResult(ok=False, error=f"app.py not found in {folder}")

        executable = python_executable or sys.executable
        process = subprocess.Popen(
            [executable, "app.py"],
            cwd=str(folder),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )

        output_lines: "queue.Queue[str]" = queue.Queue()

        def _pump() -> None:
            assert process.stdout is not None
            for line in process.stdout:
                output_lines.put(line)
            process.stdout.close()

        reader = threading.Thread(target=_pump, daemon=True)
        reader.start()

        start = time.monotonic()
        captured: List[str] = []
        endpoint_url = ""
        try:
            while time.monotonic() - start < startup_timeout_s:
                if process.poll() is not None and output_lines.empty():
                    detail = _tail("".join(captured))
                    return SmokeTestResult(
                        ok=False,
                        error=f"app.py exited early (code {process.returncode}): {detail}",
                    )
                try:
                    line = output_lines.get(timeout=1.0)
                except queue.Empty:
                    continue

                captured.append(line)
                match = _LOCAL_URL_RE.search(line) or _PUBLIC_URL_RE.search(line)
                if match:
                    endpoint_url = match.group(0)
                    break

            if not endpoint_url:
                return SmokeTestResult(
                    ok=False,
                    error="timed out waiting for the Gradio URL",
                )

            startup_seconds = round(time.monotonic() - start, 2)
            try:
                self.endpoint_client.fetch_controls(endpoint_url)
            except EndpointProbeError as exc:
                return SmokeTestResult(
                    ok=False,
                    endpoint_url=endpoint_url,
                    startup_seconds=startup_seconds,
                    controls_ok=False,
                    error=str(exc),
                )

            return SmokeTestResult(
                ok=True,
                endpoint_url=endpoint_url,
                startup_seconds=startup_seconds,
                controls_ok=True,
            )
        finally:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()

    def ensure_package_venv(
        self,
        package_dir: Path,
        *,
        log: Optional[Callable[[str], None]] = None,
    ) -> str:
        """Build (or reuse) an isolated venv with the package's requirements.

        Returns the path to the venv's Python interpreter. The venv lives in
        ``<package_dir>/.venv`` and is keyed by a hash of ``requirements.txt``
        (plus the host Python version) via a sentinel file, so repeated
        smoke-tests reuse it instead of reinstalling heavy dependencies such as
        torch. When the requirements change, the venv is rebuilt.

        This NEVER installs into the active interpreter -- everything lands in
        the throwaway venv. The dependency list is third-party/LLM-authored, so
        callers should still treat the resulting smoke-test as running
        untrusted code (review or sandbox first).
        """

        folder = Path(package_dir)
        req_file = folder / "requirements.txt"
        req_text = req_file.read_text(encoding="utf-8") if req_file.exists() else ""

        digest = hashlib.sha256(
            (sys.version + "\n" + req_text).encode("utf-8")
        ).hexdigest()[:16]

        venv_dir = folder / ".venv"
        if os.name == "nt":
            python_path = venv_dir / "Scripts" / "python.exe"
        else:
            python_path = venv_dir / "bin" / "python"
        sentinel = venv_dir / ".harp-requirements.sha256"

        def emit(message: str) -> None:
            if log is not None:
                log(message)

        if (
            python_path.exists()
            and sentinel.exists()
            and sentinel.read_text(encoding="utf-8").strip() == digest
        ):
            emit(f"reusing cached venv at {venv_dir}")
            return str(python_path)

        if venv_dir.exists():
            emit(f"requirements changed; rebuilding venv at {venv_dir}")
            shutil.rmtree(venv_dir, ignore_errors=True)

        emit(f"creating venv at {venv_dir}")
        self._run_pip([sys.executable, "-m", "venv", str(venv_dir)], log=emit)

        # Upgrading pip is best-effort; an old pip should still install most wheels.
        self._run_pip(
            [str(python_path), "-m", "pip", "install", "--upgrade", "pip"],
            log=emit,
            check=False,
        )

        if req_text.strip():
            emit("installing requirements.txt (first run may take several minutes)...")
            self._run_pip(
                [str(python_path), "-m", "pip", "install", "-r", str(req_file)],
                log=emit,
            )
        else:
            emit("requirements.txt is empty; venv created with no extra packages")

        sentinel.write_text(digest, encoding="utf-8")
        return str(python_path)

    @staticmethod
    def _run_pip(
        cmd: List[str],
        *,
        log: Optional[Callable[[str], None]] = None,
        check: bool = True,
    ) -> None:
        """Run a pip/venv subprocess, streaming output and raising on failure."""

        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if log is not None and proc.stdout:
            for line in proc.stdout.splitlines():
                log(line)
        if check and proc.returncode != 0:
            raise VenvSetupError(
                f"command failed (exit {proc.returncode}): {' '.join(cmd)}\n"
                f"{_tail(proc.stdout or '')}"
            )

    def deploy_space(
        self,
        package_dir: Path,
        repo_id: str,
        *,
        token: Optional[str] = None,
        private: bool = False,
        space_sdk: str = "gradio",
        commit_message: str = "Deploy HARP wrapper via model agent",
        log: Optional[Callable[[str], None]] = None,
    ) -> JSON:
        """Create (or reuse) a Hugging Face Space and upload a generated package.

        The package folder already carries everything a Space needs (``app.py``,
        ``requirements.txt``, a ``README.md`` with ``sdk: gradio`` front matter,
        and ``packages.txt``), so deploying is: create the Space repo, then
        upload the folder. The Space's own container installs the dependencies,
        which lets users with Hugging Face access verify a wrapper without
        installing anything locally.

        Requires the optional ``huggingface_hub`` package and a write token
        (passed via ``token`` or the ``HF_TOKEN`` / ``HUGGING_FACE_HUB_TOKEN``
        environment variables, or an existing cached login).
        """

        folder = Path(package_dir)
        app_py = folder / "app.py"
        if not app_py.exists():
            raise DeploySpaceError(f"app.py not found in {folder}")

        repo_id = _normalize_space_repo_id(repo_id)
        if repo_id.count("/") != 1 or not all(repo_id.split("/")):
            raise DeploySpaceError(
                f"Space id must look like 'username/space-name' (got '{repo_id}')."
            )

        try:
            from huggingface_hub import HfApi
        except ImportError as exc:  # optional dependency
            raise DeploySpaceError(
                "huggingface_hub is required to deploy to a Space. Install it with "
                "`pip install huggingface_hub` (it is intentionally optional so the "
                "rest of the agent stays dependency-free)."
            ) from exc

        resolved_token = (
            token
            or os.environ.get("HF_TOKEN")
            or os.environ.get("HUGGING_FACE_HUB_TOKEN")
        )

        def emit(message: str) -> None:
            if log is not None:
                log(message)

        api = HfApi(token=resolved_token)
        try:
            emit(f"creating/reusing Space {repo_id} (sdk={space_sdk}, private={private})")
            api.create_repo(
                repo_id=repo_id,
                repo_type="space",
                space_sdk=space_sdk,
                private=private,
                exist_ok=True,
            )
            emit("uploading package files...")
            api.upload_folder(
                repo_id=repo_id,
                repo_type="space",
                folder_path=str(folder),
                commit_message=commit_message,
                ignore_patterns=[
                    ".venv/*",
                    "**/.venv/*",
                    "__pycache__/*",
                    "**/__pycache__/*",
                    "*.pyc",
                ],
            )
        except DeploySpaceError:
            raise
        except Exception as exc:  # network/auth/permission errors from the hub
            raise DeploySpaceError(f"Hugging Face Space deploy failed: {exc}") from exc

        space_url = f"{HUGGING_FACE_BASE}/spaces/{repo_id}"
        emit(f"deployed: {space_url}")
        return {
            "repo_id": repo_id,
            "space_url": space_url,
            "private": private,
            "sdk": space_sdk,
            "authenticated": bool(resolved_token),
        }

    def deploy_into_space(
        self,
        package_dir: Path,
        repo_id: str,
        *,
        token: Optional[str] = None,
        gradio_version: str = HARP_GRADIO_VERSION,
        freeze_from: Optional[Path] = None,
        commit_message: str = "Add HARP pyharp endpoint via model agent",
        log: Optional[Callable[[str], None]] = None,
    ) -> JSON:
        """Overlay a HARP wrapper onto an EXISTING Space, reconciling dependencies.

        Unlike :meth:`deploy_space` (which uploads a self-contained package), this
        targets a Space that already contains the model's code (e.g. a *duplicate*
        of the original Space). It uploads only ``app.py`` plus a reconciled
        ``requirements.txt`` / ``README.md`` -- every other repo file is left
        untouched -- so the model's own modules and weights stay in place.

        The reconciliation fixes the predictable HARP conflict: pyharp pins
        ``gradio==<gradio_version>``, but a Gradio Space's README ``sdk_version``
        (which Hugging Face force-installs) and its ``requirements.txt`` usually
        pin a different gradio. Both are realigned to ``gradio_version`` and the
        pyharp requirement is ensured. Conflicts it cannot safely touch (e.g. two
        unrelated packages pinned incompatibly) are reported, not silently
        "fixed".

        When ``freeze_from`` points at a known-good ``pip freeze`` file, the
        requirements are additionally locked to that exact closure (see
        :func:`merge_frozen_pins`) so unpinned ML deps cannot drift when gradio
        is forced down -- the usual cause of "right melody, gibberish words".

        Requires ``huggingface_hub`` and a write token; the Space must already
        exist.
        """

        folder = Path(package_dir)
        app_py = folder / "app.py"
        if not app_py.exists():
            raise DeploySpaceError(f"app.py not found in {folder}")

        repo_id = _normalize_space_repo_id(repo_id)
        if repo_id.count("/") != 1 or not all(repo_id.split("/")):
            raise DeploySpaceError(
                f"Space id must look like 'username/space-name' (got '{repo_id}')."
            )

        try:
            from huggingface_hub import HfApi, hf_hub_download
        except ImportError as exc:  # optional dependency
            raise DeploySpaceError(
                "huggingface_hub is required to deploy to a Space. Install it with "
                "`pip install huggingface_hub`."
            ) from exc

        resolved_token = (
            token
            or os.environ.get("HF_TOKEN")
            or os.environ.get("HUGGING_FACE_HUB_TOKEN")
        )

        def emit(message: str) -> None:
            if log is not None:
                log(message)

        existing_req = self._download_space_text(
            hf_hub_download, repo_id, "requirements.txt", resolved_token
        )
        if existing_req is None:
            raise DeploySpaceError(
                f"Could not read requirements.txt from Space '{repo_id}'. For "
                "--into-space the Space must already exist (duplicate the model's "
                "original Space first), and your token must have access to it."
            )

        if freeze_from is not None:
            freeze_path = Path(freeze_from)
            if not freeze_path.exists():
                raise DeploySpaceError(
                    f"--freeze-from file not found: {freeze_path}"
                )
            freeze_text = freeze_path.read_text(encoding="utf-8")
            new_req, changes = merge_frozen_pins(
                existing_req, freeze_text, gradio_version=gradio_version
            )
        else:
            new_req, changes = reconcile_requirements(existing_req, gradio_version)
        uploads: List[Tuple[str, bytes]] = [
            ("app.py", app_py.read_bytes()),
            ("requirements.txt", new_req.encode("utf-8")),
        ]

        existing_readme = self._download_space_text(
            hf_hub_download, repo_id, "README.md", resolved_token
        )
        if existing_readme is not None:
            new_readme, readme_changes = reconcile_readme(existing_readme, gradio_version)
            changes.extend(readme_changes)
            uploads.append(("README.md", new_readme.encode("utf-8")))

        unresolved = _duplicate_pin_conflicts(new_req)

        api = HfApi(token=resolved_token)
        try:
            for path_in_repo, data in uploads:
                emit(f"uploading {path_in_repo}...")
                api.upload_file(
                    path_or_fileobj=data,
                    path_in_repo=path_in_repo,
                    repo_id=repo_id,
                    repo_type="space",
                    commit_message=commit_message,
                )
        except DeploySpaceError:
            raise
        except Exception as exc:  # network/auth/permission errors from the hub
            raise DeploySpaceError(f"Hugging Face upload failed: {exc}") from exc

        space_url = f"{HUGGING_FACE_BASE}/spaces/{repo_id}"
        emit(f"deployed into Space: {space_url}")
        if unresolved:
            emit("WARNING: unresolved dependency conflicts remain: " + "; ".join(unresolved))
        return {
            "repo_id": repo_id,
            "space_url": space_url,
            "mode": "into-space",
            "gradio_version": gradio_version,
            "uploaded": [path for path, _ in uploads],
            "changes": changes,
            "unresolved_conflicts": unresolved,
            "authenticated": bool(resolved_token),
        }

    @staticmethod
    def _download_space_text(
        downloader: Callable[..., str],
        repo_id: str,
        filename: str,
        token: Optional[str],
    ) -> Optional[str]:
        """Download one text file from a Space via huggingface_hub, or None."""

        try:
            local = downloader(
                repo_id=repo_id,
                filename=filename,
                repo_type="space",
                token=token,
            )
        except Exception:  # missing file / auth / network -> treat as absent
            return None
        try:
            return Path(local).read_text(encoding="utf-8")
        except OSError:
            return None

    def fetch_space_sources(
        self,
        space_id: str,
        *,
        entry: str = "app.py",
        max_files: int = 6,
        max_chars: int = 6000,
    ) -> Dict[str, str]:
        """Download a Space's entry file and the first-party modules it imports.

        Best-effort grounding for LLM recipe generation: returns
        ``{filename: source}`` for ``app.py`` plus the local modules it (and they)
        import, so the LLM can reuse the model's REAL loading/inference API and UI
        instead of guessing from the model card. Stdlib and well-known third-party
        imports are skipped; the crawl is bounded by ``max_files`` and each file is
        truncated to ``max_chars``. One unreachable file never aborts the rest.
        """

        space_id = _normalize_space_repo_id(space_id)
        sources: Dict[str, str] = {}
        seen: set = set()
        queue_: List[str] = [entry]
        attempts = 0

        while queue_ and len(sources) < max_files and attempts < max_files * 6:
            filename = queue_.pop(0)
            if filename in seen:
                continue
            seen.add(filename)
            attempts += 1
            try:
                text = self.scraper.get_space_file(space_id, filename=filename)
            except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
                continue
            if text is None:
                continue

            sources[filename] = text[:max_chars]
            for module in _local_import_modules(text):
                candidate = module.replace(".", "/") + ".py"
                if candidate not in seen and candidate not in queue_:
                    queue_.append(candidate)

        return sources

    # ---- GitHub grounding (mirror of the Space-source path above) ----

    def resolve_github_target(
        self, repo_url: str, *, ref: Optional[str] = None
    ) -> Tuple[str, str, str]:
        """Return ``(owner, repo, ref)`` for a GitHub URL, resolving the default
        branch via the API when no ref is given in the URL or by the caller."""

        owner, repo, url_ref, _subpath = _parse_github_url(repo_url)
        resolved = (ref or url_ref or "").strip()
        if not resolved:
            try:
                info = self.github_scraper.get_repo_info(owner, repo) or {}
            except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
                info = {}
            resolved = str(info.get("default_branch") or "main")
        return owner, repo, resolved

    def github_pip_requirement(self, repo_url: str, *, ref: Optional[str] = None) -> str:
        """Build the ``git+https://...`` pip requirement for a GitHub repo."""

        owner, repo, resolved = self.resolve_github_target(repo_url, ref=ref)
        return f"git+https://github.com/{owner}/{repo}.git@{resolved}"

    def get_github_card(self, repo_url: str, *, ref: Optional[str] = None) -> JSON:
        """Synthesize a model-card-shaped dict from a GitHub repo.

        Mirrors the Hugging Face card shape (``meta`` / ``readme`` / ``files``)
        so :class:`RecipeGenerationContext.from_card` can consume it unchanged.
        """

        owner, repo, resolved = self.resolve_github_target(repo_url, ref=ref)
        try:
            info = self.github_scraper.get_repo_info(owner, repo) or {}
        except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
            info = {}

        readme = ""
        for candidate in ("README.md", "README.rst", "README.txt", "readme.md", "README"):
            try:
                text = self.github_scraper.get_file(owner, repo, resolved, candidate)
            except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
                text = None
            if text:
                readme = text
                break

        try:
            files = self.github_scraper.list_tree(owner, repo, resolved)
        except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
            files = []

        license_obj = info.get("license") if isinstance(info.get("license"), Mapping) else {}
        license_name = str(license_obj.get("spdx_id") or license_obj.get("key") or "")
        if license_name.upper() in ("NOASSERTION", "NONE"):
            license_name = ""

        return {
            "meta": {
                "id": f"{owner}/{repo}",
                "name": repo,
                "author": owner,
                "tags": [str(topic) for topic in (info.get("topics") or [])],
                "license": license_name,
                "library_name": "",
                "pipeline_tag": "",
            },
            "readme": readme,
            "files": files[:200],
        }

    def fetch_github_sources(
        self,
        repo_url: str,
        *,
        ref: Optional[str] = None,
        entry_hints: Iterable[str] = (),
        max_files: int = 8,
        max_chars: int = 6000,
    ) -> Dict[str, str]:
        """Download a GitHub repo's entry/inference files and their first-party
        imports, returning ``{path: source}`` for LLM grounding.

        The repo tree is listed once; promising python files (app/inference/
        pipeline modules, shallow paths, skipping test/doc trees) seed a bounded
        BFS that follows only the repo's own imports. Stdlib/third-party imports
        are skipped. One unreachable file never aborts the rest.
        """

        owner, repo, resolved = self.resolve_github_target(repo_url, ref=ref)
        _o, _r, _url_ref, subpath = _parse_github_url(repo_url)

        try:
            tree = self.github_scraper.list_tree(owner, repo, resolved)
        except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
            return {}

        py_files = {path for path in tree if path.endswith(".py")}
        seeds = _github_seed_files(py_files, subpath=subpath, entry_hints=entry_hints)

        sources: Dict[str, str] = {}
        seen: set = set()
        queue_: List[str] = list(seeds)
        attempts = 0

        while queue_ and len(sources) < max_files and attempts < max_files * 8:
            path = queue_.pop(0)
            if path in seen:
                continue
            seen.add(path)
            attempts += 1
            if path not in py_files:
                continue
            try:
                text = self.github_scraper.get_file(owner, repo, resolved, path)
            except (HTTPError, URLError, TimeoutError, socket.timeout, OSError):
                continue
            if text is None:
                continue

            sources[path] = text[:max_chars]
            current_dir = path.rsplit("/", 1)[0] if "/" in path else ""
            for module in _local_import_modules(text):
                for candidate in _module_path_candidates(module, (current_dir, subpath)):
                    if candidate in py_files and candidate not in seen and candidate not in queue_:
                        queue_.append(candidate)

        return sources

    def harvest_space_apps(
        self,
        output_dir: Path,
        *,
        author: str = "",
        query: str = "",
        limit: int = 100,
        filename: str = "app.py",
    ) -> List[JSON]:
        """Download `app.py` (or another file) from an author's HF Spaces.

        Read-only review helper: it discovers Spaces and saves each Space's
        wrapper file under ``output_dir/<slug>/<filename>`` so a corpus of real
        HARP wrappers can be studied offline. A per-Space ``index.json`` summary
        is written alongside. One unreachable Space never aborts the batch.
        """

        output_dir = Path(output_dir)
        output_dir.mkdir(parents=True, exist_ok=True)

        candidates = self.scraper.discover(query=query, author=author, limit=limit)
        results: List[JSON] = []

        for candidate in candidates:
            record: JSON = {
                "id": candidate.id,
                "url": candidate.url,
                "status": "",
                "path": "",
                "error": "",
            }
            try:
                text = self.scraper.get_space_file(candidate.id, filename=filename)
            except (HTTPError, URLError, TimeoutError, socket.timeout, OSError) as exc:
                record["status"] = "error"
                record["error"] = str(exc)
                results.append(record)
                continue

            if text is None:
                record["status"] = "missing"
                results.append(record)
                continue

            folder = output_dir / _slug(candidate.id)
            folder.mkdir(parents=True, exist_ok=True)
            destination = folder / filename
            destination.write_text(text, encoding="utf-8")
            record["status"] = "ok"
            record["path"] = str(destination)
            results.append(record)

        write_json(output_dir / "index.json", results)
        return results

    def check_endpoint_health(self, model_path: str) -> JSON:
        """Liveness probe: does the Space's HARP controls endpoint respond now?

        This is orthogonal to static analysis: a wrapper can be well-formed yet
        its Space may be down (e.g. after a Hugging Face/runtime update). Returns
        ``alive`` with control counts, or ``dead`` with the failure reason.
        """

        try:
            controls = self.endpoint_client.fetch_controls(model_path)
        except (EndpointProbeError, ValueError, HTTPError, URLError, OSError) as exc:
            return {"status": "dead", "reason": str(exc)}

        inputs = controls.get("inputs") if isinstance(controls.get("inputs"), list) else []
        outputs = controls.get("outputs") if isinstance(controls.get("outputs"), list) else []
        return {
            "status": "alive",
            "reason": "",
            "n_inputs": len(inputs),
            "n_outputs": len(outputs),
        }


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


# Backwards-compatible internal alias.
_write_json = write_json


def _tail(text: str, limit: int = 500) -> str:
    text = text.strip()
    return text if len(text) <= limit else "..." + text[-limit:]


def _model_meta(card: Mapping[str, Any]) -> Mapping[str, Any]:
    meta = card.get("meta")
    return meta if isinstance(meta, Mapping) else card


def _slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", value.strip()).strip("-").lower()


def _normalize_hf_model_repo_id(value: str) -> str:
    repo_id = value.strip().rstrip("/")
    if repo_id.startswith(f"{HUGGING_FACE_BASE}/"):
        repo_id = repo_id.removeprefix(f"{HUGGING_FACE_BASE}/")
    if repo_id.startswith("models/"):
        repo_id = repo_id.removeprefix("models/")
    if repo_id.startswith("spaces/"):
        raise ValueError("Expected a Hugging Face model repo, not a Space repo.")
    if repo_id.count("/") != 1:
        raise ValueError("Expected a Hugging Face repo id like 'author/model-name'.")
    return repo_id


_THIRD_PARTY_IMPORT_DENYLIST = frozenset(
    {
        "gradio",
        "spaces",
        "torch",
        "torchaudio",
        "numpy",
        "np",
        "scipy",
        "pandas",
        "librosa",
        "soundfile",
        "sf",
        "matplotlib",
        "transformers",
        "diffusers",
        "huggingface_hub",
        "pyharp",
        "PIL",
        "cv2",
        "tqdm",
        "yaml",
        "requests",
        "einops",
        "safetensors",
        "sklearn",
        "pydub",
    }
)


def _local_import_modules(source: str) -> List[str]:
    """Return module names imported by ``source`` that look first-party.

    Stdlib and well-known third-party packages are filtered out so a Space-source
    crawl follows only the repo's own modules (e.g. ``webui``, ``ensure_models``).
    """

    try:
        tree = ast.parse(source)
    except SyntaxError:
        return []

    stdlib = getattr(sys, "stdlib_module_names", frozenset())
    modules: List[str] = []
    for node in ast.walk(tree):
        names: List[str] = []
        if isinstance(node, ast.ImportFrom) and node.module and node.level == 0:
            names.append(node.module)
        elif isinstance(node, ast.Import):
            names.extend(alias.name for alias in node.names)
        for name in names:
            top = name.split(".")[0]
            if not top or top in stdlib or top in _THIRD_PARTY_IMPORT_DENYLIST:
                continue
            modules.append(name)
    return modules


# Common entry/inference module basenames, highest-signal first. Used to seed
# the GitHub source crawl when the repo isn't a Gradio app with an obvious app.py.
_GITHUB_ENTRY_BASENAMES = (
    "app.py",
    "gradio_app.py",
    "demo.py",
    "webui.py",
    "web_ui.py",
    "inference.py",
    "infer.py",
    "predict.py",
    "pipeline.py",
    "api.py",
    "model.py",
    "run.py",
    "main.py",
    "cli.py",
    "__init__.py",
)

_GITHUB_PATH_SKIP_DIRS = (
    "test",
    "tests",
    "example",
    "examples",
    "docs",
    "doc",
    "scripts",
    "benchmark",
    "benchmarks",
    ".github",
)


def _parse_github_url(url: str) -> Tuple[str, str, Optional[str], str]:
    """Parse a GitHub repo reference into ``(owner, repo, ref, subpath)``.

    Accepts the common shapes::

        https://github.com/owner/repo
        https://github.com/owner/repo.git
        https://github.com/owner/repo/tree/<ref>/<subpath>
        https://github.com/owner/repo/blob/<ref>/<file.py>
        git@github.com:owner/repo.git
        owner/repo

    ``ref`` and ``subpath`` are ``None``/``""`` when not present in the URL. A
    ref containing slashes (e.g. ``feature/x``) is not disambiguated from the
    subpath; pass ``--ref`` explicitly for such branches.
    """

    text = (url or "").strip()
    if not text:
        raise ValueError("empty GitHub repository URL")

    # git@github.com:owner/repo(.git)
    if text.startswith("git@"):
        text = text.split(":", 1)[-1]
    else:
        for prefix in ("https://", "http://", "ssh://", "git://"):
            if text.startswith(prefix):
                text = text[len(prefix) :]
                break
        if text.startswith("github.com/"):
            text = text[len("github.com/") :]
        elif text.startswith("www.github.com/"):
            text = text[len("www.github.com/") :]

    text = text.strip("/")
    parts = [segment for segment in text.split("/") if segment]
    if len(parts) < 2:
        raise ValueError(f"could not parse GitHub owner/repo from '{url}'")

    owner = parts[0]
    repo = parts[1]
    if repo.endswith(".git"):
        repo = repo[: -len(".git")]

    ref: Optional[str] = None
    subpath = ""
    if len(parts) >= 4 and parts[2] in ("tree", "blob"):
        ref = parts[3]
        subpath = "/".join(parts[4:])
    return owner, repo, ref, subpath


def _module_path_candidates(module: str, search_dirs: Iterable[str]) -> List[str]:
    """Map an imported module name to candidate file paths within a repo tree."""

    rel = module.replace(".", "/")
    bases = ["", *[d for d in search_dirs if d]]
    candidates: List[str] = []
    for base in bases:
        prefix = f"{base}/" if base else ""
        candidates.append(f"{prefix}{rel}.py")
        candidates.append(f"{prefix}{rel}/__init__.py")
    # Preserve order while removing duplicates.
    seen: set = set()
    unique: List[str] = []
    for candidate in candidates:
        if candidate not in seen:
            seen.add(candidate)
            unique.append(candidate)
    return unique


def _github_seed_files(
    py_files: Iterable[str], *, subpath: str = "", entry_hints: Iterable[str] = ()
) -> List[str]:
    """Pick the most promising python files to seed the GitHub source crawl."""

    files = list(py_files)
    fileset = set(files)
    seeds: List[str] = []

    def _add(path: str) -> None:
        if path in fileset and path not in seeds:
            seeds.append(path)

    for hint in entry_hints:
        _add(hint)

    # A subpath pointing straight at a file or directory is a strong hint.
    if subpath:
        if subpath.endswith(".py"):
            _add(subpath)
        for path in files:
            if path == subpath or path.startswith(subpath.rstrip("/") + "/"):
                _add(path)

    def _depth(path: str) -> int:
        return path.count("/")

    def _is_skippable(path: str) -> bool:
        return any(seg in _GITHUB_PATH_SKIP_DIRS for seg in path.split("/")[:-1])

    # Known entry basenames, shallowest first, skipping test/doc/example trees.
    for basename in _GITHUB_ENTRY_BASENAMES:
        matches = sorted(
            (p for p in files if p.split("/")[-1] == basename and not _is_skippable(p)),
            key=_depth,
        )
        for path in matches:
            _add(path)

    # Fallback: the shallowest remaining python files (still skipping tests/docs).
    for path in sorted((p for p in files if not _is_skippable(p)), key=_depth):
        _add(path)

    return seeds


_GRADIO_REQ_RE = re.compile(r"^\s*gradio(\[[^\]]*\])?\s*([<>=!~].*)?\s*$", re.IGNORECASE)
_SDK_VERSION_RE = re.compile(r"^(\s*sdk_version\s*:\s*).*$", re.IGNORECASE)
_SDK_GRADIO_RE = re.compile(r"^\s*sdk\s*:\s*gradio\s*$", re.IGNORECASE)
_PINNED_REQ_RE = re.compile(r"^([A-Za-z0-9_.\-]+)(\[[^\]]*\])?==([^\s;#]+)")


def reconcile_requirements(
    text: str,
    gradio_version: str = HARP_GRADIO_VERSION,
    *,
    ensure_pyharp: bool = True,
) -> Tuple[str, List[str]]:
    """Align a requirements.txt with pyharp: pin gradio and ensure pyharp.

    Returns ``(new_text, changes)``. Any ``gradio`` requirement (with or without
    extras / version spec) is rewritten to ``gradio[extras]==<gradio_version>``;
    if none is present one is added. The pyharp git requirement is appended when
    missing. Comments, blank lines, and ``-r``/``-e`` directives are preserved.
    """

    changes: List[str] = []
    out: List[str] = []
    has_pyharp = False
    pinned_gradio = False

    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", "-")):
            out.append(line)
            continue
        if "pyharp" in stripped.lower():
            has_pyharp = True
        match = _GRADIO_REQ_RE.match(line)
        if match:
            extras = match.group(1) or ""
            new_line = f"gradio{extras}=={gradio_version}"
            if new_line != stripped:
                changes.append(f"pinned gradio to {gradio_version} (was '{stripped}')")
            out.append(new_line)
            pinned_gradio = True
            continue
        out.append(line)

    if not pinned_gradio:
        out.append(f"gradio=={gradio_version}")
        changes.append(f"added gradio=={gradio_version}")
    if ensure_pyharp and not has_pyharp:
        out.append(PYHARP_REQUIREMENT)
        changes.append("added the pyharp requirement")

    new_text = "\n".join(out)
    if not new_text.endswith("\n"):
        new_text += "\n"
    return new_text, changes


def reconcile_readme(
    text: str,
    gradio_version: str = HARP_GRADIO_VERSION,
) -> Tuple[str, List[str]]:
    """Set a Space README's ``sdk_version`` front-matter to ``gradio_version``.

    Hugging Face force-installs ``gradio==<sdk_version>``, so this must match
    pyharp's gradio. Returns ``(new_text, changes)``; a no-op when there is no
    YAML front matter.
    """

    lines = text.splitlines()
    if not lines or lines[0].strip() != "---":
        return text, []

    end = None
    for index in range(1, len(lines)):
        if lines[index].strip() == "---":
            end = index
            break
    if end is None:
        return text, []

    changes: List[str] = []
    replaced = False
    for index in range(1, end):
        prefix_match = _SDK_VERSION_RE.match(lines[index])
        if prefix_match:
            new_line = f'{prefix_match.group(1)}"{gradio_version}"'
            if new_line.strip() != lines[index].strip():
                changes.append(f'set README sdk_version to "{gradio_version}"')
            lines[index] = new_line
            replaced = True

    if not replaced:
        insert_at = 1
        for index in range(1, end):
            if _SDK_GRADIO_RE.match(lines[index]):
                insert_at = index + 1
                break
        lines.insert(insert_at, f'sdk_version: "{gradio_version}"')
        changes.append(f'added README sdk_version "{gradio_version}"')

    new_text = "\n".join(lines)
    if text.endswith("\n") and not new_text.endswith("\n"):
        new_text += "\n"
    return new_text, changes


_REQ_NAME_RE = re.compile(r"^([A-Za-z0-9_.\-]+)(\[[^\]]*\])?")

# Audio/text ML libraries whose version drift is the usual cause of "right
# melody/timbre, gibberish words" regressions, and which gradio itself does NOT
# depend on -- so re-pinning them from a known-good freeze is safe. We
# deliberately exclude gradio's own transitive deps (huggingface_hub, numpy,
# pydantic, fastapi, pandas, ...): pinning those from a gradio-6 freeze would
# just reintroduce the gradio==5.28.0 resolution conflict.
_ML_PIN_ALLOWLIST = frozenset(
    {
        "transformers",
        "tokenizers",
        "torch",
        "torchaudio",
        "torchvision",
        "safetensors",
        "sentencepiece",
        "einops",
        "accelerate",
        "librosa",
        "soundfile",
        "soxr",
        "resampy",
        "encodec",
        "descript-audiotools",
        "descript-audio-codec",
        "phonemizer",
        "g2p-en",
        "pypinyin",
        "jieba",
        "inflect",
        "unidecode",
        "num2words",
    }
)


# Packages that must NEVER be pinned from a freeze -- even when the Space declares
# them. These are gradio's own dependency closure plus transport/serialization
# infra. A freeze captured against a *different* gradio (e.g. gradio 6) carries
# versions of these that conflict with gradio==5.28.0 (the classic case:
# huggingface_hub 1.x from a gradio-6 env vs gradio 5.28.0 + transformers needing
# huggingface_hub<1.0). We leave them as the Space declared them so pip can
# resolve a set compatible with gradio 5.28.0.
_FREEZE_BLOCKLIST = frozenset(
    {
        "gradio",
        "gradio-client",
        "huggingface-hub",
        "hf-xet",
        "hf-transfer",
        "numpy",
        "pandas",
        "pillow",
        "pydantic",
        "pydantic-core",
        "fastapi",
        "starlette",
        "anyio",
        "sniffio",
        "h11",
        "httpx",
        "httpcore",
        "uvicorn",
        "websockets",
        "python-multipart",
        "orjson",
        "aiofiles",
        "ffmpy",
        "jinja2",
        "markupsafe",
        "typer",
        "click",
        "rich",
        "shellingham",
        "typing-extensions",
        "typing-inspection",
        "packaging",
        "pyyaml",
        "ruff",
        "semantic-version",
        "tomlkit",
        "safehttpx",
        "groovy",
        "markdown-it-py",
        "mdurl",
        "pygments",
        "requests",
        "certifi",
        "urllib3",
        "charset-normalizer",
        "idna",
        "setuptools",
        "wheel",
        "pip",
    }
)


def _canon(name: str) -> str:
    """Canonicalize a distribution name for case/underscore-insensitive matching."""

    return name.strip().lower().replace("_", "-")


def parse_freeze(text: str) -> Dict[str, Tuple[str, str]]:
    """Parse ``pip freeze`` output into ``{canonical_name: (name, version)}``.

    Editable installs, VCS/URL pins, and lines without an exact ``==`` are
    skipped (they cannot be safely transplanted as a plain version pin).
    """

    frozen: Dict[str, Tuple[str, str]] = {}
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", "-")):
            continue
        if " @ " in stripped or stripped.startswith(("git+", "http", "file:")):
            continue
        match = re.match(r"^([A-Za-z0-9_.\-]+)==([^\s;#]+)", stripped)
        if not match:
            continue
        # Drop local build tags (e.g. torch 2.4.0+cu121) -- they are
        # environment-specific and usually have "no matching distribution" on a
        # Space's default index.
        version = match.group(2).split("+", 1)[0]
        frozen[_canon(match.group(1))] = (match.group(1), version)
    return frozen


def merge_frozen_pins(
    requirements_text: str,
    freeze_text: str,
    *,
    gradio_version: str = HARP_GRADIO_VERSION,
    ensure_pyharp: bool = True,
) -> Tuple[str, List[str]]:
    """Lock a requirements.txt to a known-good ``pip freeze`` closure.

    Every package the Space already declares is re-pinned to its exact version
    from ``freeze_text`` (so nothing drifts when gradio is forced down), a curated
    set of audio/text ML libraries (:data:`_ML_PIN_ALLOWLIST`) is added from the
    freeze even if undeclared (the usual hidden culprits behind gibberish
    output), and gradio/pyharp are still managed exactly like
    :func:`reconcile_requirements`. gradio and pyharp are intentionally *not*
    taken from the freeze. Returns ``(new_text, changes)``.
    """

    frozen = parse_freeze(freeze_text)
    frozen.pop("gradio", None)
    for key in [name for name in frozen if "pyharp" in name]:
        frozen.pop(key)

    changes: List[str] = []
    out: List[str] = []
    has_pyharp = False
    pinned_gradio = False
    declared: set = set()

    for line in requirements_text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", "-")):
            out.append(line)
            continue
        if "pyharp" in stripped.lower():
            has_pyharp = True
        gradio_match = _GRADIO_REQ_RE.match(line)
        if gradio_match:
            new_line = f"gradio{gradio_match.group(1) or ''}=={gradio_version}"
            if new_line != stripped:
                changes.append(f"pinned gradio to {gradio_version} (was '{stripped}')")
            out.append(new_line)
            pinned_gradio = True
            continue
        name_match = _REQ_NAME_RE.match(stripped)
        if name_match:
            name = name_match.group(1)
            extras = name_match.group(2) or ""
            canon = _canon(name)
            declared.add(canon)
            if canon in _FREEZE_BLOCKLIST:
                # gradio-coupled / infra: keep the Space's OWN declared constraint
                # verbatim and never transplant the freeze version. The Space's
                # constraints are deliberate and correct (e.g. 'numpy<2.0.0' for
                # NumPy-1.x C-extensions like numba/pyworld, or 'huggingface_hub>=0.20.0'),
                # whereas a gradio-6-era freeze pin (e.g. huggingface_hub==1.x, numpy 2.x)
                # would either break those extensions or conflict with gradio==<version>.
                if canon in frozen:
                    changes.append(
                        f"kept '{stripped}' as declared (gradio-coupled; not locked from freeze)"
                    )
                out.append(line)
                continue
            if canon in frozen:
                _, version = frozen[canon]
                new_line = f"{name}{extras}=={version}"
                if new_line != stripped:
                    changes.append(f"pinned {name} to {version} (was '{stripped}')")
                out.append(new_line)
                continue
        out.append(line)

    for canon in sorted(_ML_PIN_ALLOWLIST):
        if canon in _FREEZE_BLOCKLIST:
            continue
        if canon in frozen and canon not in declared:
            fname, version = frozen[canon]
            out.append(f"{fname}=={version}")
            changes.append(f"added {fname}=={version} (from freeze)")

    if not pinned_gradio:
        out.append(f"gradio=={gradio_version}")
        changes.append(f"added gradio=={gradio_version}")
    if ensure_pyharp and not has_pyharp:
        out.append(PYHARP_REQUIREMENT)
        changes.append("added the pyharp requirement")

    new_text = "\n".join(out)
    if not new_text.endswith("\n"):
        new_text += "\n"
    return new_text, changes


def lint_generated_app(source: str) -> List[str]:
    """Heuristic warnings for a generated wrapper's ``app.py``.

    Catches the most common ways an auto-generated wrapper silently diverges from
    a Space's real behavior. The headline failure mode it targets: re-implementing
    a model's multi-stage pipeline (e.g. a ``run_preprocess`` step plus a separate
    ``run_svs``/inference step) instead of calling the Space's single high-level
    entry point. That divergence changes preprocessing defaults (sample rate,
    mono, trimming, language, vocal separation) and produces subtle, hard-to-debug
    output corruption -- classically "right melody/timbre, gibberish words".
    """

    warnings: List[str] = []

    has_preprocess = re.search(r"\brun_preprocess\b|\bPreprocessPipeline\b", source)
    has_synth_stage = re.search(
        r"\brun_svs\b|\brun_svs_from_paths\b|\bsvs_process\b", source
    )
    if has_preprocess and has_synth_stage:
        warnings.append(
            "wrapper appears to re-implement the model's multi-stage pipeline (it "
            "calls a preprocess step AND a separate synthesis/SVS step). Prefer "
            "importing and calling the Space's single high-level entry point -- the "
            "function its UI button calls, e.g. `synthesis_function` -- and passing "
            "every parameter through, so preprocessing defaults (sample rate, mono, "
            "trimming, language, vocal separation) match the original exactly."
        )
    if re.search(r"librosa\.load\([^)]*\bsr\s*=\s*None", source):
        warnings.append(
            "wrapper loads audio with `librosa.load(..., sr=None)`, skipping the "
            "resampling/normalization the original app may rely on; a mismatched "
            "sample rate or channel layout can corrupt the downstream lyric/ASR path "
            "(right melody, wrong words)."
        )
    return warnings


def _duplicate_pin_conflicts(text: str) -> List[str]:
    """Report packages pinned with ``==`` to two different versions in one file."""

    pins: Dict[str, str] = {}
    conflicts: List[str] = []
    for line in text.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith(("#", "-")):
            continue
        match = _PINNED_REQ_RE.match(stripped)
        if not match:
            continue
        name = match.group(1).lower()
        version = match.group(3)
        if name in pins and pins[name] != version:
            conflicts.append(f"{name}: {pins[name]} vs {version}")
        else:
            pins.setdefault(name, version)
    return conflicts


def _normalize_space_repo_id(value: str) -> str:
    """Reduce a Space URL/path to a bare ``username/space-name`` id."""

    repo_id = value.strip().rstrip("/")
    if repo_id.startswith(f"{HUGGING_FACE_BASE}/"):
        repo_id = repo_id.removeprefix(f"{HUGGING_FACE_BASE}/")
    if repo_id.startswith("spaces/"):
        repo_id = repo_id.removeprefix("spaces/")
    return repo_id


def _short_description(card: Mapping[str, Any]) -> str:
    readme = str(card.get("readme") or "")
    for line in readme.splitlines():
        line = line.strip()
        if line and not line.startswith(("#", "<!--", "---", "license", "tags:")):
            return line[:200]
    return "Auto-generated HARP wrapper."


def _render_generated_space_readme(card: Mapping[str, Any], task: str) -> str:
    meta = _model_meta(card)
    repo_id = str(meta.get("id") or "unknown/unknown")
    model_name = repo_id.split("/")[-1].replace("_", " ").replace("-", " ").title()
    license_name = str(meta.get("license") or "").strip().lower() or "other"
    description = _short_description(card)

    return "\n".join(
        [
            "---",
            f"title: {model_name}",
            "colorFrom: indigo",
            "colorTo: gray",
            "sdk: gradio",
            "sdk_version: 5.28.0",
            "app_file: app.py",
            "pinned: false",
            f"license: {license_name}",
            "---",
            "",
            f"# {model_name}",
            "",
            f"Generated HARP wrapper for `{repo_id}`.",
            "",
            f"- Source model: {HUGGING_FACE_BASE}/{repo_id}",
            f"- Task: `{task}`",
            "",
            description,
            "",
        ]
    )


def _render_readme(package: ModelPackage) -> str:
    card = package.card
    tags = ", ".join(str(tag) for tag in card.get("tags", []))
    title = card.get("name") or package.model_path
    author = card.get("author") or (package.space.author if package.space else "")
    license_name = package.space.license if package.space else ""

    lines = [
        f"# {title}",
        "",
        f"- HARP path: `{package.model_path}`",
        f"- Documentation: {package.documentation_url}",
        f"- Endpoint: {package.endpoint_url}",
    ]
    if author:
        lines.append(f"- Author: {author}")
    if license_name:
        lines.append(f"- License: {license_name}")
    if tags:
        lines.append(f"- Tags: {tags}")
    lines.extend(
        [
            "",
            "## Description",
            "",
            str(card.get("description") or "No description returned by the endpoint."),
            "",
            "## HARP Contract",
            "",
            f"- Inputs: {len(package.inputs)}",
            f"- Outputs: {len(package.outputs)}",
            "",
        ]
    )
    return "\n".join(lines)
