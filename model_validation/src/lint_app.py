"""
The lint of the app file of a Space against the structural rules of
docs/DEPLOYMENT.md, by reading its syntax tree rather than running it: where
the model is loaded, what the GPU function does, where the interface is built,
and what the components and the launch declare. lint.py puts these findings
beside those of lint_requirements.py.
"""

import ast
import re
import sys
from pathlib import Path

from lint_base import Finding


__all__ = [
    'LocalModules',
    'lint_app',
    'lint_containers',
    'lint_revisions'
]


# Calls that load a model or fetch its weights. The final name is enough for
# most, and the dotted ones are too generic to match by their last part alone.
LOADER_NAMES = {
    "from_pretrained", "from_hparams", "from_checkpoint", "from_local",
    "load_model", "load_models", "load_checkpoint", "load_state_dict",
    "load_pretrained_model", "load_separator", "load_finetuned", "load_compiled", "prepare_model",
    "get_model", "get_pretrained", "hf_hub_download", "snapshot_download",
    "download_model_from_huggingface", "preload_models", "init_df",
}


LOADER_DOTTED = {"torch.load", "torch.hub.load", "torch.jit.load", "whisper.load_model",
                 "Interface.default"}


# Calls that start a function in the background, whose target therefore runs
THREAD_STARTERS = {"Thread", "submit", "start_new_thread"}


# Methods that move a model, which inside a GPU call ZeroGPU repeats on every call
MOVE_METHODS = {"to", "cuda"}


# How many calls deep the lint follows the Space's own functions
MAX_CALL_DEPTH = 4


# The Gradio names that are not components, so are no sign of an interface
GRADIO_NON_COMPONENTS = {"Error", "Info", "Warning", "Progress", "update", "skip"}


def is_name_guard(node, negated: bool) -> bool:
    """
    Whether a statement is `if __name__ == "__main__":`, or its negation.

    A guard that adds a condition with `and`, such as `if __name__ !=
    "__main__" and checkpoint_ok:`, still runs in one process alone, so it
    counts as the guard.

    Args:
        node (ast.AST): A statement.
        negated (bool): True to match `!=`, False to match `==`.

    Returns:
        matched (bool): True when the statement is that guard.
    """

    if not isinstance(node, ast.If):
        return False

    wanted = '__name__ != "__main__"' if negated else '__name__ == "__main__"'
    tests = node.test.values if isinstance(node.test, ast.BoolOp) \
        and isinstance(node.test.op, ast.And) else [node.test]

    return any(isinstance(test, ast.Compare)
               and ast.unparse(test).replace("'", '"') == wanted for test in tests)


def dotted(node) -> str:
    """
    The dotted name an expression spells, e.g. "torch.hub.load".

    Args:
        node (ast.AST): A Name or Attribute expression.

    Returns:
        name (str): The dotted name, or empty for anything else.
    """

    if isinstance(node, ast.Name):
        return node.id
    if isinstance(node, ast.Attribute):
        base = dotted(node.value)
        return f"{base}.{node.attr}" if base else node.attr
    if isinstance(node, ast.Call):
        return dotted(node.func)

    return ""


def is_loader(call: ast.Call) -> bool:
    """
    Whether a call loads a model or fetches its weights.

    Args:
        call (ast.Call): The call.

    Returns:
        loads (bool): True for a call in LOADER_NAMES or LOADER_DOTTED.
    """

    name = dotted(call.func)

    return name.split(".")[-1] in LOADER_NAMES or any(
        name == loader or name.endswith(f".{loader}") for loader in LOADER_DOTTED)


def statements_outside(body: list, skip_guards: bool = True):
    """
    Walk the statements that run when the module is imported.

    Function and class bodies are left out, since they run only when called,
    and so are the two `__name__` guards, which the caller handles.

    Args:
        body (list): Statements of a module or of a block within it.
        skip_guards (bool): Whether to leave out the `__name__` guards.

    Yields:
        node (ast.stmt): Each statement that runs at import, nested blocks
            included.
    """

    for node in body:
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef,
                             ast.Import, ast.ImportFrom)):
            continue
        if skip_guards and (is_name_guard(node, True) or is_name_guard(node, False)):
            continue
        yield node
        for field in ("body", "orelse", "finalbody", "handlers"):
            nested = getattr(node, field, None)
            if isinstance(nested, list):
                yield from statements_outside(nested, skip_guards)


