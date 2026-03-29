#include "domain.h"

#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

std::string ProjectTypeToString(ProjectType type) {
    switch (type) {
        case ProjectType::NEW_FEATURE: return "🔵 新功能开发";
        case ProjectType::BUG_FIX: return "🔴 缺陷修复";
        case ProjectType::REFACTOR: return "🟡 代码重构";
    }
    return "未知类型";
}

std::string StageStateIcon(StageState state) {
    switch (state) {
        case StageState::DONE: return "✅";
        case StageState::RUNNING: return "⏳";
        case StageState::BLOCKED: return "⛔";
        case StageState::REVIEW: return "🔎";
        case StageState::READY: return "▶";
        case StageState::PENDING:
        default: return "⏸️";
    }
}

ftxui::Color StageStateColor(StageState state) {
    switch (state) {
        case StageState::DONE: return ftxui::Color::Green;
        case StageState::RUNNING: return ftxui::Color::Yellow;
        case StageState::BLOCKED: return ftxui::Color::Red;
        case StageState::REVIEW: return ftxui::Color::Cyan;
        default: return ftxui::Color::White;
    }
}

std::string ActionStateLabel(ActionState state) {
    switch (state) {
        case ActionState::PROCESSING: return "执行中";
        case ActionState::SUCCESS: return "成功";
        case ActionState::FAILURE: return "失败";
        case ActionState::PARTIAL: return "部分成功";
        case ActionState::IDLE:
        default: return "待执行";
    }
}

std::string ActionStateIcon(ActionState state) {
    switch (state) {
        case ActionState::PROCESSING: return "⏳";
        case ActionState::SUCCESS: return "✅";
        case ActionState::FAILURE: return "❌";
        case ActionState::PARTIAL: return "🟡";
        case ActionState::IDLE:
        default: return "⏸️";
    }
}

std::string ReviewActionLabel(ActionState state, bool can_complete) {
    if (can_complete) {
        switch (state) {
            case ActionState::SUCCESS: return "验收与复核已完成";
            case ActionState::PARTIAL: return "验收已完成，建议复核";
            case ActionState::FAILURE: return "验收已完成，存在失败信号";
            default: return "可完成";
        }
    }

    switch (state) {
        case ActionState::FAILURE: return "仍有失败信号，需继续排查";
        case ActionState::PROCESSING: return "仍在执行中";
        default: return "等待验收确认";
    }
}

std::string ReviewStatusColorLabel(ActionState state, bool can_complete) {
    if (can_complete) {
        switch (state) {
            case ActionState::SUCCESS: return "已可完成";
            case ActionState::PARTIAL: return "可完成，建议复核";
            case ActionState::FAILURE: return "可完成，存在失败信号";
            default: return "可完成";
        }
    }

    switch (state) {
        case ActionState::FAILURE: return "复核中有失败信号";
        case ActionState::PROCESSING: return "执行中";
        default: return "等待验收确认";
    }
}

ftxui::Color ActionStateColor(ActionState state) {
    switch (state) {
        case ActionState::SUCCESS: return ftxui::Color::Green;
        case ActionState::PROCESSING:
        case ActionState::PARTIAL: return ftxui::Color::Yellow;
        case ActionState::FAILURE: return ftxui::Color::Red;
        case ActionState::IDLE:
        default: return ftxui::Color::White;
    }
}

FocusRegion NextFocusRegion(FocusRegion region) {
    switch (region) {
        case FocusRegion::PROJECT_TREE: return FocusRegion::WORKBENCH;
        case FocusRegion::WORKBENCH: return FocusRegion::INFO_PANEL;
        case FocusRegion::INFO_PANEL: return FocusRegion::STATUS_BAR;
        case FocusRegion::STATUS_BAR:
        case FocusRegion::OVERLAY:
        default: return FocusRegion::PROJECT_TREE;
    }
}

FocusRegion PreviousFocusRegion(FocusRegion region) {
    switch (region) {
        case FocusRegion::PROJECT_TREE: return FocusRegion::STATUS_BAR;
        case FocusRegion::WORKBENCH: return FocusRegion::PROJECT_TREE;
        case FocusRegion::INFO_PANEL: return FocusRegion::WORKBENCH;
        case FocusRegion::STATUS_BAR: return FocusRegion::INFO_PANEL;
        case FocusRegion::OVERLAY:
        default: return FocusRegion::WORKBENCH;
    }
}

