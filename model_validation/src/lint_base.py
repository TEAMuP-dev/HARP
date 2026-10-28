"""
What the lint modules share: the rules, with their severity and the section of
docs/DEPLOYMENT.md each comes from, the record of one finding, and the record of
what the lint reads from one Space.
"""

import dataclasses


__all__ = [
    'ERROR',
    'WARNING',
    'RULES',
    'DEPLOYMENT_GUIDE',
    'Finding',
    'SpaceFiles'
]


ERROR, WARNING = "error", "warning"


# Every rule, with its severity and the section of docs/DEPLOYMENT.md it comes
# from (an anchor of that file, or None for one the guide does not state)
RULES = {
    # app.py
    "spaces-first": (ERROR, "choosing-the-hardware"),
    "spaces-fallback": (ERROR, "hardware-portability"),
    "gpu-on-process-fn": (ERROR, "protecting-the-callers-allowance"),
    "load-outside-worker": (ERROR, "worker-processes"),
    "load-in-gpu-call": (ERROR, "hardware-portability"),
    "interface-outside-main": (ERROR, "worker-processes"),
    "card-order": (ERROR, "structure-of-apppy"),
    "show-error": (ERROR, "error-reporting"),
    "queue": (ERROR, "structure-of-apppy"),
    "input-format": (ERROR, "structure-of-apppy"),
    "aiff-container": (ERROR, "container"),
    "info-warning": (ERROR, "error-reporting"),
    "unpinned-revision": (WARNING, "upstream-hub-repository"),
    # Requirements
    "pyharp-pin": (ERROR, "versions-and-compatibility"),
    "gradio-listed": (ERROR, "versions-and-compatibility"),
    "spaces-listed": (ERROR, "hardware-portability"),
    "spaces-exact": (ERROR, "hardware-portability"),
    "zerogpu-torch": (ERROR, "choosing-the-hardware"),
    "torchaudio-match": (ERROR, "versions-and-compatibility"),
    "url-unpinned": (ERROR, "versions-and-compatibility"),
    "unpinned": (WARNING, "versions-and-compatibility"),
    "resolves": (ERROR, None),
    # The checks themselves
    "app-file": (ERROR, "structure-of-apppy"),
    "stale-exception": (WARNING, None),
}


DEPLOYMENT_GUIDE = "docs/DEPLOYMENT.md"


@dataclasses.dataclass
class Finding:
    """One way in which a Space departs from the deployment guide."""

    rule: str
    message: str
    line: int | None = None
    file: str = ""
    severity: str = dataclasses.field(init=False)
                                     # ERROR fails the Space, WARNING is only
                                     # reported (see RULES)

    def __post_init__(self):
        self.severity = RULES[self.rule][0]

    def describe(self) -> str:
        """
        Render the finding on one line, with where it is and the guide section.

        Returns:
            description (str): e.g. "app.py:12: spaces imported after torch
                (docs/DEPLOYMENT.md#choosing-the-hardware)".
        """

        where = self.file + (f":{self.line}" if self.line else "")
        anchor = RULES[self.rule][1]
        section = f" ({DEPLOYMENT_GUIDE}#{anchor})" if anchor else ""

        return f"{where + ': ' if where else ''}{self.message}{section}"


@dataclasses.dataclass
class SpaceFiles:
    """What the lint reads from one Space."""

    space_id: str
    sdk: str = "gradio"              # "gradio" or "docker"
    sdk_version: str = ""
    python_version: str = ""         # as the README sets it, or empty
    hardware: str = ""
    app_file: str = "app.py"
    files: dict = dataclasses.field(default_factory=dict)
                                     # name -> text, for the files read
    listing: list = dataclasses.field(default_factory=list)
                                     # every path in the repository

    @property
    def zerogpu(self) -> bool:
        """Whether the Space runs on ZeroGPU hardware."""

        return self.hardware.lower().startswith("zero")