def calls_in(node) -> list:
    """
    The calls a statement makes directly, outside any function it defines.

    Args:
        node (ast.AST): The statement.

    Returns:
        calls (list): ast.Call nodes.
    """

    found = []

    def visit(current):
        for child in ast.iter_child_nodes(current):
            if isinstance(child, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef,
                                  ast.Lambda)):
                continue
            if isinstance(child, ast.Call):
                found.append(child)
            visit(child)

    if isinstance(node, ast.Call):
        found.append(node)
    visit(node)

    return found


def imports_outside(body: list):
    """
    Walk the import statements that run when the module is imported.

    Args:
        body (list): Statements of a module or of a block within it.

    Yields:
        node (ast.Import | ast.ImportFrom): Each import outside functions,
            classes, and the two `__name__` guards.
    """

    for node in body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            yield node
            continue
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)) \
                or is_name_guard(node, True) or is_name_guard(node, False):
            continue
        for field in ("body", "orelse", "finalbody", "handlers"):
            nested = getattr(node, field, None)
            if isinstance(nested, list):
                yield from imports_outside(nested)


def imported_modules(node, local: set) -> list:
    """
    The Space's own modules an import statement names.

    Args:
        node (ast.Import | ast.ImportFrom): The statement.
        local (set): Names of the Space's top-level modules.

    Returns:
        modules (list): Each local module imported.
    """

    if isinstance(node, ast.ImportFrom):
        return [node.module] if node.level == 0 and node.module in local else []

    return [alias.name for alias in node.names if alias.name in local]


class LocalModules:
    """
    The own top-level modules of the Space, into which the lint follows calls.

    A Space often keeps its loading in a module beside the app, such as
    `runtime.py`, so `warmup()` in the app loads only through that module.
    """

    def __init__(self, files: dict):
        self.trees = {}
        for name, text in files.items():
            if name.endswith(".py") and "/" not in name:
                try:
                    self.trees[name[:-3]] = ast.parse(text)
                except SyntaxError:
                    continue
        self._tracers = {}

    def tracer(self, module: str):
        """
        The LoaderTracer of one local module.

        Args:
            module (str): The name of the module.

        Returns:
            tracer (LoaderTracer | None): Its tracer, or None when the Space
                has no such module.
        """

        if module not in self.trees:
            return None
        if module not in self._tracers:
            self._tracers[module] = LoaderTracer(self.trees[module], self)

        return self._tracers[module]

    def loads_at_import(self, module: str, seen: set | None = None) -> str:
        """
        What a local module loads at its own module scope, when imported.

        Args:
            module (str): The name of the module.
            seen (set | None): Modules already followed, against cycles.

        Returns:
            loader (str): The name of the first call that loads, or empty
                when importing the module loads nothing.
        """

        seen = seen if seen is not None else set()
        if module in seen or module not in self.trees:
            return ""
        seen.add(module)

        tree, tracer = self.trees[module], self.tracer(module)
        for node in statements_outside(tree.body):
            for call in calls_in(node):
                if tracer.call_loads(call):
                    return dotted(call.func)
        for node in imports_outside(tree.body):
            for imported in imported_modules(node, set(self.trees)):
                loader = self.loads_at_import(imported, seen)
                if loader:
                    return loader

        return ""


