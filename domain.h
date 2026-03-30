#pragma once

#include <string>
#include <vector>
#include <ftxui/dom/elements.hpp>

enum class ProjectType {
    NEW_FEATURE,
    BUG_FIX,
    REFACTOR,
};

std::string ProjectTypeToString(ProjectType type);

enum class TaskStatus {
    PENDING,
    IN_PROGRESS,
    REVIEW,
    COMPLETED,
    BLOCKED,
};

enum class WorkspaceView {
    NORMAL,
    ITERATION_HISTORY,
    GLOBAL_VALIDATION,
};

enum class StageState {
    PENDING,
    READY,
    RUNNING,
    REVIEW,
    BLOCKED,
    DONE,
};

enum class ActionState {
    IDLE,
    PROCESSING,
    SUCCESS,
    FAILURE,
    PARTIAL,
};

enum class FocusRegion {
    PROJECT_TREE,
    WORKBENCH,
    INFO_PANEL,
    STATUS_BAR,
    OVERLAY,
};

enum class OverlayKind {
    NONE,
    ACCEPTANCE_CONFIRM,
    BLOCKING_CONFIRM,
    RECOVERY_ACTIONS,
};

enum class CompactMode {
    FULL,
    MEDIUM,
    NARROW_TABS,
    MINIMAL_OVERLAY,
};

struct UiSessionState {
    FocusRegion active_region = FocusRegion::WORKBENCH;
    FocusRegion last_focus_region = FocusRegion::WORKBENCH;
    WorkspaceView workspace_view = WorkspaceView::NORMAL;
    OverlayKind overlay_kind = OverlayKind::NONE;
    CompactMode compact_mode = CompactMode::FULL;
    bool input_captured_by_overlay = false;
    int project_tree_index = 0;
    float scroll_y = 0.0f;
    bool input_mode = false;
    std::string user_input;
};

struct AcceptanceTest {
    std::string description;
    bool passed;
    std::string notes;
};

struct IterationRecord {
    int iteration_number;
    std::string action;
    std::string test_result;
    std::string reason;
    std::string status_icon;
    std::string summary;
    std::string next_step;
};

struct StageBuildSummary {
    int return_code = -1;
    std::string headline;
    std::vector<std::string> details;
    std::vector<std::string> failure_lines;
    std::vector<std::string> related_files;
    bool command_ran = false;
    bool success = false;
};

struct StageExecutorSummary {
    bool available = false;
    bool attempted = false;
    int return_code = -1;
    std::string command;
    std::string prompt;
    std::string headline;
    std::vector<std::string> details;
    std::vector<std::string> failure_lines;
    bool success = false;
};

struct StageExecutionSummary {
    std::vector<std::string> project_observations;
    std::vector<std::string> key_files;
    std::vector<std::string> implementation_notes;
    std::vector<std::string> validation_points;
    std::vector<std::string> next_actions;
    std::string risk_summary;
    std::string result_summary;
    StageExecutorSummary executor_summary;
    StageBuildSummary build_summary;
};

struct Task {
    std::string name;
    TaskStatus status;
    int iteration_count;
    std::vector<IterationRecord> iteration_history;
};

struct BugInfo {
    std::string description;
    std::string severity;
    std::string location;
    std::string root_cause;
    std::vector<std::string> fix_steps;
    int files_modified;
    int lines_added;
    std::string risk_level;
};

struct ProjectStage {
    std::string name;
    StageState stage_state = StageState::PENDING;
    ActionState action_state = ActionState::IDLE;
    std::string output;
    std::string goal;
    std::vector<std::string> work_items;
    StageExecutionSummary execution_summary;
    int iteration_count = 0;
    int max_iterations = 5;
    std::vector<AcceptanceTest> acceptance_tests;
    std::vector<std::string> conversation_history;
    std::vector<IterationRecord> iteration_history;
};

struct Project {
    std::string name;
    ProjectType type;
    std::string working_directory;
    int current_stage_index = 0;
    std::vector<ProjectStage> stages;
    std::vector<Task> tasks;
    BugInfo bug_info;

    Project(std::string n, ProjectType t);
};

std::string StageStateIcon(StageState state);
ftxui::Color StageStateColor(StageState state);
std::string ActionStateLabel(ActionState state);
std::string ActionStateIcon(ActionState state);
std::string ReviewActionLabel(ActionState state, bool can_complete);
std::string ReviewStatusColorLabel(ActionState state, bool can_complete);
ftxui::Color ActionStateColor(ActionState state);
FocusRegion NextFocusRegion(FocusRegion region);
FocusRegion PreviousFocusRegion(FocusRegion region);
std::vector<AcceptanceTest> generate_default_acceptance_tests(const std::string& stage_name);
bool is_analysis_stage(const std::string& stage_name);
bool is_design_stage(const std::string& stage_name);
bool is_execution_stage(const std::string& stage_name);
