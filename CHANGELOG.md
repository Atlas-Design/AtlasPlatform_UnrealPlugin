# Changelog

## 1.1.0

- Migrated Atlas Platform integration to **API v0.2**: workspace-scoped upload/download URLs, multipart file upload, and `Authorization: Bearer` workspace API keys.
- Added **Authentication** settings (`Workspace Api Key`, `ATLAS_API_KEY` via environment) with pre-flight validation before workflow execution.
- Applied configurable **HTTP request timeouts** to all platform calls.
- Per-run job archives under `{OutputFolder}/{WorkflowName}/{RunId}/` (`job.json`, `inputs/`, `outputs/`).
- **Workflow Editor** is the default editor entry point; Batch Editor and standalone Job History are hidden from menus until a future release (`GAtlasExposeBatchAndJobHistoryUI` in `AtlasWorkflowEditor.cpp`).
- Updated `README.md` and `Source/ARCHITECTURE.md` for auth, v0.2 endpoints, and current defaults (5s poll interval, 900s max execution time).

## 1.0.0

- Initial early-access release: workflow assets, editor UI, Blueprint async execution, upload cache, and job history.