class LoaderTracer:
    """
    Decides whether a call loads a model, following the Space's own functions.

    A call to a function or class the module defines loads when its body does,
    so `load_assets()` at module scope is found even though only its body
    names hf_hub_download. Calls into the Space's other top-level modules are
    followed too, given them. A function passed to a thread counts as called.
    """

    def __init__(self, tree: ast.Module, modules: LocalModules | None = None):
        self.tree = tree
        self.functions = {node.name: node for node in tree.body
                          if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))}
        self.classes = {node.name: node for node in tree.body
                        if isinstance(node, ast.ClassDef)}
        self.modules = modules
        # alias -> (local module, imported name, or None for the module itself)
        self.imports = {}
        if modules is not None:
            for node in ast.walk(tree):
                if isinstance(node, ast.ImportFrom) and node.level == 0 \
                        and node.module in modules.trees:
                    for alias in node.names:
                        self.imports[alias.asname or alias.name] = (node.module, alias.name)
                elif isinstance(node, ast.Import):
                    for alias in node.names:
                        if alias.name in modules.trees:
                            self.imports[alias.asname or alias.name] = (alias.name, None)
        self._memo = {}
        self._loaded_globals = None

    def resolve(self, name: str) -> tuple:
        """
        The Space's own function or class a dotted call name reaches.

        Args:
            name (str): The dotted name of the call, e.g. "runtime.load_model".

        Returns:
            target (tuple): (tracer, name) of the module that defines it and
                its name there, or (None, "") for anything else.
        """

        if name in self.functions or name in self.classes:
            return self, name

        head, _, rest = name.partition(".")
        if head not in self.imports:
            return None, ""
        module, imported = self.imports[head]
        target = rest if imported is None else (imported if not rest else "")
        tracer = self.modules.tracer(module)
        if tracer is None or not target or "." in target:
            return None, ""

        return tracer, target

    def body_loads(self, name: str, depth: int = 0) -> bool:
        """
        Whether calling a module-level function or class loads a model.

        Args:
            name (str): The function or class name.
            depth (int): How far the search has already followed calls.

        Returns:
            loads (bool): True when its body loads, directly or through a
                further module-level function.
        """

        if name in self._memo:
            return self._memo[name]
        if depth > MAX_CALL_DEPTH:
            return False

        self._memo[name] = False
        if name in self.functions:
            bodies = [self.functions[name]]
        elif name in self.classes:
            bodies = [node for node in self.classes[name].body
                      if isinstance(node, ast.FunctionDef) and node.name == "__init__"]
        else:
            return False

        loads = any(self.call_loads(call, depth + 1)
                    for body in bodies for call in ast.walk(body)
                    if isinstance(call, ast.Call) and call is not body)
        self._memo[name] = loads

        return loads

    def call_loads(self, call: ast.Call, depth: int = 0) -> bool:
        """
        Whether one call loads a model.

        Args:
            call (ast.Call): The call.
            depth (int): How far the search has already followed calls.

        Returns:
            loads (bool): True for a loader, or for a call into the code of the
                module itself that reaches one.
        """

        if is_loader(call):
            return True

        name = dotted(call.func)
        tracer, target = self.resolve(name)
        if tracer is not None:
            return tracer.body_loads(target, depth)

        # A thread or pool given one of the Space's functions runs it
        if name.split(".")[-1] in THREAD_STARTERS:
            targets = [kw.value for kw in call.keywords if kw.arg == "target"] + call.args[:1]
            for candidate in targets:
                tracer, target = self.resolve(dotted(candidate))
                if tracer is not None and tracer.body_loads(target, depth):
                    return True

        return False

    def loaded_globals(self) -> set:
        """
        The module-level names bound to what a loading call returns.

        Returns:
            names (set): Names assigned from a loader at module scope or in
                the guard of the worker.
        """

        if self._loaded_globals is None:
            self._loaded_globals = set()
            blocks = [self.tree.body] + [node.body for node in self.tree.body
                                         if is_name_guard(node, True)]
            for block in blocks:
                for node in statements_outside(block):
                    if isinstance(node, ast.Assign) and isinstance(node.value, ast.Call) \
                            and self.call_loads(node.value) and not self.call_places(node.value):
                        self._loaded_globals |= assigned_names(node)

        return set(self._loaded_globals)

    def call_places(self, call: ast.Call) -> bool:
        """
        Whether a loading call puts what it builds on a device itself.

        Moving the result again is then a no-op, which costs nothing inside a
        GPU call.

        Args:
            call (ast.Call): The call.

        Returns:
            places (bool): True when the call passes a device, by keyword or
                as an argument named for one, or when the Space's own function
                it reaches moves something.
        """

        if any(keyword.arg in ("device", "map_location") for keyword in call.keywords) or any(
                "device" in dotted(argument).lower() for argument in call.args):
            return True

        tracer, target = self.resolve(dotted(call.func))
        function = tracer.functions.get(target) if tracer is not None else None

        return function is not None and any(
            isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute)
            and node.func.attr in MOVE_METHODS for node in ast.walk(function))


