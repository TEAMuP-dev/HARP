"""
Refusal checks for HARP model validation.

A test case with `expect_error` sends an input the model cannot use, and passes
only if the model refuses it the way docs/DEPLOYMENT.md asks: with a gr.Error
whose message says what was wrong. HARP shows that message to the user. Any
other exception reaches HARP as a RuntimeError carrying only its own text, such
as "list index out of range", which reads as a crash rather than as advice.

Telling the two apart takes the error payload Gradio sends, since it carries a
`title` for a gr.Error and none for any other exception. gradio_client builds
its AppError from that payload with defaults filling whatever is missing, so
the title alone cannot say whether the app sent one. install() therefore
replaces the AppError gradio_client raises with a subclass that records which
fields the payload carried.
"""

import gradio_client.client
from gradio_client.exceptions import AppError


__all__ = [
    'DeclaredAppError',
    'install',
    'judge_refusal'
]


# The titles of the errors raised by ZeroGPU and by pyharp itself rather than
# by an app. ZeroGPU prefixes every title it uses with "ZeroGPU" (its worker,
# quota, queue, and client errors), and pyharp names a worker that died or a
# job that was canceled.
INFRASTRUCTURE_TITLES = ("ZeroGPU",)
PYHARP_TITLES = {"Processing failed", "Canceled"}


class DeclaredAppError(AppError):
    """
    An AppError that records which fields the error payload of the app carried.

    Gradio adds `title`, `duration` and `visible` to the payload of a gr.Error
    only, so `"title" in payload_fields` is what marks a deliberate refusal.
    """

    def __init__(self, message: str = "Error raised.", **fields):
        super().__init__(message, **fields)
        self.payload_fields = set(fields)


def install() -> None:
    """
    Make gradio_client raise DeclaredAppError, which records the payload fields.

    It raises AppError by its module-level name in gradio_client.client, so
    rebinding that name is enough, and code catching AppError still catches
    the subclass. Installing twice is harmless.
    """

    gradio_client.client.AppError = DeclaredAppError


def is_bare_type_name(text: str) -> bool:
    """
    Whether a message is only the type name of an exception, e.g. "RuntimeError".

    pyharp forwards the type name when an exception carries no text, and
    ZeroGPU's worker error carries only the type of what was raised.

    Args:
        text (str): An error message.

    Returns:
        bare (bool): True when the message is a single identifier ending in
            Error or Exception.
    """

    stripped = text.strip().strip("'\"")

    return stripped.isidentifier() and stripped.endswith(("Error", "Exception"))


def judge_refusal(exc: Exception, expected) -> tuple:
    """
    Decide whether an error raised by /process is the refusal a case expects.

    Args:
        exc (Exception): What the /process call raised.
        expected (bool | str): The `expect_error` of the case, True for any
            readable refusal, or text the message has to contain.

    Returns:
        refused (bool): True when the model refused the input as it should.
        problem (str): Why it does not count as a refusal, or empty.
        reached_gpu (bool): Whether the error shows that the request got past
            the checks of the model to a GPU call, or may have.
    """

    if isinstance(exc, TimeoutError):
        return False, (f"no refusal arrived ({exc}), so the model went on to process "
                       f"an input it should refuse"), True

    if not isinstance(exc, AppError):
        return False, f"/process failed rather than refusing: {type(exc).__name__}: {exc}", True

    message = str(getattr(exc, "message", "") or "").strip()
    title = str(getattr(exc, "title", "") or "")
    fields = getattr(exc, "payload_fields", None)

    if fields is not None and "title" not in fields:
        shown = f"'{message}'" if message else "no message at all"
        return False, (f"refused only by an uncaught exception, which reaches HARP as "
                       f"{shown} rather than a gr.Error saying what was wrong"), False

    if title.startswith(INFRASTRUCTURE_TITLES):
        return False, (f"failed in ZeroGPU ({title}: {message}) rather than being refused "
                       f"before the GPU was reserved"), True

    if title in PYHARP_TITLES:
        return False, f"failed in pyharp ({title}: {message}) rather than being refused", True

    if not message or is_bare_type_name(message):
        return False, (f"refused with a gr.Error that says nothing a user can act on: "
                       f"'{message}'"), False

    if isinstance(expected, str) and expected.lower() not in message.lower():
        return False, (f"refused, but the message does not mention '{expected}': "
                       f"'{message}'"), False

    return True, "", False
