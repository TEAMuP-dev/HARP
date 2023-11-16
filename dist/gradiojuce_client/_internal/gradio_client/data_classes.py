from __future__ import annotations

from typing import TypedDict

from typing_extensions import NotRequired


class FileData(TypedDict):
    name: str | None  # filename
    data: str | None  # base64 encoded data
    size: NotRequired[int | None]  # size in bytes
    is_file: NotRequired[
        bool
    ]  # whether the data corresponds to a file or base64 encoded data
    orig_name: NotRequired[str]  # original filename
    mime_type: NotRequired[str]
    is_stream: NotRequired[bool]