def assigned_names(node: ast.Assign) -> set:
    """
    The plain names an assignment binds, tuple targets included.

    Args:
        node (ast.Assign): The assignment.

    Returns:
        names (set): Each name bound.
    """

    names = set()
    for target in node.targets:
        elements = target.elts if isinstance(target, (ast.Tuple, ast.List)) else [target]
        names |= {element.id for element in elements if isinstance(element, ast.Name)}

    return names


def startup_functions(tree: ast.Module, tracer: LoaderTracer) -> set:
    """
    The Space's own functions the app runs as it starts, in either process.

    A loader one of these runs has already run by the time a request arrives,
    so a call to it from the GPU function reads a cache rather than loading.

    Args:
        tree (ast.Module): The parsed app.
        tracer (LoaderTracer): The tracer of the app.

    Returns:
        started (set): (id of the tracer of the defining module, name) per
            function reached from module scope or the guard of the worker.
    """

    started = set()

    def mark(owner, name, depth):
        key = (id(owner), name)
        if key in started or depth > MAX_CALL_DEPTH:
            return
        started.add(key)
        node = owner.functions.get(name) or owner.classes.get(name)
        for call in ast.walk(node) if node is not None else []:
            if isinstance(call, ast.Call):
                callee, target = owner.resolve(dotted(call.func))
                if callee is not None:
                    mark(callee, target, depth + 1)

    blocks = [tree.body] + [node.body for node in tree.body if is_name_guard(node, True)]
    for block in blocks:
        for node in statements_outside(block):
            for call in calls_in(node):
                callee, target = tracer.resolve(dotted(call.func))
                if callee is not None:
                    mark(callee, target, 0)

    return started


def gpu_call_loading(tree: ast.Module, tracer: LoaderTracer) -> list:
    """
    The loading each GPU function does, which ZeroGPU repeats on every call.

    ZeroGPU runs each call in a fresh fork, so a model a GPU function builds,
    or moves to the device, is built or moved again on every call, whatever
    cache holds it. The functions the GPU function calls are followed into,
    except those that already ran at startup, whose caches the fork inherits.

    Args:
        tree (ast.Module): The parsed app.
        tracer (LoaderTracer): The tracer of the app.

    Returns:
        found (list): (node, message) per GPU function that loads.
    """

    started = startup_functions(tree, tracer)

    def visit(owner, function, through, depth):
        loaded = owner.loaded_globals()
        for node in ast.walk(function):
            if isinstance(node, ast.Assign) and isinstance(node.value, ast.Call) \
                    and owner.call_loads(node.value) and not owner.call_places(node.value):
                loaded |= assigned_names(node)
        via = f", through {'() and '.join(through)}()" if through else ""

        for call in (node for node in ast.walk(function) if isinstance(node, ast.Call)):
            name = dotted(call.func)
            callee, target = owner.resolve(name)
            if callee is not None and (id(callee), target) in started:
                continue
            if is_loader(call):
                return call, f"{name}() runs inside the GPU call{via}, so ZeroGPU repeats it on " \
                             f"every call. Load in the worker at startup"
            if isinstance(call.func, ast.Attribute) and call.func.attr in MOVE_METHODS \
                    and isinstance(call.func.value, ast.Name) and call.func.value.id in loaded:
                return call, f"{name}() moves a model inside the GPU call{via}, so ZeroGPU " \
                             f"repeats it on every call. Place it in the worker at startup"
            if callee is None or depth >= MAX_CALL_DEPTH:
                continue
            if target in callee.functions:
                found = visit(callee, callee.functions[target], through + [target], depth + 1)
                if found:
                    return found
            elif callee.body_loads(target):
                return call, f"{name}() builds a model inside the GPU call{via}, so ZeroGPU " \
                             f"builds it again on every call. Build it in the worker at startup"

        return None

    found = []
    for function in (node for node in ast.walk(tree)
                     if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
                     and any(is_gpu_decorator(d) for d in node.decorator_list)):
        loading = visit(tracer, function, [], 0)
        if loading:
            found.append(loading)

    return found


