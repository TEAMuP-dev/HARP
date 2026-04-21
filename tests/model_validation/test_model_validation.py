from __future__ import annotations

import pytest

from tests.model_validation.helpers import (
    build_process_inputs,
    cleanup_outputs,
    import_app_module,
    iter_enabled_validation_models,
    load_registry,
    missing_python_modules,
    validate_outputs,
)


ENABLED_MODELS = [
    model
    for model in iter_enabled_validation_models()
    if model.get("validation", {}).get("mode") == "local_pyharp_example"
]


def test_model_registry_has_unique_ids_and_featured_paths() -> None:
    registry = load_registry()
    models = registry.get("models", [])

    ids = [model["id"] for model in models]
    assert len(ids) == len(set(ids)), "Model registry ids must be unique."

    featured_paths = [
        model["path"]
        for model in models
        if model.get("featured") and model.get("path")
    ]
    assert featured_paths, "Registry should contain at least one featured model path."
    assert len(featured_paths) == len(set(featured_paths)), (
        "Featured model paths in the registry must be unique."
    )


@pytest.mark.parametrize("model_entry", ENABLED_MODELS, ids=[model["id"] for model in ENABLED_MODELS])
def test_enabled_model_validation(model_entry: dict) -> None:
    missing_modules = missing_python_modules(model_entry)

    if missing_modules:
        pytest.skip(f"Missing optional Python dependencies: {', '.join(missing_modules)}")

    mode = model_entry["validation"]["mode"]
    files_to_cleanup = []

    try:
        if mode == "local_pyharp_example":
            module = import_app_module(model_entry)

            assert hasattr(module, "model_card")
            assert getattr(module.model_card, "name", "")
            assert hasattr(module, "process_fn")
            assert callable(module.process_fn)

            if hasattr(module, "input_components"):
                assert len(module.input_components) == len(model_entry["validation"].get("inputs", []))

            result = module.process_fn(*build_process_inputs(model_entry))
            files_to_cleanup = validate_outputs(result, model_entry)
        else:
            raise AssertionError(f"Unsupported validation mode: {mode}")
    finally:
        cleanup_outputs(files_to_cleanup)