std::vector<AcceptanceTest> generate_default_acceptance_tests(const std::string& stage_name) {
    std::vector<AcceptanceTest> tests;
    if (stage_name == "需求讨论" || stage_name == "问题定义" || stage_name == "重构分析") {
        tests.push_back({"需求文档已生成", false, ""});
        tests.push_back({"需求已明确且无歧义", false, ""});
        tests.push_back({"验收标准已定义", false, ""});
    } else if (stage_name == "方案设计" || stage_name == "修复方案") {
        tests.push_back({"设计文档已生成", false, ""});
        tests.push_back({"技术方案已评审", false, ""});
        tests.push_back({"实现路径已明确", false, ""});
    } else {
        tests.push_back({"代码已实现", false, ""});
        tests.push_back({"单元测试已确认", false, ""});
        tests.push_back({"集成测试已确认", false, ""});
        tests.push_back({"符合设计文档要求", false, ""});
    }
    return tests;
}

bool is_analysis_stage(const std::string& stage_name) {
    return stage_name == "需求讨论" || stage_name == "问题定义" || stage_name == "重构分析";
}

bool is_design_stage(const std::string& stage_name) {
    return stage_name == "方案设计" || stage_name == "修复方案";
}

bool is_execution_stage(const std::string& stage_name) {
    return !is_analysis_stage(stage_name) && !is_design_stage(stage_name);
}

Project::Project(std::string n, ProjectType t)
    : name(std::move(n)), type(t), working_directory(fs::current_path().string()) {
    if (type == ProjectType::NEW_FEATURE) {
        stages = {
            {"需求讨论", StageState::READY, ActionState::IDLE, "", "梳理当前目录中的项目结构、构建方式和可以实际实现的新功能范围", {"扫描当前目录下的源码与构建文件", "总结已有程序能力与限制", "形成可执行的功能目标"}},
            {"方案设计", StageState::PENDING, ActionState::IDLE, "", "给出基于当前代码的实际实施方案与验收标准", {"列出受影响文件", "制定实现步骤", "将需求映射到验收测试"}},
            {"任务迭代", StageState::PENDING, ActionState::IDLE, "", "围绕当前目录下的真实代码分批实现并复核功能", {"更新核心代码", "构建程序", "记录复核结果"}},
        };
        tasks = {
            {"任务1: 分析当前代码结构", TaskStatus::PENDING, 0, {}},
            {"任务2: 明确功能边界", TaskStatus::PENDING, 0, {}},
            {"任务3: 修改核心逻辑", TaskStatus::PENDING, 0, {}},
            {"任务4: 构建并复核程序", TaskStatus::PENDING, 0, {}},
            {"任务5: 完成验收清单", TaskStatus::PENDING, 0, {}},
        };
    } else if (type == ProjectType::BUG_FIX) {
        stages = {
            {"问题定义", StageState::READY, ActionState::IDLE, "", "定位当前目录下程序的真实问题与受影响文件", {"检查关键源码文件", "总结可复现问题", "确认修改范围"}},
            {"修复方案", StageState::PENDING, ActionState::IDLE, "", "制定可在当前项目中直接落地的修复步骤", {"说明根因", "列出排查与修复操作", "定义回归检查项"}},
            {"执行迭代", StageState::PENDING, ActionState::IDLE, "", "在当前目录的真实代码上实施修复并复核", {"修改代码", "构建程序", "记录复核结果"}},
        };
        bug_info.description = "等待基于当前目录定位实际问题与影响范围";
        bug_info.severity = "中";
        bug_info.location = "待分析";
        bug_info.root_cause = "等待结合当前目录中的真实代码与构建结果完成根因分析";
        bug_info.fix_steps = {
            "1. 基于当前目录收集真实项目上下文",
            "2. 明确问题位置、影响范围与回归检查项",
            "3. 结合构建检查与验收确认推进修复阶段",
        };
        bug_info.files_modified = 0;
        bug_info.lines_added = 0;
        bug_info.risk_level = "中";
    } else {
        stages = {
            {"重构分析", StageState::READY, ActionState::IDLE, "", "分析当前目录下适合重构的真实代码区域", {"识别复杂逻辑", "评估影响范围", "制定边界"}},
            {"方案设计", StageState::PENDING, ActionState::IDLE, "", "输出实际重构步骤与验收条件", {"明确目标结构", "控制修改范围", "定义复核方式"}},
            {"代码迭代", StageState::PENDING, ActionState::IDLE, "", "在现有代码上逐步重构并完成构建检查", {"调整实现", "构建检查", "确认结果"}},
        };
    }

    for (auto& stage : stages) {
        stage.acceptance_tests = generate_default_acceptance_tests(stage.name);
    }
}