def process_fn_name(tree: ast.Module) -> str:
    """
    The name of the function the app passes to build_endpoint as process_fn.

    Args:
        tree (ast.Module): The parsed app.

    Returns:
        name (str): The name of the function, "process_fn" when none is passed
            by name.
    """

    for call in ast.walk(tree):
        if isinstance(call, ast.Call) and dotted(call.func).endswith("build_endpoint"):
            for keyword in call.keywords:
                if keyword.arg == "process_fn" and isinstance(keyword.value, ast.Name):
                    return keyword.value.id
            if len(call.args) >= 4 and isinstance(call.args[3], ast.Name):
                return call.args[3].id

    return "process_fn"


def base_call(node):
    """
    The call a chain of method calls starts from, e.g. gr.Audio(...) in
    gr.Audio(...).harp_required(True).set_info("...").

    Args:
        node (ast.AST): An expression.

    Returns:
        call (ast.Call | None): The first call of the chain, or None when the
            expression is not a call.
    """

    while isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
            and isinstance(node.func.value, ast.Call):
        node = node.func.value

    return node if isinstance(node, ast.Call) else None


def input_component_calls(tree: ast.Module) -> list:
    """
    The calls that build the components passed to build_endpoint as inputs.

    The inputs are a list written in the call, or a name bound to one, and
    each element is a component or a name bound to one.

    Args:
        tree (ast.Module): The parsed app.

    Returns:
        calls (list): ast.Call per input component found.
    """

    assigned = {}
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name):
                    assigned[target.id] = node.value

    def resolve(node):
        return assigned.get(node.id, node) if isinstance(node, ast.Name) else node

    calls = []
    for call in ast.walk(tree):
        if not (isinstance(call, ast.Call) and dotted(call.func).endswith("build_endpoint")):
            continue
        inputs = next((kw.value for kw in call.keywords if kw.arg == "input_components"),
                      call.args[1] if len(call.args) > 1 else None)
        inputs = resolve(inputs) if inputs is not None else None
        if isinstance(inputs, (ast.List, ast.Tuple)):
            for element in inputs.elts:
                component = base_call(resolve(element))
                if component is not None:
                    calls.append(component)

    return calls


def is_gpu_decorator(node) -> bool:
    """
    Whether a decorator is spaces.GPU, called with arguments or not.

    Args:
        node (ast.AST): The decorator expression.

    Returns:
        gpu (bool): True for `@spaces.GPU`, `@spaces.GPU(...)` or `@GPU`.
    """

    name = dotted(node.func if isinstance(node, ast.Call) else node)

    return name in ("spaces.GPU", "GPU")


