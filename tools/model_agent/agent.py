from __future__ import annotations

import json
import queue
import re
import socket
import subprocess
import sys
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


JSON = Dict[str, Any]
HUGGING_FACE_BASE = "https://huggingface.co"
KNOWN_HYPHENATED_SPACE_ORGS = ("teamup-tech",)

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
    ):
        self.scraper = scraper or HuggingFaceSpaceScraper()
        self.endpoint_client = endpoint_client or HarpEndpointClient()

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
