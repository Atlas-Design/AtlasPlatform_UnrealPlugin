# Changelog

## Unreleased

- Stabilize non-batch workflow execution, upload ownership, cancellation, output persistence, and library loading.
- Move workflow widget behavior to native C++ while retaining Designer-owned layouts; connect history filters.
- Add compact scalar and ordered-array file cards, source dialogs, validated drops, per-item replacement, multi-file browsing, and keyboard/drag reordering.
- Extend schema and file handling to additional API file families, explicit formats, Atlas asset IDs, and ordered arrays; keep unknown types unsupported.
- Preserve optional omission separately from false, zero, empty text, and empty arrays, including history and reruns.
- Add explicit Boolean labels, numeric validation, and expanded text editing.
- Show recorded inputs as read-only cards and outputs with per-item imports, Import all, restored import status, Save As, Reveal, Copy ID, Reimport, and explicit mesh insertion with Undo.
- Distinguish imported output destinations by run, parameter, and item; preserve supported source texture formats and lazy asynchronous previews.
- Add schema, array, file-selection, native-widget, and saved-widget rendering coverage. UE 5.5 editor and DirectX 12 render checks are recorded; native manual acceptance and full runtime/game validation remain outstanding.

## 1.1.0

- Migrated Atlas Platform integration to **API v0.2**: workspace-scoped upload/download URLs, multipart file upload, and `Authorization: Bearer` workspace API keys.
- Added **Authentication** settings (`Workspace Api Key`, `ATLAS_API_KEY` via environment) with pre-flight validation before workflow execution.
- Applied configurable **HTTP request timeouts** to all platform calls.
- Per-run job archives under `{OutputFolder}/{WorkflowName}/{RunId}/` (`job.json`, `inputs/`, `outputs/`).
- **Workflow Editor** is the default editor entry point; Batch Editor and standalone Job History are hidden from menus until a future release (`GAtlasExposeBatchAndJobHistoryUI` in `AtlasWorkflowEditor.cpp`).
- Updated `README.md` and `Source/ARCHITECTURE.md` for auth, v0.2 endpoints, and current defaults (5s poll interval, 900s max execution time).

## 1.0.0

- Initial early-access release: workflow assets, editor UI, Blueprint async execution, upload cache, and job history.