def lint_app(text: str, file: str, modules: LocalModules | None = None,
             zerogpu: bool = False) -> list:
    """
    Check an app file against the structural rules of docs/DEPLOYMENT.md.

    Args:
        text (str): The contents of the file.
        file (str): Its name, for reporting.
        modules (LocalModules | None): The top-level modules of the Space, into
            which to follow the calls of the app, or None to read the app alone.
        zerogpu (bool): Whether the Space runs on ZeroGPU, where a GPU
            function that loads repeats it on every call.

    Returns:
        findings (list): Finding objects, in no particular order.
    """

    try:
        tree = ast.parse(text)
    except SyntaxError as exc:
        return [Finding("app-file", f"does not parse: {exc.msg}", exc.lineno, file)]

    findings = []

    def find(rule, message, node=None):
        findings.append(Finding(rule, message, getattr(node, "lineno", None), file))

    # --- spaces first, and imported plainly ----------------------------------
    stdlib = set(sys.stdlib_module_names) | {"__future__"}
    first_third_party = None
    spaces_import = None
    for node in tree.body:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            names = ([alias.name for alias in node.names] if isinstance(node, ast.Import)
                     else [node.module or ""])
            roots = {name.split(".")[0] for name in names}
            if "spaces" in roots:
                spaces_import = node
                break
            third_party = roots - stdlib
            if third_party and first_third_party is None:
                first_third_party = (node, sorted(third_party)[0])

    if spaces_import is not None and first_third_party is not None:
        node, module = first_third_party
        find("spaces-first", f"spaces is imported after {module} (line {node.lineno}), "
                             f"which initializes CUDA first on ZeroGPU", spaces_import)

    for node in ast.walk(tree):
        if isinstance(node, ast.Try) and any(
                isinstance(stmt, (ast.Import, ast.ImportFrom))
                and "spaces" in ast.unparse(stmt).split() for stmt in node.body):
            find("spaces-fallback", "spaces is imported inside a try, as a fallback for "
                                    "it missing, though it is always installed", node)

    # --- the GPU decorator, off process_fn -----------------------------------
    process_name = process_fn_name(tree)
    functions = {node.name: node for node in ast.walk(tree)
                 if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))}
    process = functions.get(process_name)
    if process is not None and any(is_gpu_decorator(d) for d in process.decorator_list):
        find("gpu-on-process-fn", f"@spaces.GPU decorates {process_name}, so the GPU is "
                                  f"reserved before its input is checked", process)

    # --- loading, in the worker alone ----------------------------------------
    tracer = LoaderTracer(tree, modules)
    both = None
    for node in statements_outside(tree.body):
        loaders = [call for call in calls_in(node) if tracer.call_loads(call)]
        if loaders:
            both = node, f"{dotted(loaders[0].func)}() runs at module scope, in both the app " \
                         f"and the worker, rather than under `if __name__ != \"__main__\":`"
            break
    for node in imports_outside(tree.body) if modules is not None else []:
        for module in imported_modules(node, set(modules.trees)):
            loader = modules.loads_at_import(module)
            if loader and (both is None or node.lineno < both[0].lineno):
                both = node, f"importing {module} runs {loader}() at module scope, in both " \
                             f"the app and the worker. Load in a function the worker calls " \
                             f"under `if __name__ != \"__main__\":`"
    if both is not None:
        find("load-outside-worker", both[1], both[0])

    for guard in (node for node in tree.body if is_name_guard(node, False)):
        for node in statements_outside(guard.body, skip_guards=False):
            loaders = [call for call in calls_in(node) if tracer.call_loads(call)]
            if loaders:
                find("load-outside-worker",
                     f"{dotted(loaders[0].func)}() runs in the `__main__` block, in the "
                     f"app alone, which never runs process_fn", node)
                break

    # --- nothing loaded inside a GPU call ------------------------------------
    for node, message in gpu_call_loading(tree, tracer) if zerogpu else []:
        find("load-in-gpu-call", message, node)

    # --- the interface, behind the main guard --------------------------------
    def builds_interface(call):
        parts = dotted(call.func).split(".")
        component = (parts[0] == "gr" and len(parts) == 2 and parts[1][:1].isupper()
                     and parts[1] not in GRADIO_NON_COMPONENTS)
        return component or parts[-1] in ("build_endpoint", "launch")

    interface = next(((node, call) for node in statements_outside(tree.body)
                      for call in calls_in(node) if builds_interface(call)), None)
    if interface is not None:
        node, call = interface
        find("interface-outside-main",
             f"{dotted(call.func)}() runs at module scope, so the worker, which imports "
             f"this file, builds it too. Move it under `if __name__ == \"__main__\":`", node)

    # --- the model card, ahead of loading and process_fn ---------------------
    def card_index(body):
        for index, node in enumerate(body):
            if isinstance(node, (ast.Assign, ast.AnnAssign)) and node.value is not None \
                    and isinstance(node.value, ast.Call) \
                    and dotted(node.value.func).endswith("ModelCard"):
                return index
        return None

    card = card_index(tree.body)
    guarded = any(card_index(node.body) is not None
                  for node in tree.body if is_name_guard(node, False))
    if card is None and guarded:
        find("card-order", "the ModelCard is defined inside the `__main__` block rather "
                           "than at module scope, ahead of the model loading")
    elif card is not None:
        process_index = next((index for index, node in enumerate(tree.body)
                              if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
                              and node.name == process_name), None)
        worker_index = next((index for index, node in enumerate(tree.body)
                             if is_name_guard(node, True)), None)
        later_than = [what for what, index in (("process_fn", process_index),
                                               ("the model loading", worker_index))
                      if index is not None and index < card]
        if later_than:
            find("card-order", f"the ModelCard comes after {' and '.join(later_than)}, "
                               f"where the guide puts it before both", tree.body[card])

    # --- show_error, the queue, and messages HARP never sees -----------------
    launches = [call for call in ast.walk(tree)
                if isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute)
                and call.func.attr == "launch"]
    for call in launches:
        if any(kw.arg is None for kw in call.keywords):
            continue
        shows = any(kw.arg == "show_error" and isinstance(kw.value, ast.Constant)
                    and kw.value.value is True for kw in call.keywords)
        if not shows:
            find("show-error", "launch() is not given show_error=True, so HARP receives "
                               "no reason when a job fails with anything but a gr.Error",
                 call)

    if launches and not any(isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute)
                            and call.func.attr == "queue" for call in ast.walk(tree)):
        find("queue", "demo.queue() is never called, so HARP cannot cancel a running job",
             launches[0])

    for node in ast.walk(tree):
        if isinstance(node, ast.Attribute) and dotted(node) in ("gr.Warning", "gr.Info"):
            find("info-warning", f"{dotted(node)} never reaches HARP. Raise gr.Error for "
                                 f"what a user has to know", node)
            break

    # --- inputs as they arrived ----------------------------------------------
    for call in input_component_calls(tree):
        if dotted(call.func) == "gr.Audio" and any(kw.arg == "format" for kw in call.keywords):
            find("input-format", "an input gr.Audio sets format, so Gradio converts every "
                                 "upload before process_fn sees it, which drops its "
                                 "container and fails inside Gradio on a file it cannot "
                                 "decode", call)

    return findings


