#include "ui_panels.h"
#include "workflow_controller.h"

ftxui::Element RenderProjectTreePanel(const std::shared_ptr<AppState>& state) {
    state->ui_session.project_tree_index = std::max(0, std::min(state->ui_session.project_tree_index,
        (int)state->project->stages.size() - 1));
    std::vector<ftxui::Element> nav_items;

    std::string type_icon = (state->project->type == ProjectType::NEW_FEATURE) ? "🔵" : "🔴";
    nav_items.push_back(ftxui::text(type_icon + " " + state->project->name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    nav_items.push_back(ftxui::separator());
    nav_items.push_back(ftxui::text("📋 项目阶段") | ftxui::bold);
    nav_items.push_back(ftxui::emptyElement());

    for (size_t i = 0; i < state->project->stages.size(); ++i) {
        const auto& stage = state->project->stages[i];
        std::string label = StageStateIcon(stage.stage_state) + " " + stage.name;

        bool is_selected = ((int)i == state->project->current_stage_index);
        bool has_focus_cursor = ((int)i == state->ui_session.project_tree_index) &&
                                (state->ui_session.active_region == FocusRegion::PROJECT_TREE);
        auto element = ftxui::text(label);
        if (is_selected) {
            element = element | ftxui::bold | ftxui::color(ftxui::Color::Yellow) | ftxui::bgcolor(ftxui::Color::GrayDark);
        } else if (stage.stage_state == StageState::DONE) {
            element = element | ftxui::color(ftxui::Color::Green);
        } else if (stage.stage_state == StageState::RUNNING) {
            element = element | ftxui::color(ftxui::Color::Yellow);
        } else if (stage.stage_state == StageState::BLOCKED) {
            element = element | ftxui::color(ftxui::Color::Red) | ftxui::bold;
        } else if (stage.stage_state == StageState::REVIEW) {
            element = element | ftxui::color(ftxui::Color::Cyan);
        } else {
            element = element | ftxui::dim;
        }
        if (has_focus_cursor) {
            element = element | ftxui::inverted;
        }
        nav_items.push_back(element);
    }

    if (state->project->type == ProjectType::NEW_FEATURE && state->project->current_stage_index == 2) {
        nav_items.push_back(ftxui::emptyElement());
        nav_items.push_back(ftxui::separator());
        nav_items.push_back(ftxui::text("📂 任务树") | ftxui::bold);
        nav_items.push_back(ftxui::emptyElement());

        for (const auto& task : state->project->tasks) {
            std::string task_icon;
            ftxui::Color task_color;
            if (task.status == TaskStatus::COMPLETED) {
                task_icon = "✅";
                task_color = ftxui::Color::Green;
            } else if (task.status == TaskStatus::REVIEW) {
                task_icon = "🔎";
                task_color = ftxui::Color::Cyan;
            } else if (task.status == TaskStatus::BLOCKED) {
                task_icon = "⛔";
                task_color = ftxui::Color::Red;
            } else if (task.status == TaskStatus::IN_PROGRESS) {
                task_icon = "🟡";
                task_color = ftxui::Color::Yellow;
            } else {
                task_icon = "⚪";
                task_color = ftxui::Color::White;
            }
            nav_items.push_back(ftxui::text(task_icon + " " + task.name) | ftxui::color(task_color));
        }
    }

    return ftxui::vbox(nav_items) | ftxui::border | ftxui::flex |
           ((state->ui_session.active_region == FocusRegion::PROJECT_TREE)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderContextPanel(const std::shared_ptr<AppState>& state) {
    std::vector<ftxui::Element> sections;
    const auto& current_stage = state->project->stages[state->project->current_stage_index];
    const auto& summary = current_stage.execution_summary;

    int passed_tests = workflow::CountPassedAcceptanceTests(current_stage);
    int total_tests = current_stage.acceptance_tests.size();
    const bool can_complete_stage = total_tests > 0 && passed_tests == total_tests;

    auto stage_state_line = ftxui::text("阶段状态 " + ReviewStatusColorLabel(current_stage.action_state, can_complete_stage)) |
                            ftxui::color(ActionStateColor(current_stage.action_state));
    if (current_stage.stage_state != StageState::REVIEW) {
        stage_state_line = ftxui::text("阶段状态 " + StageStateIcon(current_stage.stage_state) + " " +
                                       ((current_stage.stage_state == StageState::RUNNING) ? "执行中" :
                                        (current_stage.stage_state == StageState::DONE) ? "已完成" :
                                        (current_stage.stage_state == StageState::BLOCKED) ? "已阻塞" :
                                        (current_stage.stage_state == StageState::READY) ? "可开始" : "待开始")) |
                           ftxui::color(StageStateColor(current_stage.stage_state));
    }

    if (state->project->type == ProjectType::NEW_FEATURE) {
        std::vector<ftxui::Element> feature_context;
        feature_context.push_back(ftxui::text("🧩 当前任务上下文") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        feature_context.push_back(ftxui::separator());
        feature_context.push_back(ftxui::text("阶段 " + current_stage.name) | ftxui::bold);
        feature_context.push_back(stage_state_line);
        feature_context.push_back(ftxui::text("目标 " + current_stage.goal) | ftxui::dim);

        if (!summary.result_summary.empty()) {
            feature_context.push_back(ftxui::emptyElement());
            feature_context.push_back(ftxui::text("摘要") | ftxui::bold);
            feature_context.push_back(ftxui::text(summary.result_summary));
        }

        if (state->project->current_stage_index == 2 && !state->project->tasks.empty()) {
            int completed_tasks = 0;
            std::string active_task = "等待进入任务迭代";
            for (const auto& task : state->project->tasks) {
                if (task.status == TaskStatus::COMPLETED) {
                    completed_tasks++;
                } else if (active_task == "等待进入任务迭代" &&
                           (task.status == TaskStatus::IN_PROGRESS || task.status == TaskStatus::REVIEW || task.status == TaskStatus::BLOCKED || task.status == TaskStatus::PENDING)) {
                    active_task = task.name;
                }
            }
            feature_context.push_back(ftxui::emptyElement());
            feature_context.push_back(ftxui::text("当前任务 " + active_task));
            feature_context.push_back(ftxui::text("任务进度 " + std::to_string(completed_tasks) + "/" +
                                                 std::to_string(state->project->tasks.size())) |
                                      ftxui::color(completed_tasks == (int)state->project->tasks.size() ?
                                                   ftxui::Color::Green : ftxui::Color::Yellow));
        }

        if (!summary.risk_summary.empty()) {
            feature_context.push_back(ftxui::emptyElement());
            feature_context.push_back(ftxui::text("风险提示") | ftxui::bold);
            feature_context.push_back(ftxui::text(summary.risk_summary));
        }

        sections.push_back(ftxui::vbox(feature_context) | ftxui::border);
        sections.push_back(ftxui::emptyElement());
    } else if (state->project->type == ProjectType::BUG_FIX) {
        std::vector<ftxui::Element> bug_context;
        bug_context.push_back(ftxui::text("🐞 问题上下文") | ftxui::bold | ftxui::color(ftxui::Color::Red));
        bug_context.push_back(ftxui::separator());
        bug_context.push_back(ftxui::text("阶段 " + current_stage.name) | ftxui::bold);
        bug_context.push_back(stage_state_line);
        bug_context.push_back(ftxui::text("问题 " + state->project->bug_info.description));
        bug_context.push_back(ftxui::text("位置 " + state->project->bug_info.location));
        if (!state->project->bug_info.root_cause.empty()) {
            bug_context.push_back(ftxui::text("根因/风险 " + state->project->bug_info.root_cause));
        }
        bug_context.push_back(ftxui::text("严重级别 " + state->project->bug_info.severity));
        bug_context.push_back(ftxui::text("风险等级 " + state->project->bug_info.risk_level));
        if (state->project->current_stage_index >= 1) {
            bug_context.push_back(ftxui::text("修改文件 " + std::to_string(state->project->bug_info.files_modified)));
            bug_context.push_back(ftxui::text("变更估计 " + std::to_string(state->project->bug_info.lines_added) + " 行"));
        }
        sections.push_back(ftxui::vbox(bug_context) | ftxui::border);
        sections.push_back(ftxui::emptyElement());
    }

    std::vector<ftxui::Element> metrics;
    metrics.push_back(ftxui::text("📌 关键指标") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    metrics.push_back(ftxui::separator());
    if (current_stage.iteration_count > 0) {
        auto iter_line = ftxui::text("迭代 " + std::to_string(current_stage.iteration_count) + "/" +
                                     std::to_string(current_stage.max_iterations));
        if (current_stage.iteration_count >= current_stage.max_iterations) {
            iter_line = iter_line | ftxui::color(ftxui::Color::Red);
        } else if (current_stage.iteration_count >= 3) {
            iter_line = iter_line | ftxui::color(ftxui::Color::Yellow);
        }
        metrics.push_back(iter_line);
    }
    if (total_tests > 0) {
        auto test_line = ftxui::text("验收 " + std::to_string(passed_tests) + "/" + std::to_string(total_tests));
        if (passed_tests == total_tests) {
            test_line = test_line | ftxui::color(ftxui::Color::Green);
        } else if (passed_tests > 0) {
            test_line = test_line | ftxui::color(ftxui::Color::Yellow);
        }
        metrics.push_back(test_line);
    }
    if (summary.build_summary.command_ran) {
        metrics.push_back(ftxui::text("构建 " + std::string(summary.build_summary.success ? "通过" : "失败")) |
                          ftxui::color(summary.build_summary.success ? ftxui::Color::Green : ftxui::Color::Red));
    }
    if (summary.executor_summary.attempted || summary.executor_summary.available) {
        std::string executor_status = !summary.executor_summary.available ? "不可用" :
                                      summary.executor_summary.success ? "已执行" :
                                      summary.executor_summary.attempted ? "执行失败" : "待执行";
        auto executor_line = ftxui::text("Claude " + executor_status);
        if (!summary.executor_summary.available) {
            executor_line = executor_line | ftxui::dim;
        } else if (summary.executor_summary.success) {
            executor_line = executor_line | ftxui::color(ftxui::Color::Green);
        } else if (summary.executor_summary.attempted) {
            executor_line = executor_line | ftxui::color(ftxui::Color::Yellow);
        }
        metrics.push_back(executor_line);
    }
    sections.push_back(ftxui::vbox(metrics) | ftxui::border);
    sections.push_back(ftxui::emptyElement());

    std::vector<ftxui::Element> actions;
    actions.push_back(ftxui::text("⚡ 快捷操作") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    actions.push_back(ftxui::separator());
    if (current_stage.stage_state == StageState::BLOCKED) {
        actions.push_back(ftxui::text("[R] 重置并重试") | ftxui::color(ftxui::Color::Red));
        actions.push_back(ftxui::text("[Enter/Space] 查看恢复建议") | ftxui::dim);
    } else if (current_stage.stage_state == StageState::RUNNING) {
        actions.push_back(ftxui::text("[I] 查看局部迭代详情") | ftxui::dim);
        actions.push_back(ftxui::text("[V] 查看全局复核") | ftxui::dim);
    } else if (current_stage.stage_state == StageState::REVIEW) {
        if (passed_tests < total_tests) {
            actions.push_back(ftxui::text("[T] 继续确认下一项验收") | ftxui::color(ftxui::Color::Yellow));
        }
        if (can_complete_stage) {
            actions.push_back(ftxui::text("[C] 完成当前阶段") | ftxui::color(ftxui::Color::Green));
        }
        actions.push_back(ftxui::text("[I] 局部迭代详情 · [V] 全局复核") | ftxui::dim);
    } else if (current_stage.stage_state == StageState::DONE) {
        actions.push_back(ftxui::text("[Space] 进入下一阶段") | ftxui::color(ftxui::Color::Green));
        actions.push_back(ftxui::text("[I] 局部迭代详情 · [V] 全局复核") | ftxui::dim);
    } else {
        actions.push_back(ftxui::text("[Space] 启动当前阶段") | ftxui::color(ftxui::Color::Green));
        actions.push_back(ftxui::text("[I] 局部迭代详情 · [V] 全局复核") | ftxui::dim);
    }
    if (!summary.next_actions.empty()) {
        actions.push_back(ftxui::emptyElement());
        actions.push_back(ftxui::text("下一步建议") | ftxui::bold);
        for (size_t i = 0; i < summary.next_actions.size() && i < 3; ++i) {
            actions.push_back(ftxui::text("- " + summary.next_actions[i]));
        }
    }
    sections.push_back(ftxui::vbox(actions) | ftxui::border);
    sections.push_back(ftxui::emptyElement());

    std::vector<ftxui::Element> shortcuts;
    shortcuts.push_back(ftxui::text("⌨️ 当前视图快捷键") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    shortcuts.push_back(ftxui::separator());
    shortcuts.push_back(ftxui::text("[Tab] 切换焦点") | ftxui::dim);
    shortcuts.push_back(ftxui::text("[I] 局部迭代详情") | ftxui::dim);
    shortcuts.push_back(ftxui::text("[V] 全局复核") | ftxui::dim);
    if (current_stage.stage_state == StageState::REVIEW) {
        shortcuts.push_back(ftxui::text("[T] 验收确认") | ftxui::dim);
    }
    if (current_stage.stage_state == StageState::BLOCKED) {
        shortcuts.push_back(ftxui::text("[R] 重试阶段") | ftxui::dim);
    }
    if (can_complete_stage && current_stage.stage_state == StageState::REVIEW) {
        shortcuts.push_back(ftxui::text("[C] 完成阶段") | ftxui::dim);
    }
    shortcuts.push_back(ftxui::text("[Ctrl+Q] 退出") | ftxui::dim);
    sections.push_back(ftxui::vbox(shortcuts) | ftxui::border);

    return ftxui::vbox(sections) | ftxui::flex |
           ((state->ui_session.active_region == FocusRegion::INFO_PANEL)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderStatusBar(const std::shared_ptr<AppState>& state) {
    std::vector<ftxui::Element> status_items;

    const auto& current_stage = state->project->stages[state->project->current_stage_index];
    int passed_tests = workflow::CountPassedAcceptanceTests(current_stage);
    int total_tests = current_stage.acceptance_tests.size();
    const bool can_complete_stage = total_tests > 0 && passed_tests == total_tests;

    std::string type_label = (state->project->type == ProjectType::NEW_FEATURE) ? "新功能" : "缺陷修复";
    auto type_color = (state->project->type == ProjectType::NEW_FEATURE) ? ftxui::Color::Blue : ftxui::Color::Red;
    status_items.push_back(ftxui::text("[" + type_label + "流程]") | ftxui::color(type_color) | ftxui::bold);

    std::string stage_text = "[阶段 " + std::to_string(state->project->current_stage_index + 1) + "/" +
                             std::to_string(state->project->stages.size()) + " " + current_stage.name + "]";
    status_items.push_back(ftxui::text(stage_text) | ftxui::color(StageStateColor(current_stage.stage_state)));

    if (current_stage.stage_state == StageState::REVIEW) {
        std::string review_status = ReviewStatusColorLabel(current_stage.action_state, can_complete_stage);
        ftxui::Color review_color = ftxui::Color::Yellow;
        if (can_complete_stage) {
            review_color = (current_stage.action_state == ActionState::SUCCESS) ? ftxui::Color::Green : ftxui::Color::Yellow;
        } else if (current_stage.action_state == ActionState::FAILURE) {
            review_color = ftxui::Color::Red;
        }
        status_items.push_back(ftxui::text("[复核 " + review_status + "]") | ftxui::color(review_color));
        status_items.push_back(ftxui::text("[动作 " + ActionStateIcon(current_stage.action_state) + " " + ActionStateLabel(current_stage.action_state) + "]") |
                               ftxui::color(ActionStateColor(current_stage.action_state)));
    }

    if (current_stage.iteration_count > 0) {
        std::string iter_text = "[迭代 " + std::to_string(current_stage.iteration_count) + "/" +
                               std::to_string(current_stage.max_iterations) + "]";
        auto iter_color = (current_stage.iteration_count >= current_stage.max_iterations) ? ftxui::Color::Red :
                         (current_stage.iteration_count >= 3) ? ftxui::Color::Yellow : ftxui::Color::Green;
        status_items.push_back(ftxui::text(iter_text) | ftxui::color(iter_color));
    }

    if (total_tests > 0) {
        std::string test_text = "[验收 " + std::to_string(passed_tests) + "/" + std::to_string(total_tests);
        if (passed_tests == total_tests) {
            test_text += " ✅]";
        } else {
            test_text += "]";
        }
        auto test_color = (passed_tests == total_tests) ? ftxui::Color::Green :
                         (passed_tests > 0) ? ftxui::Color::Yellow : ftxui::Color::White;
        status_items.push_back(ftxui::text(test_text) | ftxui::color(test_color));
    }

    std::string status_icon = "[🟢]";
    auto status_color = ftxui::Color::Green;
    if (current_stage.stage_state == StageState::BLOCKED) {
        status_icon = "[🔴]";
        status_color = ftxui::Color::Red;
    } else if (current_stage.stage_state == StageState::RUNNING || state->is_processing) {
        status_icon = "[🟡]";
        status_color = ftxui::Color::Yellow;
    } else if (current_stage.stage_state == StageState::REVIEW) {
        if (can_complete_stage && current_stage.action_state == ActionState::SUCCESS) {
            status_icon = "[🟢]";
            status_color = ftxui::Color::Green;
        } else if (can_complete_stage) {
            status_icon = "[🟡]";
            status_color = ftxui::Color::Yellow;
        } else if (current_stage.action_state == ActionState::FAILURE) {
            status_icon = "[🔴]";
            status_color = ftxui::Color::Red;
        } else {
            status_icon = "[🟡]";
            status_color = ftxui::Color::Yellow;
        }
    }
    status_items.push_back(ftxui::text(status_icon) | ftxui::color(status_color) | ftxui::bold);

    return ftxui::hbox(status_items) | ftxui::border |
           ((state->ui_session.active_region == FocusRegion::STATUS_BAR)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}
