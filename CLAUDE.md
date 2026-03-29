# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ACPRE TUI is a C++/FTXUI terminal application for staged software workflow demos. The UI is in Chinese and guides the user through either new-feature work or bug-fix work, now backed by the actual current-directory project context instead of purely simulated AI stage output.

## Common Commands

```bash
make              # Build vendored FTXUI into ./libftxui/ and then build ./acpre_tui
make clean        # Remove local object files and the acpre_tui binary
./acpre_tui       # Run the TUI
```

## Build Notes

- The project uses a single top-level `Makefile`; there is no separate lint or test target in this repository.
- First build extracts `FTXUI-6.1.9.tar.gz`, builds FTXUI with CMake, and installs it into `./libftxui/`.
- Application sources are compiled with `g++` and `-std=c++17`.
- FTXUI link order is important: `-lftxui-component` must come before `-lftxui-dom` and `-lftxui-screen`.

## Architecture

### Code Layout

- `main.cpp`: minimal entrypoint that delegates to `AcpreTui::run()`.
- `acpre.h`: shared types plus the `AcpreTui` singleton and logging macros.
- `acpre.cpp`: almost all application logic, including data models, state management, screen construction, event handling, directory inspection, build-command execution, and stage processing.

### Runtime Model

The app is effectively a small state machine around `AppState`:

- `AppScreen` switches between project-type selection, project-name input, and the main workspace.
- `WorkspaceView` switches the center panel between the normal stage view, iteration history, and global validation.
- `AppState` owns the current `Project`, screen/view selection, processing progress, and a listener list used to trigger rerenders.

### Project / Workflow Model

`Project` is initialized differently depending on `ProjectType`:

- `NEW_FEATURE`: stages are `需求讨论 -> 方案设计 -> 任务迭代`, and the project also gets a task tree shown in the left/right panels during the iteration stage.
- `BUG_FIX`: stages are `问题定义 -> 修复方案 -> 执行迭代`, and the right panel shows bug metadata such as severity, location, and risk.
- `REFACTOR` exists in the data model, but the current type-selection UI only exposes new-feature and bug-fix flows.

Each `ProjectStage` tracks:

- coarse status (`pending` / `running` / `done`)
- a stage goal and a list of concrete work items
- iteration count and max-iteration threshold
- acceptance tests
- conversation history and iteration history
- whether human intervention is required after too many retries

### UI Composition

The main workspace is a three-column FTXUI layout built in `CreateMainWorkspace()`:

- left: project header, stage navigation, and the feature-task tree when applicable
- center: current stage content or alternate views for iteration history / global validation
- right: contextual summaries, per-type metadata, and shortcut help

`run()` lazily creates the workspace component only after a project exists, then renders the appropriate screen based on `AppState::screen`.

### Current Execution Model

Stage execution is no longer fake demo output. Instead:

- `Project` captures the current working directory and initializes stage goals/work items around real project inspection and build validation.
- `AppState::start_current_stage()` starts `processing_thread` for a real stage pass.
- Analysis/design stages inspect the current directory, summarize files/extensions, and run `make -n` for preview output.
- Execution stages now attempt to invoke `claude -p` inside the current working directory so the TUI can trigger real code modification tasks.
- If the `claude` command is unavailable, execution stages fall back to local `make` validation and surface that status in the UI.
- Stage output includes executor availability, executor summary/failure lines, and build results.
- Acceptance tests are left for manual confirmation instead of being auto-passed.
- A detached thread posts `ftxui::Event::Custom` every 100ms so the UI keeps refreshing while background work runs.

### Input / Event Handling

Global exit is handled in `run()` with `Ctrl+Q`. Screen-specific input is routed to the active component. In the main workspace, the important shortcuts are:

- `Space`: execute the current stage using current-directory inspection/build behavior
- `T`: manually confirm acceptance tests one by one
- `C`: complete the current stage after validation
- `R`: retry after human-intervention state
- `I`: iteration history view
- `V`: global validation view
- `Esc`: return from alternate views to the normal workspace

### Current Behavior Notes

- The UI now surfaces the current working directory on the project setup screen.
- Stage output should describe the real repository state rather than invented requirement/design/implementation prose.
- Analysis/design stages summarize the current repo and show `make -n` preview information.
- Execution stages try to call Claude Code first, then run `make`, and show both executor output and build result summaries.
- The bug-info and task panels now describe the real workflow conversion work instead of unrelated placeholder examples.

### Logging

`AcpreTui` is a singleton mainly used for thread-safe logging. The `A_LOG*` macros write directly to stdout under a mutex and cap each log line at `APP_LOG_MAX` bytes.