def lint_containers(text: str, file: str) -> list:
    """
    Find AIFF listed among the containers a Space writes back.

    Gradio converts an AIFF output to 16-bit WAV, since browsers cannot play
    it, and soundfile cannot tell the format from `.aif`, so `save_audio` and
    `sf.write` both fail on an output named with it, inside the GPU call on
    ZeroGPU. The guide keeps WAV and FLAC only.

    Args:
        text (str): The contents of a Python file.
        file (str): Its name, for reporting.

    Returns:
        findings (list): A Finding per set that lists `.aiff` or `.aif`.
    """

    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []

    return [Finding("aiff-container", "AIFF is listed among the containers kept, but Gradio "
                                      "converts an AIFF output to 16-bit WAV, and soundfile cannot "
                                      "write `.aif` at all. Keep WAV and FLAC only",
                    node.lineno, file)
            for node in ast.walk(tree) if isinstance(node, ast.Set)
            and any(isinstance(element, ast.Constant) and element.value in (".aiff", ".aif")
                    for element in node.elts)]


def lint_revisions(text: str, file: str, listing: list) -> list:
    """
    Flag weights fetched from a Hub repository without a pinned revision.

    Only a repository named by a string literal is checked, since a name held
    in a variable may carry its revision beside it. A literal that names a
    directory of the Space itself is a local path rather than a repository.

    Args:
        text (str): The contents of a Python file.
        file (str): Its name, for reporting.
        listing (list): Every path in the Space, to tell a local directory
            from a repository.

    Returns:
        findings (list): Finding objects.
    """

    try:
        tree = ast.parse(text)
    except SyntaxError:
        return []

    directories = {str(Path(path).parent) for path in listing}
    findings = []

    for call in ast.walk(tree):
        if not isinstance(call, ast.Call):
            continue
        name = dotted(call.func).split(".")[-1]
        if name not in ("hf_hub_download", "snapshot_download", "from_pretrained"):
            continue
        keywords = {kw.arg: kw.value for kw in call.keywords}
        repository = keywords.get("repo_id", call.args[0] if call.args else None)
        if not (isinstance(repository, ast.Constant) and isinstance(repository.value, str)):
            continue
        repo = repository.value
        if not re.fullmatch(r"[\w.-]+/[\w.-]+", repo) or repo in directories:
            continue
        if "revision" not in keywords and None not in keywords:
            findings.append(Finding("unpinned-revision",
                                    f"{name}('{repo}') pins no revision, so the Space "
                                    f"changes whenever the repository does",
                                    call.lineno, file))

    return findings
