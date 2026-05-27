from __future__ import annotations

import json
import re
import socket
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, Iterable, List, Mapping, Optional
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlencode
from urllib.request import Request, urlopen


JSON = Dict[str, Any]
HUGGING_FACE_BASE = "https://huggingface.co"


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

    def _get_json(self, url: str) -> Any:
        headers = {"User-Agent": "harp-model-agent/0.1"}
        if self.token:
            headers["Authorization"] = f"Bearer {self.token}"
        request = Request(url, headers=headers)
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
            # Short Space URLs flatten "org/model" into "org-model". Known
            # hyphenated orgs need to be restored before the generic split.
            for org in ("teamup-tech",):
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
                "User-Agent": "harp-model-agent/0.1",
            },
            method="POST",
        )
        try:
            with urlopen(request, timeout=self.timeout) as response:
                return json.loads(response.read().decode("utf-8"))
        except (HTTPError, URLError, TimeoutError, socket.timeout) as exc:
            raise EndpointProbeError(f"POST {url} failed: {exc}") from exc

    def _get_text(self, url: str) -> str:
        request = Request(url, headers={"Accept": "*/*", "User-Agent": "harp-model-agent/0.1"})
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

    def package_model(self, model_path: str, *, include_space_metadata: bool = True) -> ModelPackage:
        controls = self.endpoint_client.fetch_controls(model_path)
        host_slash_model = self.endpoint_client.infer_host_slash_model(model_path)
        space = self.scraper.get_space(host_slash_model) if include_space_metadata and "/" in host_slash_model else None
        card = controls.get("card") if isinstance(controls.get("card"), dict) else {}
        inputs = controls.get("inputs") if isinstance(controls.get("inputs"), list) else []
        outputs = controls.get("outputs") if isinstance(controls.get("outputs"), list) else []

        return ModelPackage(
            model_path=host_slash_model,
            source_url=f"{HUGGING_FACE_BASE}/spaces/{host_slash_model}" if "/" in host_slash_model else "",
            endpoint_url=self.endpoint_client.infer_endpoint_url(model_path),
            documentation_url=self.endpoint_client.infer_documentation_url(model_path),
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


def _write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", value.strip()).strip("-").lower()


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
