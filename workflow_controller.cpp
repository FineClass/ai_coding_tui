#include "workflow_controller.h"

namespace workflow {

int CountPassedAcceptanceTests(const ProjectStage& stage) {
    int passed_tests = 0;
    for (const auto& test : stage.acceptance_tests) {
        if (test.passed) {
            passed_tests++;
        }
    }
    return passed_tests;
}

bool CanCompleteStage(const ProjectStage& stage) {
    return !stage.acceptance_tests.empty() && CountPassedAcceptanceTests(stage) == static_cast<int>(stage.acceptance_tests.size());
}

void SyncDerivedProjectState(Project& project) {
    if (project.current_stage_index >= static_cast<int>(project.stages.size())) {
        return;
    }

    auto& stage = project.stages[project.current_stage_index];
    const auto& summary = stage.execution_summary;
    const bool stage_done = stage.stage_state == StageState::DONE;
    const bool stage_ready = stage.stage_state == StageState::READY;
    const bool stage_running = stage.stage_state == StageState::RUNNING;
    const bool stage_review = stage.stage_state == StageState::REVIEW;
    const bool stage_blocked = stage.stage_state == StageState::BLOCKED;

    if (project.type == ProjectType::NEW_FEATURE) {
        auto set_task_status = [&](size_t index, TaskStatus status) {
            if (index < project.tasks.size()) {
                project.tasks[index].status = status;
            }
        };

        for (auto& task : project.tasks) {
            task.status = TaskStatus::PENDING;
        }

        if (project.current_stage_index == 0) {
            TaskStatus discovery_status = stage_done ? TaskStatus::COMPLETED :
                                        stage_blocked ? TaskStatus::BLOCKED :
                                        stage_review ? TaskStatus::REVIEW :
                                        (stage_ready || stage_running) ? TaskStatus::IN_PROGRESS : TaskStatus::PENDING;
            set_task_status(0, discovery_status);
            set_task_status(1, discovery_status);
        } else if (project.current_stage_index == 1) {
            set_task_status(0, TaskStatus::COMPLETED);
            set_task_status(1, TaskStatus::COMPLETED);
        } else {
            set_task_status(0, TaskStatus::COMPLETED);
            set_task_status(1, TaskStatus::COMPLETED);

            TaskStatus implementation_status = stage_done ? TaskStatus::COMPLETED :
                                               stage_blocked ? TaskStatus::BLOCKED :
                                               (stage_review && stage.action_state == ActionState::FAILURE) ? TaskStatus::IN_PROGRESS :
                                               stage_review ? TaskStatus::REVIEW :
                                               stage_running ? TaskStatus::IN_PROGRESS : TaskStatus::PENDING;
            set_task_status(2, implementation_status);

            TaskStatus build_validation_status = stage_done ? TaskStatus::COMPLETED :
                                                 stage_blocked ? TaskStatus::BLOCKED :
                                                 (stage_review && summary.build_summary.success && stage.action_state == ActionState::SUCCESS) ? TaskStatus::COMPLETED :
                                                 (stage_review && summary.build_summary.success) ? TaskStatus::REVIEW :
                                                 (stage_review && stage.action_state == ActionState::FAILURE) ? TaskStatus::BLOCKED :
                                                 stage_running ? TaskStatus::IN_PROGRESS : TaskStatus::PENDING;
            set_task_status(3, build_validation_status);

            TaskStatus acceptance_status = stage_done ? TaskStatus::COMPLETED :
                                          stage_blocked ? TaskStatus::BLOCKED :
                                          stage_review ? TaskStatus::REVIEW : TaskStatus::PENDING;
            set_task_status(4, acceptance_status);
        }
    } else if (project.type == ProjectType::BUG_FIX) {
        project.bug_info.description = summary.result_summary.empty() ? stage.goal : summary.result_summary;
        project.bug_info.location = summary.key_files.empty() ? "待分析" : summary.key_files.front();
        project.bug_info.root_cause = summary.risk_summary.empty() ? "待补充" : summary.risk_summary;
        project.bug_info.fix_steps = summary.next_actions.empty() ? stage.work_items : summary.next_actions;
        project.bug_info.files_modified = static_cast<int>(summary.key_files.size());
        project.bug_info.lines_added = summary.build_summary.command_ran ? static_cast<int>(summary.build_summary.details.size() * 10) : 0;

        if (stage.stage_state == StageState::DONE) {
            project.bug_info.severity = "低";
            project.bug_info.risk_level = "低";
        } else if (stage.stage_state == StageState::BLOCKED ||
                   (stage.stage_state == StageState::REVIEW && stage.action_state == ActionState::FAILURE)) {
            project.bug_info.severity = "高";
            project.bug_info.risk_level = "高";
        } else if (stage.stage_state == StageState::REVIEW && stage.action_state == ActionState::PARTIAL) {
            project.bug_info.severity = "中";
            project.bug_info.risk_level = "中";
        } else if (stage.stage_state == StageState::RUNNING) {
            project.bug_info.severity = "中";
            project.bug_info.risk_level = "中";
        } else if (project.current_stage_index == 0) {
            project.bug_info.severity = "中";
            project.bug_info.risk_level = "低";
        } else {
            project.bug_info.severity = "中";
            project.bug_info.risk_level = "中";
        }
    }
}

bool PrepareCurrentStageForExecution(Project& project, bool is_processing, int& processing_progress) {
    if (project.current_stage_index >= static_cast<int>(project.stages.size()) || is_processing) {
        return false;
    }

    auto& stage = project.stages[project.current_stage_index];
    if (stage.iteration_count >= stage.max_iterations) {
        stage.stage_state = StageState::BLOCKED;
        stage.action_state = ActionState::FAILURE;
        stage.execution_summary.result_summary = "已达到最大迭代次数，需要人工介入评估。";
        stage.execution_summary.next_actions = {"检查当前阶段摘要", "确认是否需要手动调整工作项", "重试前先明确风险"};
        stage.output = "⚠️ 已达到最大迭代次数 (" + std::to_string(stage.max_iterations) + ")，需要人工介入评估";
        SyncDerivedProjectState(project);
        return false;
    }

    stage.stage_state = StageState::RUNNING;
    stage.action_state = ActionState::PROCESSING;
    stage.iteration_count++;
    processing_progress = 0;
    stage.output.clear();
    stage.conversation_history.push_back("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    stage.conversation_history.push_back("开始第 " + std::to_string(stage.iteration_count) + " 次实际执行");
    SyncDerivedProjectState(project);
    return true;
}

bool CompleteCurrentStage(Project& project, bool& is_processing) {
    if (project.current_stage_index >= static_cast<int>(project.stages.size())) {
        return false;
    }

    auto& stage = project.stages[project.current_stage_index];
    if (!CanCompleteStage(stage)) {
        stage.output += "\n\n❌ 验收项尚未全部确认，请继续检查当前目录中的实际结果并手动确认。";
        stage.execution_summary.next_actions = {"继续根据验收清单补齐确认", "查看复核视图中的未确认项", "必要时重新执行当前阶段"};
        stage.stage_state = StageState::REVIEW;
        stage.action_state = ActionState::FAILURE;
        is_processing = false;
        SyncDerivedProjectState(project);
        return false;
    }

    stage.stage_state = StageState::DONE;
    stage.action_state = ActionState::SUCCESS;
    stage.output += "\n\n✅ 当前阶段已按验收清单确认完成。";
    stage.execution_summary.result_summary = "当前阶段已按验收清单确认完成。";
    is_processing = false;
    SyncDerivedProjectState(project);

    if (project.current_stage_index < static_cast<int>(project.stages.size()) - 1) {
        project.current_stage_index++;
        auto& next_stage = project.stages[project.current_stage_index];
        next_stage.stage_state = StageState::READY;
        next_stage.action_state = ActionState::IDLE;
        SyncDerivedProjectState(project);
    }
    return true;
}

bool RetryCurrentStage(Project& project, bool& is_processing) {
    if (project.current_stage_index >= static_cast<int>(project.stages.size())) {
        return false;
    }

    auto& stage = project.stages[project.current_stage_index];
    stage.stage_state = StageState::READY;
    stage.action_state = ActionState::IDLE;
    is_processing = false;
    SyncDerivedProjectState(project);
    return true;
}

bool ConfirmNextAcceptanceTest(ProjectStage& stage) {
    if (stage.stage_state != StageState::REVIEW || stage.acceptance_tests.empty()) {
        return false;
    }

    for (auto& test : stage.acceptance_tests) {
        if (!test.passed) {
            test.passed = true;
            if (test.notes.empty()) {
                test.notes = stage.execution_summary.result_summary;
            } else {
                test.notes = "已人工确认: " + test.notes;
            }
            return true;
        }
    }
    return false;
}

}  // namespace workflow
