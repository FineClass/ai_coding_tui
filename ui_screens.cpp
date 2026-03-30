#include "ui_screens.h"
#include "ui_panels.h"
#include "project_persistence.h"
#include "workflow_controller.h"

#include <algorithm>
#include <filesystem>

namespace {

ftxui::Element RenderIterationHistoryWorkspace(const std::shared_ptr<AppState>& state) {
    const auto& stage = state->project->stages[state->project->current_stage_index];
    const auto& summary = stage.execution_summary;
    std::vector<ftxui::Element> content;
    content.push_back(ftxui::text("局部迭代详情 - " + stage.name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    content.push_back(ftxui::separator());
    content.push_back(ftxui::emptyElement());

    if (stage.iteration_history.empty()) {
        content.push_back(ftxui::text("暂无迭代记录") | ftxui::dim);
    } else {
        content.push_back(ftxui::text("迭代历史") | ftxui::bold);
        content.push_back(ftxui::emptyElement());

        std::vector<ftxui::Element> header_row;
        header_row.push_back(ftxui::text("迭代") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
        header_row.push_back(ftxui::separator());
        header_row.push_back(ftxui::text("操作") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
        header_row.push_back(ftxui::separator());
        header_row.push_back(ftxui::text("测试结果") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15));
        header_row.push_back(ftxui::separator());
        header_row.push_back(ftxui::text("原因/详情") | ftxui::bold | ftxui::flex);
        content.push_back(ftxui::hbox(header_row) | ftxui::border);

        for (const auto& record : stage.iteration_history) {
            std::vector<ftxui::Element> row;
            row.push_back(ftxui::text("迭代" + std::to_string(record.iteration_number)) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
            row.push_back(ftxui::separator());
            row.push_back(ftxui::text(record.status_icon + " " + record.action) |
                         ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
            row.push_back(ftxui::separator());

            auto test_element = ftxui::text(record.test_result) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15);
            if (record.test_result == "验收已确认") {
                test_element = test_element | ftxui::color(ftxui::Color::Green);
            } else if (record.test_result == "待进一步复核") {
                test_element = test_element | ftxui::color(ftxui::Color::Yellow);
            } else {
                test_element = test_element | ftxui::color(ftxui::Color::Red);
            }
            row.push_back(test_element);
            row.push_back(ftxui::separator());
            row.push_back(ftxui::text(record.summary + " / " + record.next_step) | ftxui::flex);

            content.push_back(ftxui::hbox(row) | ftxui::border);
            content.push_back(ftxui::text("  风险 " + record.reason) | ftxui::dim);
            if (!summary.build_summary.failure_lines.empty() && record.iteration_number == stage.iteration_history.back().iteration_number) {
                for (const auto& line : summary.build_summary.failure_lines) {
                    content.push_back(ftxui::text("  关键信号 " + line) | ftxui::color(ftxui::Color::Red));
                }
            }
        }
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::separator());

    std::string iter_stat = "迭代限制 已用 " + std::to_string(stage.iteration_count) + "/" +
                           std::to_string(stage.max_iterations) + " 次  [剩余 " +
                           std::to_string(stage.max_iterations - stage.iteration_count) + " 次]";
    content.push_back(ftxui::text(iter_stat) | ftxui::bold);

    if (stage.iteration_count >= stage.max_iterations) {
        content.push_back(ftxui::text("人工介入阈值 已达到最大迭代次数") | ftxui::color(ftxui::Color::Red) | ftxui::bold);
    } else if (stage.iteration_count >= 3) {
        content.push_back(ftxui::text("人工介入阈值 接近上限，建议关注") | ftxui::color(ftxui::Color::Yellow));
    } else {
        content.push_back(ftxui::text("人工介入阈值 超过 " + std::to_string(stage.max_iterations) + " 次自动请求确认"));
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::text("[Esc] 返回主工作区") | ftxui::dim | ftxui::center);

    return ftxui::vbox(content) |
           ((state->ui_session.active_region == FocusRegion::WORKBENCH)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderGlobalValidationWorkspace(const std::shared_ptr<AppState>& state) {
    const auto& stage = state->project->stages[state->project->current_stage_index];
    const auto& summary = stage.execution_summary;
    std::vector<ftxui::Element> content;
    content.push_back(ftxui::text("全局复核视图 - " + stage.name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    content.push_back(ftxui::separator());
    content.push_back(ftxui::emptyElement());

    int passed = workflow::CountPassedAcceptanceTests(stage);
    int total = stage.acceptance_tests.size();
    bool can_complete = total > 0 && passed == total;

    ActionState validation_state = stage.action_state;
    if (stage.stage_state == StageState::DONE) {
        validation_state = ActionState::SUCCESS;
    } else if (stage.stage_state == StageState::BLOCKED) {
        validation_state = ActionState::FAILURE;
    }

    content.push_back(ftxui::text("复核状态 " + ReviewActionLabel(validation_state, can_complete)) |
                      ftxui::bold | ftxui::color(ActionStateColor(validation_state)));
    content.push_back(ftxui::text("验收确认 " + std::to_string(passed) + "/" + std::to_string(total)) | ftxui::dim);
    content.push_back(ftxui::emptyElement());

    content.push_back(ftxui::text("当前工作项 / 验收清单映射") | ftxui::bold);
    content.push_back(ftxui::emptyElement());

    std::vector<ftxui::Element> header_row;
    header_row.push_back(ftxui::text("工作目录条目") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 25));
    header_row.push_back(ftxui::separator());
    header_row.push_back(ftxui::text("测试用例") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
    header_row.push_back(ftxui::separator());
    header_row.push_back(ftxui::text("状态") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
    header_row.push_back(ftxui::separator());
    header_row.push_back(ftxui::text("详情") | ftxui::bold | ftxui::flex);
    content.push_back(ftxui::hbox(header_row) | ftxui::border);

    int test_num = 1;
    for (const auto& test : stage.acceptance_tests) {
        std::vector<ftxui::Element> row;
        row.push_back(ftxui::text(test.description) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 25));
        row.push_back(ftxui::separator());
        row.push_back(ftxui::text("TC-" + std::string(3 - std::to_string(test_num).length(), '0') +
                     std::to_string(test_num)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
        row.push_back(ftxui::separator());

        std::string status_text = "⚪ 未确认";
        ftxui::Color status_color = ftxui::Color::White;
        if (test.passed) {
            status_text = "✅ 已确认";
            status_color = ftxui::Color::Green;
        } else if (validation_state == ActionState::PARTIAL) {
            status_text = "🟡 待复核";
            status_color = ftxui::Color::Yellow;
        } else if (validation_state == ActionState::FAILURE) {
            status_text = "❌ 待排查";
            status_color = ftxui::Color::Red;
        } else if (validation_state == ActionState::PROCESSING) {
            status_text = "⏳ 处理中";
            status_color = ftxui::Color::Yellow;
        }
        row.push_back(ftxui::text(status_text) |
                      ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12) |
                      ftxui::color(status_color));
        row.push_back(ftxui::separator());

        std::string detail = test.notes.empty() ? summary.result_summary : test.notes;
        if (!test.passed && validation_state == ActionState::PARTIAL) {
            if (!detail.empty()) detail += " | ";
            detail += "构建已完成，建议继续确认该项验收";
        } else if (!test.passed && validation_state == ActionState::FAILURE && !summary.build_summary.failure_lines.empty()) {
            if (!detail.empty()) detail += " | ";
            detail += "优先排查 " + summary.build_summary.failure_lines.front();
        }
        row.push_back(ftxui::text(detail) | ftxui::flex);

        content.push_back(ftxui::hbox(row) | ftxui::border);
        test_num++;
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::separator());

    int percentage = total > 0 ? (passed * 100 / total) : 0;
    std::string progress_text = "验收确认进度 " + std::to_string(passed) + "/" +
                               std::to_string(total) + " 已确认 (" +
                               std::to_string(percentage) + "%)";
    auto progress_element = ftxui::text(progress_text) | ftxui::bold;
    if (can_complete && validation_state == ActionState::SUCCESS) {
        progress_element = progress_element | ftxui::color(ftxui::Color::Green);
    } else if (can_complete) {
        progress_element = progress_element | ftxui::color(ftxui::Color::Yellow);
    } else if (validation_state == ActionState::FAILURE) {
        progress_element = progress_element | ftxui::color(ftxui::Color::Red);
    } else {
        progress_element = progress_element | ftxui::color(ftxui::Color::Yellow);
    }
    content.push_back(progress_element);
    content.push_back(ftxui::text("完成门槛 验收清单全部确认") | ftxui::dim);
    content.push_back(ftxui::text("结果摘要 " + summary.result_summary) | ftxui::dim);
    if (validation_state == ActionState::PARTIAL) {
        content.push_back(ftxui::text("复核提示 构建已完成，建议继续逐项确认验收") | ftxui::dim);
    } else if (validation_state == ActionState::FAILURE && !summary.build_summary.failure_lines.empty()) {
        content.push_back(ftxui::text("复核提示 优先排查关键信号后再继续确认验收") | ftxui::dim);
    }

    content.push_back(ftxui::emptyElement());

    if (can_complete && validation_state == ActionState::SUCCESS) {
        content.push_back(ftxui::text("✅ 验收与复核均已完成，可以完成当前阶段") |
                         ftxui::color(ftxui::Color::Green) | ftxui::bold);
    } else if (can_complete && validation_state == ActionState::PARTIAL) {
        content.push_back(ftxui::text("🟡 验收已完成，可以完成当前阶段；但仍建议先复核关键改动") |
                         ftxui::color(ftxui::Color::Yellow) | ftxui::bold);
    } else if (can_complete && validation_state == ActionState::FAILURE) {
        content.push_back(ftxui::text("🟡 验收已完成，可以完成当前阶段；但仍建议先排查关键信号") |
                         ftxui::color(ftxui::Color::Yellow) | ftxui::bold);
    } else if (validation_state == ActionState::PARTIAL) {
        content.push_back(ftxui::text("🟡 仍在等待验收确认；构建已完成，建议继续逐项复核") |
                         ftxui::color(ftxui::Color::Yellow) | ftxui::bold);
    } else if (validation_state == ActionState::FAILURE) {
        content.push_back(ftxui::text("❌ 当前阶段存在失败信号，请先处理关键信号再继续") |
                         ftxui::color(ftxui::Color::Red) | ftxui::bold);
    } else {
        content.push_back(ftxui::text("⚠️ 验收清单未全部确认，不允许标记项目完成") |
                         ftxui::color(ftxui::Color::Red) | ftxui::bold);
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::text("[Esc] 返回主工作区") | ftxui::dim | ftxui::center);

    return ftxui::vbox(content) |
           ((state->ui_session.active_region == FocusRegion::WORKBENCH)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderCompletionSummary(const std::shared_ptr<AppState>& state) {
    std::vector<ftxui::Element> content;
    content.push_back(ftxui::text("🎉 项目完成总结") | ftxui::bold | ftxui::color(ftxui::Color::Green));
    content.push_back(ftxui::separator());
    content.push_back(ftxui::emptyElement());

    content.push_back(ftxui::text("项目名称: " + state->project->name) | ftxui::bold);
    content.push_back(ftxui::text("项目类型: " + ProjectTypeToString(state->project->type)));
    content.push_back(ftxui::text("工作目录: " + state->project->working_directory));
    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::separator());

    int total_iterations = 0;
    std::vector<std::string> all_key_files;
    for (size_t i = 0; i < state->project->stages.size(); ++i) {
        const auto& stage = state->project->stages[i];
        total_iterations += stage.iteration_count;
        for (const auto& f : stage.execution_summary.key_files) {
            if (std::find(all_key_files.begin(), all_key_files.end(), f) == all_key_files.end()) {
                all_key_files.push_back(f);
            }
        }
        content.push_back(ftxui::text("阶段 " + std::to_string(i + 1) + ": " + stage.name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        content.push_back(ftxui::text("  迭代次数: " + std::to_string(stage.iteration_count)));
        content.push_back(ftxui::text("  结果摘要: " + stage.execution_summary.result_summary));
        content.push_back(ftxui::text("  动作状态: " + ActionStateIcon(stage.action_state) + " " + ActionStateLabel(stage.action_state)));
        content.push_back(ftxui::emptyElement());
    }
    content.push_back(ftxui::separator());

    content.push_back(ftxui::text("总迭代次数: " + std::to_string(total_iterations)) | ftxui::bold);
    content.push_back(ftxui::emptyElement());

    content.push_back(ftxui::text("涉及关键文件:") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
    for (const auto& f : all_key_files) {
        content.push_back(ftxui::text("  - " + f));
    }
    content.push_back(ftxui::emptyElement());

    const auto& last_stage = state->project->stages.back();
    if (last_stage.execution_summary.build_summary.command_ran) {
        auto build_ok = last_stage.execution_summary.build_summary.success;
        content.push_back(ftxui::text("最终构建状态: " + std::string(build_ok ? "✅ 通过" : "❌ 失败"))
            | ftxui::color(build_ok ? ftxui::Color::Green : ftxui::Color::Red) | ftxui::bold);
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::separator());
    content.push_back(ftxui::text("[I] 查看迭代历史 · [V] 查看全局复核") | ftxui::dim | ftxui::center);

    return ftxui::vbox(content) |
           ((state->ui_session.active_region == FocusRegion::WORKBENCH)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderNormalWorkspace(const std::shared_ptr<AppState>& state) {
    bool all_done = true;
    for (const auto& s : state->project->stages) {
        if (s.stage_state != StageState::DONE) { all_done = false; break; }
    }
    if (all_done) {
        return RenderCompletionSummary(state);
    }

    const auto& stage = state->project->stages[state->project->current_stage_index];
    const auto& summary = stage.execution_summary;
    std::vector<ftxui::Element> content;

    std::string header = "当前阶段 " + stage.name;
    if (stage.iteration_count > 0) {
        header += " [迭代 " + std::to_string(stage.iteration_count) + "/" +
                  std::to_string(stage.max_iterations) + "]";
    }
    content.push_back(ftxui::text(header) | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
    content.push_back(ftxui::text("阶段目标 " + stage.goal) | ftxui::dim);
    content.push_back(ftxui::separator());

    if (stage.stage_state == StageState::BLOCKED) {
        content.push_back(ftxui::text("⚠️ 需要人工介入") | ftxui::color(ftxui::Color::Red) | ftxui::bold);
        content.push_back(ftxui::text(stage.output));
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::text("[R] 重置并重试") | ftxui::dim);
    } else if (stage.stage_state == StageState::RUNNING) {
        content.push_back(ftxui::text("正在执行当前目录下的实际阶段检查...") | ftxui::color(ftxui::Color::Green) | ftxui::bold);
        content.push_back(ftxui::emptyElement());

        int progress = state->processing_progress;
        std::string progress_bar = "[";
        for (int i = 0; i < 20; i++) {
            progress_bar += (i < progress / 5) ? "█" : "░";
        }
        progress_bar += "] " + std::to_string(progress) + "%";
        content.push_back(ftxui::text(progress_bar) | ftxui::color(ftxui::Color::Cyan));
        content.push_back(ftxui::emptyElement());

        content.push_back(ftxui::text("处理进度") | ftxui::bold);
        size_t start = stage.conversation_history.size() > 5 ?
                      stage.conversation_history.size() - 5 : 0;
        for (size_t i = start; i < stage.conversation_history.size(); ++i) {
            content.push_back(ftxui::text("  " + stage.conversation_history[i]) | ftxui::dim);
        }
    } else {
        content.push_back(ftxui::text("结果摘要 " + summary.result_summary) | ftxui::bold);
        if (stage.stage_state == StageState::REVIEW) {
            const bool stage_can_complete = workflow::CanCompleteStage(stage);
            content.push_back(ftxui::text("复核状态 " + ReviewStatusColorLabel(stage.action_state, stage_can_complete)) |
                              ftxui::color(ActionStateColor(stage.action_state)));
            if (!summary.risk_summary.empty()) {
                content.push_back(ftxui::text("风险提示 " + summary.risk_summary) | ftxui::color(ftxui::Color::Yellow));
            }
        }
        content.push_back(ftxui::emptyElement());
        if (state->project->type == ProjectType::NEW_FEATURE) {
            content.push_back(ftxui::text("当前流程 新功能 / 优化功能") | ftxui::bold | ftxui::color(ftxui::Color::Blue));
        } else if (state->project->type == ProjectType::BUG_FIX) {
            content.push_back(ftxui::text("当前流程 缺陷修复") | ftxui::bold | ftxui::color(ftxui::Color::Red));
        }
        content.push_back(ftxui::text("项目观察") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        for (const auto& item : summary.project_observations) {
            content.push_back(ftxui::text("- " + item));
        }
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::text("关键文件") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        for (const auto& item : summary.key_files) {
            content.push_back(ftxui::text("- " + item));
        }
        if (!summary.project_observations.empty()) {
            for (const auto& item : summary.project_observations) {
                if (item.find("构建文件:") == 0 || item.find("核心目录:") == 0 || item.find("入口线索:") == 0) {
                    content.push_back(ftxui::text("* " + item) | ftxui::color(ftxui::Color::Yellow));
                }
            }
        }
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::text("实施说明") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        for (const auto& item : summary.implementation_notes) {
            content.push_back(ftxui::text("- " + item));
        }
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::text("复核重点") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        for (const auto& item : summary.validation_points) {
            content.push_back(ftxui::text("- " + item));
        }
        if (summary.build_summary.command_ran) {
            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::text("构建结果") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
            content.push_back(ftxui::text("- 返回状态 " + std::to_string(summary.build_summary.return_code)));
            content.push_back(ftxui::text("- 结论 " + summary.build_summary.headline));
            for (const auto& item : summary.build_summary.details) {
                content.push_back(ftxui::text("- " + item));
            }
            for (const auto& item : summary.build_summary.failure_lines) {
                content.push_back(ftxui::text("- 关键信号 " + item) | ftxui::color(ftxui::Color::Red));
            }
            for (const auto& item : summary.build_summary.related_files) {
                content.push_back(ftxui::text("- 相关文件 " + item) | ftxui::color(ftxui::Color::Yellow));
            }
        }
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::text("下一步") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        for (const auto& item : summary.next_actions) {
            content.push_back(ftxui::text("- " + item));
        }

        content.push_back(ftxui::emptyElement());
        if (stage.stage_state == StageState::DONE) {
            if (state->project->current_stage_index < (int)state->project->stages.size() - 1) {
                content.push_back(ftxui::text("[Enter/Space] 进入下一阶段") | ftxui::dim);
            } else {
                content.push_back(ftxui::text("🎉 项目全部完成!") | ftxui::bold | ftxui::color(ftxui::Color::Blue));
            }
        } else if (stage.stage_state == StageState::REVIEW) {
            if (workflow::CanCompleteStage(stage)) {
                if (stage.action_state == ActionState::SUCCESS) {
                    content.push_back(ftxui::text("[C] 完成当前阶段") | ftxui::dim);
                } else {
                    content.push_back(ftxui::text("[C] 完成当前阶段；如有需要先查看右侧建议") | ftxui::dim);
                }
            } else {
                content.push_back(ftxui::text("[T] 继续确认验收项，全部确认后再完成阶段") | ftxui::dim);
            }
        } else if (stage.stage_state == StageState::READY || stage.stage_state == StageState::PENDING) {
            content.push_back(ftxui::text("[Enter/Space] 开始当前阶段") | ftxui::dim);
        }
    }

    if (!stage.acceptance_tests.empty() && stage.stage_state != StageState::RUNNING) {
        content.push_back(ftxui::emptyElement());
        content.push_back(ftxui::separator());
        content.push_back(ftxui::text("📋 验收测试清单") | ftxui::bold);

        for (const auto& test : stage.acceptance_tests) {
            std::string icon = "⚪";
            ftxui::Color test_color = ftxui::Color::White;
            if (test.passed) {
                icon = "✅";
                test_color = ftxui::Color::Green;
            } else if (stage.stage_state == StageState::REVIEW && stage.action_state == ActionState::PARTIAL) {
                icon = "🟡";
                test_color = ftxui::Color::Yellow;
            } else if (stage.stage_state == StageState::REVIEW && stage.action_state == ActionState::FAILURE) {
                icon = "❌";
                test_color = ftxui::Color::Red;
            } else if (stage.stage_state == StageState::RUNNING) {
                icon = "⏳";
                test_color = ftxui::Color::Yellow;
            }
            content.push_back(ftxui::text(icon + " " + test.description) | ftxui::color(test_color));
            if (!test.notes.empty()) {
                auto note = ftxui::text("    " + test.notes) | ftxui::dim;
                if (!test.passed && stage.stage_state == StageState::REVIEW && stage.action_state == ActionState::FAILURE) {
                    note = note | ftxui::color(ftxui::Color::Red);
                } else if (!test.passed && stage.stage_state == StageState::REVIEW) {
                    note = note | ftxui::color(ftxui::Color::Yellow);
                }
                content.push_back(note);
            }
        }

        if (stage.stage_state == StageState::REVIEW) {
            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::text("[T] 标记下一项验收已确认（基于实际结果）") | ftxui::dim);
        }
    }

    content.push_back(ftxui::emptyElement());
    content.push_back(ftxui::separator());
    content.push_back(ftxui::text("[I] 查看迭代历史 · [V] 查看全局复核") | ftxui::dim | ftxui::center);

    return ftxui::vbox(content) |
           ((state->ui_session.active_region == FocusRegion::WORKBENCH)
                ? ftxui::bgcolor(ftxui::Color::GrayDark)
                : ftxui::nothing);
}

ftxui::Element RenderWorkspaceOverlay(const std::shared_ptr<AppState>& state, ftxui::Element workspace_shell) {
    if (state->ui_session.overlay_kind == OverlayKind::NONE) {
        return workspace_shell;
    }

    const auto& current_stage = state->project->stages[state->project->current_stage_index];
    int passed_tests = workflow::CountPassedAcceptanceTests(current_stage);
    int total_tests = current_stage.acceptance_tests.size();
    const bool can_complete_stage = total_tests > 0 && passed_tests == total_tests;
    std::vector<ftxui::Element> overlay_lines;

    std::string overlay_title = "需要确认";
    ftxui::Color overlay_color = ftxui::Color::Red;
    std::string action_title = "恢复动作";
    std::string footer_hint = "[Enter/Space] 关闭提示层 · [Esc] 返回";
    if (current_stage.stage_state == StageState::BLOCKED) {
        overlay_title = "阶段已阻塞";
    } else if (current_stage.stage_state == StageState::REVIEW && can_complete_stage &&
               current_stage.action_state == ActionState::PARTIAL) {
        overlay_title = "建议先复核";
        overlay_color = ftxui::Color::Yellow;
        action_title = "复核建议";
        footer_hint = "[Enter/Space] 关闭并返回右栏 · [Esc] 返回";
    } else if (current_stage.stage_state == StageState::REVIEW && can_complete_stage &&
               current_stage.action_state == ActionState::FAILURE) {
        overlay_title = "存在失败信号";
        overlay_color = ftxui::Color::Yellow;
        action_title = "排查建议";
        footer_hint = "[Enter/Space] 关闭并返回右栏 · [Esc] 返回";
    }

    overlay_lines.push_back(ftxui::text(overlay_title) | ftxui::bold | ftxui::color(overlay_color));
    overlay_lines.push_back(ftxui::separator());
    overlay_lines.push_back(ftxui::text(current_stage.execution_summary.result_summary.empty() ? current_stage.goal : current_stage.execution_summary.result_summary));
    if (current_stage.stage_state == StageState::REVIEW && total_tests > 0) {
        overlay_lines.push_back(ftxui::text("验收确认 " + std::to_string(passed_tests) + "/" + std::to_string(total_tests)) |
                                ftxui::dim);
    }
    overlay_lines.push_back(ftxui::emptyElement());
    if (!current_stage.execution_summary.next_actions.empty()) {
        overlay_lines.push_back(ftxui::text(action_title) | ftxui::bold);
        for (const auto& action : current_stage.execution_summary.next_actions) {
            overlay_lines.push_back(ftxui::text("- " + action));
        }
        overlay_lines.push_back(ftxui::emptyElement());
    }
    std::string overlay_footer = footer_hint;
    if (current_stage.stage_state == StageState::DONE ||
        (current_stage.stage_state == StageState::REVIEW && workflow::CanCompleteStage(current_stage))) {
        overlay_footer += " · [C] 完成阶段";
    }
    overlay_lines.push_back(ftxui::text(overlay_footer) | ftxui::dim);

    auto overlay = ftxui::vbox(overlay_lines) |
                   ftxui::border |
                   ftxui::bgcolor(ftxui::Color::Black) |
                   ftxui::color(ftxui::Color::White) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 70);

    return ftxui::dbox({
        workspace_shell,
        ftxui::vbox({ftxui::filler(), ftxui::hbox({ftxui::filler(), overlay, ftxui::filler()}), ftxui::filler()})
    });
}

}  // namespace

bool route_workspace_event(const std::shared_ptr<AppState>& state, ftxui::Event event) {
    if (state->ui_session.input_mode) {
        if (event == ftxui::Event::Escape) {
            state->ui_session.input_mode = false;
            state->ui_session.user_input.clear();
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::Return) {
            if (!state->ui_session.user_input.empty()) {
                auto& stage = state->project->stages[state->project->current_stage_index];
                stage.conversation_history.push_back("用户: " + state->ui_session.user_input);
            }
            state->ui_session.input_mode = false;
            state->ui_session.user_input.clear();
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::Backspace) {
            if (!state->ui_session.user_input.empty()) {
                state->ui_session.user_input.pop_back();
                while (!state->ui_session.user_input.empty() &&
                       (static_cast<unsigned char>(state->ui_session.user_input.back()) & 0xC0) == 0x80) {
                    state->ui_session.user_input.pop_back();
                }
            }
            state->NotifyListeners();
            return true;
        }
        if (event.is_character()) {
            state->ui_session.user_input += event.character();
            state->NotifyListeners();
            return true;
        }
        return true;
    }

    if (state->ui_session.overlay_kind != OverlayKind::NONE) {
        auto& current_stage = state->project->stages[state->project->current_stage_index];
        if (event == ftxui::Event::Character('c') || event == ftxui::Event::Character('C')) {
            if (current_stage.stage_state != StageState::RUNNING &&
                (current_stage.stage_state == StageState::DONE ||
                 (current_stage.stage_state == StageState::REVIEW && workflow::CanCompleteStage(current_stage)))) {
                workflow::CompleteCurrentStage(*state->project, state->is_processing);
                state->ui_session.overlay_kind = OverlayKind::NONE;
                state->ui_session.input_captured_by_overlay = false;
                state->ui_session.active_region = state->ui_session.last_focus_region;
                state->NotifyListeners();
                return true;
            }
            return false;
        }
        if (event == ftxui::Event::Escape || event == ftxui::Event::Return || event == ftxui::Event::Character(' ')) {
            state->ui_session.overlay_kind = OverlayKind::NONE;
            state->ui_session.input_captured_by_overlay = false;
            state->ui_session.active_region = state->ui_session.last_focus_region;
            state->NotifyListeners();
            return true;
        }
        return true;
    }

    if (event == ftxui::Event::Tab) {
        state->ui_session.last_focus_region = state->ui_session.active_region;
        state->ui_session.active_region = NextFocusRegion(state->ui_session.active_region);
        state->NotifyListeners();
        return true;
    }
    if (event == ftxui::Event::TabReverse) {
        state->ui_session.last_focus_region = state->ui_session.active_region;
        state->ui_session.active_region = PreviousFocusRegion(state->ui_session.active_region);
        state->NotifyListeners();
        return true;
    }

    if (state->ui_session.workspace_view != WorkspaceView::NORMAL) {
        if (event == ftxui::Event::Escape) {
            state->ui_session.workspace_view = WorkspaceView::NORMAL;
            state->ui_session.scroll_y = 0.0f;
            state->ui_session.active_region = state->ui_session.last_focus_region;
            state->NotifyListeners();
            return true;
        }
        return false;
    }

    if (state->ui_session.active_region == FocusRegion::WORKBENCH) {
        constexpr float kScrollStep = 0.05f;
        constexpr float kPageStep = 0.25f;
        if (event == ftxui::Event::ArrowUp) {
            state->ui_session.scroll_y = std::max(0.f, state->ui_session.scroll_y - kScrollStep);
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            state->ui_session.scroll_y = std::min(1.f, state->ui_session.scroll_y + kScrollStep);
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::PageUp) {
            state->ui_session.scroll_y = std::max(0.f, state->ui_session.scroll_y - kPageStep);
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::PageDown) {
            state->ui_session.scroll_y = std::min(1.f, state->ui_session.scroll_y + kPageStep);
            state->NotifyListeners();
            return true;
        }
    }

    auto& stage = state->project->stages[state->project->current_stage_index];

    if (event == ftxui::Event::Return || event == ftxui::Event::Character(' ')) {
        if (state->ui_session.active_region == FocusRegion::PROJECT_TREE) {
            if (event == ftxui::Event::Return) {
                state->project->current_stage_index = std::max(0, std::min(state->ui_session.project_tree_index,
                    (int)state->project->stages.size() - 1));
                workflow::SyncDerivedProjectState(*state->project);
                state->NotifyListeners();
                return true;
            }
            return false;
        }

        if (state->ui_session.active_region == FocusRegion::INFO_PANEL) {
            int passed_tests = workflow::CountPassedAcceptanceTests(stage);
            int total_tests = stage.acceptance_tests.size();
            const bool can_complete_stage = total_tests > 0 && passed_tests == total_tests;

            if (stage.stage_state == StageState::BLOCKED ||
                (stage.stage_state == StageState::REVIEW && can_complete_stage &&
                 (stage.action_state == ActionState::PARTIAL || stage.action_state == ActionState::FAILURE))) {
                state->ui_session.last_focus_region = state->ui_session.active_region;
                state->ui_session.active_region = FocusRegion::OVERLAY;
                state->ui_session.overlay_kind = OverlayKind::RECOVERY_ACTIONS;
                state->ui_session.input_captured_by_overlay = true;
                state->NotifyListeners();
                return true;
            }
        }

        if ((stage.stage_state == StageState::READY || stage.stage_state == StageState::PENDING) &&
            stage.stage_state != StageState::BLOCKED) {
            state->start_current_stage();
            return true;
        }
        if (stage.stage_state == StageState::DONE &&
            state->project->current_stage_index < (int)state->project->stages.size() - 1) {
            workflow::CompleteCurrentStage(*state->project, state->is_processing);
            state->start_current_stage();
            return true;
        }
        return false;
    }

    if (state->ui_session.active_region == FocusRegion::PROJECT_TREE) {
        if (event == ftxui::Event::ArrowUp) {
            state->ui_session.project_tree_index = std::max(0, state->ui_session.project_tree_index - 1);
            state->NotifyListeners();
            return true;
        }
        if (event == ftxui::Event::ArrowDown) {
            state->ui_session.project_tree_index = std::min((int)state->project->stages.size() - 1, state->ui_session.project_tree_index + 1);
            state->NotifyListeners();
            return true;
        }
    }

    if (event == ftxui::Event::Character('t') || event == ftxui::Event::Character('T')) {
        if (workflow::ConfirmNextAcceptanceTest(stage)) {
            return true;
        }
        return false;
    }

    if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R')) {
        if (stage.stage_state == StageState::BLOCKED) {
            workflow::RetryCurrentStage(*state->project, state->is_processing);
            return true;
        }
        return false;
    }

    if (event == ftxui::Event::Character('c') || event == ftxui::Event::Character('C')) {
        if (stage.stage_state == StageState::RUNNING) {
            return false;
        }
        if (stage.stage_state == StageState::DONE) {
            workflow::CompleteCurrentStage(*state->project, state->is_processing);
            return true;
        }
        if (stage.stage_state == StageState::REVIEW && workflow::CanCompleteStage(stage)) {
            workflow::CompleteCurrentStage(*state->project, state->is_processing);
            return true;
        }
        return false;
    }

    if (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) {
        state->ui_session.last_focus_region = state->ui_session.active_region;
        state->ui_session.workspace_view = WorkspaceView::ITERATION_HISTORY;
        state->ui_session.scroll_y = 0.0f;
        state->NotifyListeners();
        return true;
    }

    if (event == ftxui::Event::Character('v') || event == ftxui::Event::Character('V')) {
        state->ui_session.last_focus_region = state->ui_session.active_region;
        state->ui_session.workspace_view = WorkspaceView::GLOBAL_VALIDATION;
        state->ui_session.scroll_y = 0.0f;
        state->NotifyListeners();
        return true;
    }

    if (event == ftxui::Event::Character('/')) {
        if (!state->is_processing) {
            state->ui_session.input_mode = true;
            state->ui_session.user_input.clear();
            state->NotifyListeners();
            return true;
        }
        return false;
    }

    return false;
}

ftxui::Component CreateTypeSelectionScreen(std::shared_ptr<AppState> state) {
    auto selected = std::make_shared<int>(0);
    int max_options = state->has_saved_state ? 2 : 1;

    auto renderer = ftxui::Renderer([state, selected, max_options] {
        std::vector<ftxui::Element> children;
        children.push_back(ftxui::text("选择项目类型") | ftxui::center | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        children.push_back(ftxui::emptyElement());

        if (state->has_saved_state) {
            std::vector<ftxui::Element> card0;
            card0.push_back(ftxui::text("📂 恢复上次项目") | ftxui::bold | ftxui::color(ftxui::Color::Green));
            card0.push_back(ftxui::emptyElement());
            card0.push_back(ftxui::text("检测到上次保存的项目状态，可以继续上次的工作"));
            card0.push_back(ftxui::emptyElement());
            card0.push_back(ftxui::text("[Enter/Space] 恢复项目") | ftxui::center);
            auto card0_box = ftxui::vbox(card0) | ftxui::border;
            if (*selected == 0) {
                card0_box = card0_box | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
            }
            children.push_back(card0_box);
            children.push_back(ftxui::emptyElement());
        }

        int offset = state->has_saved_state ? 1 : 0;

        std::vector<ftxui::Element> card1;
        card1.push_back(ftxui::text("🔵 新功能/优化功能") | ftxui::bold | ftxui::color(ftxui::Color::Blue));
        card1.push_back(ftxui::emptyElement());
        card1.push_back(ftxui::text("适用场景 从零开发新功能、现有功能优化"));
        card1.push_back(ftxui::text("流程 需求探讨 → 方案设计 → 任务迭代（局部+全局）"));
        card1.push_back(ftxui::text("产物 需求文档、设计文档、验收测试清单"));
        card1.push_back(ftxui::emptyElement());
        card1.push_back(ftxui::text("[Enter/Space] 选择此类型") | ftxui::center);
        auto card1_box = ftxui::vbox(card1) | ftxui::border;
        if (*selected == offset) {
            card1_box = card1_box | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
        }
        children.push_back(card1_box);
        children.push_back(ftxui::emptyElement());

        std::vector<ftxui::Element> card2;
        card2.push_back(ftxui::text("🔴 修复缺陷") | ftxui::bold | ftxui::color(ftxui::Color::Red));
        card2.push_back(ftxui::emptyElement());
        card2.push_back(ftxui::text("适用场景 Bug 修复、安全漏洞修补"));
        card2.push_back(ftxui::text("流程 明确问题 → 修复方案 → 执行迭代"));
        card2.push_back(ftxui::text("产物 问题报告、修复方案、回归测试结果"));
        card2.push_back(ftxui::emptyElement());
        card2.push_back(ftxui::text("[Enter/Space] 选择此类型") | ftxui::center);
        auto card2_box = ftxui::vbox(card2) | ftxui::border;
        if (*selected == offset + 1) {
            card2_box = card2_box | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
        }
        children.push_back(card2_box);
        children.push_back(ftxui::emptyElement());
        children.push_back(ftxui::text("快捷键 [↑↓] 选择 · [Enter/Space] 确认 · [Ctrl+Q] 退出") | ftxui::center | ftxui::dim);

        return ftxui::vbox(children) | ftxui::center | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80);
    });

    return ftxui::CatchEvent(renderer, [state, selected, max_options](ftxui::Event event) {
        if (event == ftxui::Event::ArrowUp) {
            *selected = (*selected == 0) ? max_options : *selected - 1;
            return true;
        } else if (event == ftxui::Event::ArrowDown) {
            *selected = (*selected == max_options) ? 0 : *selected + 1;
            return true;
        } else if (event == ftxui::Event::Return || event == ftxui::Event::Character(' ')) {
            int offset = state->has_saved_state ? 1 : 0;
            if (state->has_saved_state && *selected == 0) {
                auto cwd = std::filesystem::current_path().string();
                auto path = persistence::DefaultSavePath(cwd);
                auto project = std::make_shared<Project>("", ProjectType::NEW_FEATURE);
                if (persistence::LoadProject(*project, path)) {
                    state->project = project;
                    state->screen = AppScreen::MAIN_WORKSPACE;
                    workflow::SyncDerivedProjectState(*state->project);
                    state->NotifyListeners();
                    return true;
                }
            }
            int type_index = *selected - offset;
            state->project = std::make_shared<Project>("MyProject", type_index == 0 ? ProjectType::NEW_FEATURE : ProjectType::BUG_FIX);
            state->screen = AppScreen::INPUT_NAME;
            state->NotifyListeners();
            return true;
        }
        return false;
    });
}

ftxui::Component CreateNameInputScreen(std::shared_ptr<AppState> state) {
    auto input = ftxui::Input(&state->input_name, "输入项目名称");

    auto submit_project = [state] {
        if (!state->input_name.empty()) {
            state->project->name = state->input_name;
            state->screen = AppScreen::MAIN_WORKSPACE;
            if (!state->project->stages.empty()) {
                auto& first_stage = state->project->stages[state->project->current_stage_index];
                first_stage.stage_state = StageState::READY;
                first_stage.action_state = ActionState::IDLE;
                workflow::SyncDerivedProjectState(*state->project);
            }
            state->NotifyListeners();
        }
    };

    auto submit_btn = ftxui::Button("开始创建", submit_project);

    auto cancel_btn = ftxui::Button("返回", [state] {
        state->screen = AppScreen::SELECT_TYPE;
        state->input_name.clear();
        state->NotifyListeners();
    });

    auto container = ftxui::Container::Vertical({input, submit_btn, cancel_btn});
    auto component = ftxui::CatchEvent(container, [state, submit_project](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            submit_project();
            return true;
        }
        if (event == ftxui::Event::Escape) {
            state->screen = AppScreen::SELECT_TYPE;
            state->input_name.clear();
            state->NotifyListeners();
            return true;
        }
        return false;
    });

    return ftxui::Renderer(component, [container, state] {
        std::vector<ftxui::Element> children;
        children.push_back(ftxui::text("项目设置") | ftxui::center | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        children.push_back(ftxui::emptyElement());

        std::vector<ftxui::Element> info_card;
        std::string type_name = (state->project->type == ProjectType::NEW_FEATURE) ?
            "🔵 新功能/优化功能" : "🔴 修复缺陷";
        info_card.push_back(ftxui::text("已选择类型 " + type_name) | ftxui::bold);
        info_card.push_back(ftxui::emptyElement());
        info_card.push_back(ftxui::text("工作目录 " + state->project->working_directory));
        info_card.push_back(ftxui::emptyElement());
        info_card.push_back(ftxui::text(state->project->type == ProjectType::NEW_FEATURE ?
            "流程 需求探讨 → 方案设计 → 任务迭代（局部+全局）" :
            "流程 明确问题 → 修复方案 → 执行迭代"));

        children.push_back(ftxui::vbox(info_card) | ftxui::border | ftxui::color(ftxui::Color::Blue));
        children.push_back(ftxui::emptyElement());

        std::vector<ftxui::Element> input_card;
        input_card.push_back(ftxui::text("📝 项目名称") | ftxui::bold);
        input_card.push_back(ftxui::emptyElement());
        input_card.push_back(container->Render());

        children.push_back(ftxui::vbox(input_card) | ftxui::border);
        children.push_back(ftxui::emptyElement());
        children.push_back(ftxui::text("[Tab] 切换 · [Enter] 确认 · [Esc] 返回") | ftxui::center | ftxui::dim);

        return ftxui::vbox(children) | ftxui::center | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 70);
    });
}

ftxui::Component CreateMainWorkspace(std::shared_ptr<AppState> state) {
    auto nav_renderer = ftxui::Renderer([state] {
        return RenderProjectTreePanel(state);
    });

    auto workspace_renderer = ftxui::Renderer([state] {
        if (!state->project) return ftxui::text("项目未初始化");
        ftxui::Element content;
        if (state->ui_session.workspace_view == WorkspaceView::ITERATION_HISTORY) {
            content = RenderIterationHistoryWorkspace(state);
        } else if (state->ui_session.workspace_view == WorkspaceView::GLOBAL_VALIDATION) {
            content = RenderGlobalValidationWorkspace(state);
        } else {
            content = RenderNormalWorkspace(state);
        }
        content = content
            | ftxui::focusPositionRelative(0.f, state->ui_session.scroll_y)
            | ftxui::yframe
            | ftxui::vscroll_indicator
            | ftxui::flex;

        if (state->ui_session.input_mode) {
            auto input_box = ftxui::hbox({
                ftxui::text("/ ") | ftxui::bold | ftxui::color(ftxui::Color::Yellow),
                ftxui::text(state->ui_session.user_input),
                ftxui::text("_") | ftxui::blink,
            }) | ftxui::border | ftxui::bgcolor(ftxui::Color::Black);
            content = ftxui::vbox({content, input_box});
        }
        return content;
    });

    auto context_renderer = ftxui::Renderer([state] {
        return RenderContextPanel(state);
    });

    auto main_container = ftxui::Container::Horizontal({nav_renderer, workspace_renderer, context_renderer});
    auto component_with_events = ftxui::CatchEvent(main_container, [state](ftxui::Event event) {
        return route_workspace_event(state, event);
    });

    return ftxui::Renderer(component_with_events, [state, nav_renderer, workspace_renderer, context_renderer] {
        std::vector<ftxui::Element> panels = {
            nav_renderer->Render() | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 25),
            workspace_renderer->Render() | ftxui::flex,
            context_renderer->Render() | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 25),
        };
        auto workspace_shell = ftxui::vbox({
            ftxui::hbox(panels) | ftxui::flex,
            RenderStatusBar(state),
        });
        return RenderWorkspaceOverlay(state, workspace_shell);
    });
}
