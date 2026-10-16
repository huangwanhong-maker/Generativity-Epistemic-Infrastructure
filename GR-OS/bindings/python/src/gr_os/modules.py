"""Descriptive package discovery; explicit activation executes trusted Python code.

This module is not a dependency solver, native loader, sandbox or authority system.
Importing it does not load a GR native library. Categories are open descriptive
labels; semantic registrations continue to belong to the Layer 3 Library.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from importlib import metadata
import json
from pathlib import Path
import re
from typing import Any, Mapping

GROUP = "gr_os.modules"
FORMAT = "gr.module-package/1"
MAX_MANIFEST_BYTES = 64 * 1024
_ID = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,199}\Z")
_VERSION = re.compile(r"(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\Z")


class ModuleError(ValueError):
    """Malformed, ambiguous or incompatible package declaration."""


def _text(value: Any, field_name: str, pattern=None) -> str:
    if not isinstance(value, str) or not value or len(value) > 8192:
        raise ModuleError(f"{field_name}: expected a bounded nonempty string")
    if any(ord(c) < 32 for c in value) or (pattern and not pattern.fullmatch(value)):
        raise ModuleError(f"{field_name}: invalid string")
    return value


def _strings(value: Any, field_name: str, *, allow_empty: bool = True) -> None:
    if not isinstance(value, list) or len(value) > 256 or (not value and not allow_empty):
        raise ModuleError(f"{field_name}: expected a bounded list")
    for item in value:
        _text(item, field_name)
    if len(set(value)) != len(value):
        raise ModuleError(f"{field_name}: duplicate declaration")


def _object_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ModuleError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _invalid_constant(value: str):
    raise ModuleError(f"non-finite JSON constant: {value}")


def _decode(text: str) -> dict:
    try:
        value = json.loads(text, object_pairs_hook=_object_pairs,
                           parse_constant=_invalid_constant)
    except (ValueError, RecursionError) as error:
        raise ModuleError(f"invalid manifest JSON: {error}") from error
    if not isinstance(value, dict):
        raise ModuleError("manifest must be an object")
    return value


@dataclass(frozen=True)
class Manifest:
    """Immutable validated declaration, not a certificate of implementation behavior."""

    id: str
    version: str
    _encoded: str = field(repr=False)

    def describe(self) -> dict:
        """Return an independent mutable copy of the complete declaration."""
        return json.loads(self._encoded)

    def check_dependencies(self, available: Mapping[str, str]) -> None:
        """Check exact declared versions in an explicit inventory; never install/load.

        Components are CMake build requirements and are not established by this
        version-only check. This does not establish ABI or actual runtime linkage.
        """
        for requirement in self.describe()["requires"]:
            wanted = requirement["version"]
            found = available.get(requirement["id"])
            if found != wanted:
                raise ModuleError(
                    f"{self.id} requires {requirement['id']}=={wanted}; "
                    f"available: {found if found is not None else 'missing'}")


def validate_manifest(value: Mapping[str, Any]) -> Manifest:
    """Validate format-1 declarations and detach caller-owned nested data.

    Unknown optional fields are preserved. Nonempty required_extensions fail
    closed because this reader implements no mandatory extension semantics.
    Format 1 intentionally uses exact versions; pip and CMake handle resolution.
    """
    if not isinstance(value, Mapping):
        raise ModuleError("manifest must be a mapping")
    try:
        encoded = json.dumps(dict(value), sort_keys=True, ensure_ascii=False,
                             separators=(",", ":"), allow_nan=False)
        if len(encoded.encode("utf-8")) > MAX_MANIFEST_BYTES:
            raise ModuleError("manifest exceeds 64 KiB")
    except (TypeError, ValueError, RecursionError, UnicodeError) as error:
        raise ModuleError(f"manifest is not bounded JSON: {error}") from error
    document = _decode(encoded)
    required = {"format", "id", "version", "categories", "requires", "exports",
                "effects", "trust", "compatibility"}
    missing = required - document.keys()
    if missing:
        raise ModuleError(f"missing manifest fields: {', '.join(sorted(missing))}")
    if document["format"] != FORMAT:
        raise ModuleError(f"unsupported manifest format: {document['format']!r}")
    identity = _text(document["id"], "id", _ID)
    version = _text(document["version"], "version", _VERSION)
    _strings(document["categories"], "categories", allow_empty=False)
    _strings(document["effects"], "effects", allow_empty=False)
    unsupported_effects = set(document["effects"]) - {
        "pure", "read-only", "history-write", "external"}
    if unsupported_effects:
        raise ModuleError(f"unsupported effect declarations: {sorted(unsupported_effects)}")
    _text(document["trust"], "trust")
    _text(document["compatibility"], "compatibility")
    _strings(document.get("required_extensions", []), "required_extensions")
    if document.get("required_extensions"):
        raise ModuleError("unsupported required manifest extensions")
    exports = document["exports"]
    if not isinstance(exports, dict) or len(exports) > 32:
        raise ModuleError("exports must be a bounded object")
    for kind, identities in exports.items():
        _text(kind, "export category", _ID)
        _strings(identities, f"exports.{kind}")
    dependencies = document["requires"]
    if not isinstance(dependencies, list) or len(dependencies) > 128:
        raise ModuleError("requires must be a bounded list")
    seen = set()
    for dependency in dependencies:
        if not isinstance(dependency, dict) or not {"id", "version"} <= dependency.keys():
            raise ModuleError("dependency requires id and version")
        if set(dependency) - {"id", "version", "components"}:
            raise ModuleError("unsupported dependency fields")
        dep_id = _text(dependency["id"], "dependency id", _ID)
        _text(dependency["version"], "dependency version", _VERSION)
        _strings(dependency.get("components", []), "dependency components")
        if dep_id == identity or dep_id in seen:
            raise ModuleError(f"duplicate or self dependency: {dep_id}")
        seen.add(dep_id)
    return Manifest(identity, version, encoded)


def read_manifest(path: str | Path) -> Manifest:
    """Read explicitly selected JSON metadata without importing package code."""
    with Path(path).open("rb") as stream:
        raw = stream.read(MAX_MANIFEST_BYTES + 1)
    if len(raw) > MAX_MANIFEST_BYTES:
        raise ModuleError("manifest exceeds 64 KiB")
    try:
        return validate_manifest(_decode(raw.decode("utf-8")))
    except UnicodeError as error:
        raise ModuleError("manifest must be UTF-8") from error


@dataclass(frozen=True)
class ModuleEntry:
    id: str
    distribution: str
    version: str
    reference: str
    _entry_point: Any = field(repr=False, compare=False)

    def activate(self, *, available: Mapping[str, str] | None = None) -> Manifest:
        """Import and call the selected trusted manifest factory explicitly.

        Code executes BEFORE its returned declaration can be validated. Inspect
        installed metadata/source and choose trusted packages before activation.
        This does not register providers, allocate a runtime or authorize effects.
        """
        try:
            factory = self._entry_point.load()
            if not callable(factory):
                raise ModuleError("entry point must be a no-argument manifest factory")
            manifest = validate_manifest(factory())
        except ModuleError:
            raise
        except Exception as error:
            raise ModuleError(f"activation failed for {self.id}: {error}") from error
        if manifest.id != self.id or manifest.version != self.version:
            raise ModuleError(f"entry point/distribution does not match manifest: {self.id}")
        if available is not None:
            manifest.check_dependencies(available)
        return manifest


def discover() -> tuple[ModuleEntry, ...]:
    """Enumerate installed gr_os.modules metadata without loading entry points.

    Duplicate advertised IDs are ambiguous and rejected, never resolved by path
    order. Import machinery/metadata providers in this process are trusted.
    """
    result = {}
    for point in metadata.entry_points(group=GROUP):
        identity = _text(point.name, "entry point id", _ID)
        if identity in result:
            raise ModuleError(f"duplicate installed module id: {identity}")
        distribution = point.dist
        if distribution is None:
            raise ModuleError(f"entry point has no distribution: {identity}")
        name = _text(distribution.metadata.get("Name"), "distribution name", _ID)
        version = _text(distribution.version, "distribution version", _VERSION)
        reference = _text(point.value, "entry point reference")
        try:
            module, attribute, extras = point.module, point.attr, point.extras
        except (AttributeError, ValueError) as error:
            raise ModuleError(f"malformed entry point reference for {identity}: {reference}") from error
        parts = module.split(".") + (attribute.split(".") if attribute else [])
        if not all(part.isidentifier() for part in parts):
            raise ModuleError(f"malformed entry point reference for {identity}: {reference}")
        if extras:
            raise ModuleError("entry point extras are unsupported; use package optional dependencies")
        result[identity] = ModuleEntry(identity, name, version, reference, point)
    return tuple(result[key] for key in sorted(result))
