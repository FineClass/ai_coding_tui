#include "project_runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <utility>

namespace fs = std::filesystem;

namespace runtime {
namespace {

std::vector<std::string> SampleEntries(const fs::path& root, bool want_directories) {
    std::vector<std::string> entries;
    std::error_code ec;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return entries;
    }

    for (const auto& entry : fs::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (want_directories && entry.is_directory(ec)) {
            entries.push_back(entry.path().filename().string());
        }
        if (!want_directories && entry.is_regular_file(ec)) {
            entries.push_back(entry.path().filename().string());
        }
    }

    std::sort(entries.begin(), entries.end());
    if (entries.size() > 8) {
        entries.resize(8);
    }
    return entries;
}

std::string JoinLines(const std::vector<std::string>& lines, const std::string& prefix = "- ") {
    if (lines.empty()) {
        return prefix + std::string("无");
    }

    std::string output;
    for (size_t i = 0; i < lines.size(); ++i) {
        output += prefix + lines[i];
        if (i + 1 < lines.size()) {
            output += "\n";
        }
    }
    return output;
}

int RunCommandCapture(const std::string& command, std::string& output) {
    output.clear();
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return -1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
        if (output.size() > 4000) {
            output.resize(4000);
            break;
        }
    }

    return pclose(pipe);
}

std::string ShellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted += "'";
    return quoted;
}

bool CommandExists(const std::string& command) {
    std::string output;
    return RunCommandCapture("command -v " + command + " >/dev/null 2>&1", output) == 0;
}

std::string JoinPromptLines(const std::vector<std::string>& lines) {
    std::string output;
    for (size_t i = 0; i < lines.size(); ++i) {
        output += lines[i];
        if (i + 1 < lines.size()) {
            output += "\n";
        }
    }
    return output;
}

std::string Trim(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string FirstLine(const std::string& text) {
    auto pos = text.find('\n');
    if (pos == std::string::npos) {
        return Trim(text);
    }
    return Trim(text.substr(0, pos));
}

std::map<std::string, int> CollectExtensionCounts(const fs::path& root) {
    std::map<std::string, int> counts;
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return counts;
    }

    size_t visited = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto filename = entry.path().filename().string();
        if (filename == ".git" || filename == "acpre_tui") {
            continue;
        }
        auto ext = entry.path().extension().string();
        if (ext.empty()) {
            ext = "[无扩展名]";
        }
        counts[ext]++;
        visited++;
        if (visited >= 400) {
            break;
        }
    }
    return counts;
}

std::vector<std::string> SummarizeExtensions(const std::map<std::string, int>& counts) {
    std::vector<std::pair<std::string, int>> items(counts.begin(), counts.end());
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });

    std::vector<std::string> summary;
    for (size_t i = 0; i < items.size() && i < 6; ++i) {
        summary.push_back(items[i].first + " × " + std::to_string(items[i].second));
    }
    return summary;
}

std::vector<std::string> CollectCandidateFiles(const fs::path& root) {
    std::vector<std::pair<int, std::string>> ranked_files;
    std::set<std::string> seen;
    std::error_code ec;
    if (!fs::exists(root, ec)) {
        return {};
    }

    auto score_file = [](const std::string& relative_path, const std::string& filename, const std::string& ext) {
        int score = 0;
        if (filename == "main.cpp" || filename == "main.c" || filename == "main.py" || filename == "app.py" || filename == "index.js") score += 120;
        if (filename == "Makefile" || filename == "CMakeLists.txt" || filename == "package.json") score += 110;
        if (relative_path.find("src/") != std::string::npos) score += 70;
        if (relative_path.find("app/") != std::string::npos) score += 60;
        if (relative_path.find("core/") != std::string::npos) score += 55;
        if (relative_path.find("include/") != std::string::npos) score += 45;
        if (filename.find("main") != std::string::npos) score += 35;
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c") score += 25;
        if (ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".tsx" || ext == ".rs" || ext == ".go" || ext == ".java") score += 20;
        return score;
    };

    size_t visited = 0;
    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (ec) {
            break;
        }
        if (!entry.is_regular_file(ec)) {
            continue;
        }

        const auto path = entry.path();
        const auto filename = path.filename().string();
        const auto ext = path.extension().string();
        if (filename == "acpre_tui" || filename == ".gitignore") {
            continue;
        }

        bool interesting = ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cc" ||
                           ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".tsx" || ext == ".rs" ||
                           ext == ".go" || ext == ".java" || filename == "Makefile" || filename == "CMakeLists.txt" ||
                           filename == "package.json";
        if (!interesting) {
            visited++;
            if (visited >= 220) {
                break;
            }
            continue;
        }

        std::string relative = fs::relative(path, root, ec).string();
        if (!seen.insert(relative).second) {
            continue;
        }
        ranked_files.push_back({score_file(relative, filename, ext), relative});

        visited++;
        if (visited >= 220) {
            break;
        }
    }

    std::sort(ranked_files.begin(), ranked_files.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) return a.first > b.first;
        return a.second < b.second;
    });

    std::vector<std::string> files;
    for (size_t i = 0; i < ranked_files.size() && i < 8; ++i) {
        files.push_back(ranked_files[i].second);
    }
    return files;
}

std::vector<std::string> CollectCoreDirectories(const std::vector<std::string>& files) {
    std::vector<std::string> dirs;
    std::set<std::string> seen;
    for (const auto& file : files) {
        auto pos = file.find('/');
        if (pos == std::string::npos) {
            continue;
        }
        auto dir = file.substr(0, pos);
        if (seen.insert(dir).second) {
            dirs.push_back(dir);
        }
        if (dirs.size() >= 4) {
            break;
        }
    }
    return dirs;
}

std::vector<std::string> CollectBuildFiles(const std::vector<std::string>& files) {
    std::vector<std::string> result;
    for (const auto& file : files) {
        if (file == "Makefile" || file == "CMakeLists.txt" || file == "package.json") {
            result.push_back(file);
        }
    }
    return result;
}

void AppendUniqueLine(std::vector<std::string>& target, const std::string& line) {
    auto trimmed = Trim(line);
    if (trimmed.empty()) {
        return;
    }
    if (std::find(target.begin(), target.end(), trimmed) == target.end()) {
        target.push_back(trimmed);
    }
}

void AppendLines(std::vector<std::string>& target, const std::vector<std::string>& lines, const std::string& prefix = "") {
    for (const auto& line : lines) {
        AppendUniqueLine(target, prefix.empty() ? line : prefix + line);
    }
}

std::vector<std::string> SummarizeCommandOutput(const std::string& text, size_t max_lines = 3) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (!line.empty()) {
            lines.push_back(line);
        }
        if (lines.size() >= max_lines) {
            break;
        }
    }
    return lines;
}

std::vector<std::string> ExtractFailureLines(const std::string& text, size_t max_lines = 4) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        auto trimmed = Trim(line);
        if (trimmed.empty()) {
            continue;
        }

        bool looks_like_error = trimmed.find("error") != std::string::npos ||
                                trimmed.find("Error") != std::string::npos ||
                                trimmed.find("undefined reference") != std::string::npos ||
                                trimmed.find("No such file") != std::string::npos ||
                                trimmed.find("未定义") != std::string::npos ||
                                trimmed.find("失败") != std::string::npos ||
                                trimmed.find("fatal") != std::string::npos;
        if (looks_like_error) {
            lines.push_back(trimmed);
        }
        if (lines.size() >= max_lines) {
            break;
        }
    }
    return lines;
}

std::vector<std::string> ExtractRelatedFilesFromOutput(const std::string& text, size_t max_files = 4) {
    std::vector<std::string> files;
    std::set<std::string> seen;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        std::string cleaned;
        for (char ch : token) {
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '_' || ch == '/' || ch == '-') {
                cleaned.push_back(ch);
            }
        }
        if (cleaned.empty()) {
            continue;
        }
        bool looks_like_file = cleaned.find(".cpp") != std::string::npos || cleaned.find(".h") != std::string::npos ||
                               cleaned.find(".c") != std::string::npos || cleaned.find(".hpp") != std::string::npos ||
                               cleaned.find("Makefile") != std::string::npos || cleaned.find("CMakeLists.txt") != std::string::npos;
        if (looks_like_file && seen.insert(cleaned).second) {
            files.push_back(cleaned);
        }
        if (files.size() >= max_files) {
            break;
        }
    }
    return files;
}

std::string DetectEntryHint(const std::vector<std::string>& files) {
    for (const auto& file : files) {
        if (file == "main.cpp" || file == "main.c" || file == "app.py" || file == "index.js") {
            return file;
        }
    }
    if (!files.empty()) {
        return files.front();
    }
    return "未识别到明显入口文件";
}

std::string BuildExecutorPrompt(const Project& project, ProjectStage& stage, const std::vector<std::string>& candidate_files) {
    std::vector<std::string> lines;
    lines.push_back("你正在当前工作目录内执行真实代码任务，请直接修改本地代码，而不是只输出建议。");
    lines.push_back("项目名称: " + project.name);
    lines.push_back("阶段: " + stage.name);
    lines.push_back("阶段目标: " + stage.goal);
    lines.push_back("项目类型: " + ProjectTypeToString(project.type));
    lines.push_back("工作目录: " + project.working_directory);
    lines.push_back("当前工作项:");
    for (const auto& item : stage.work_items) {
        lines.push_back("- " + item);
    }
    if (!candidate_files.empty()) {
        lines.push_back("优先关注文件:");
        for (const auto& file : candidate_files) {
            lines.push_back("- " + file);
        }
    }
    lines.push_back("要求:");
    lines.push_back("- 直接在当前目录内完成必要的代码修改");
    lines.push_back("- 修改后运行适合当前项目的构建或检查命令");
    lines.push_back("- 输出简短结果摘要、修改文件、复核结论和下一步");
    if (project.type == ProjectType::NEW_FEATURE) {
        lines.push_back("本阶段目的是真正落地功能或优化，而不是做模拟演示。");
    } else if (project.type == ProjectType::BUG_FIX) {
        lines.push_back("本阶段目的是真正修复当前目录中的问题，并给出回归检查结论。");
    } else {
        lines.push_back("本阶段目的是真正完成代码重构，并确认行为未被破坏。");
    }
    return JoinPromptLines(lines);
}

StageExecutorSummary RunStageExecutor(const Project& project, ProjectStage& stage, const std::vector<std::string>& candidate_files) {
    StageExecutorSummary summary;
    summary.available = CommandExists("claude");
    summary.prompt = BuildExecutorPrompt(project, stage, candidate_files);
    if (!summary.available) {
        summary.headline = "未检测到 claude 命令，执行阶段将只保留本地构建检查。";
        summary.failure_lines.push_back("命令缺失: claude");
        return summary;
    }

    summary.attempted = true;
    summary.command = "cd " + ShellQuote(project.working_directory) + " && claude -p " + ShellQuote(summary.prompt) + " 2>&1";
    std::string executor_output;
    summary.return_code = RunCommandCapture(summary.command, executor_output);
    summary.details = SummarizeCommandOutput(executor_output, 6);
    summary.failure_lines = ExtractFailureLines(executor_output, 4);
    summary.success = summary.return_code == 0;
    summary.headline = FirstLine(executor_output);
    if (summary.headline.empty()) {
        summary.headline = summary.success ? "Claude Code 已执行完成" : "Claude Code 执行失败";
    }
    if (!summary.success && summary.failure_lines.empty()) {
        summary.failure_lines = SummarizeCommandOutput(executor_output, 3);
    }
    return summary;
}

}  // namespace

StageExecutionSummary BuildStageSummary(const Project& project, ProjectStage& stage) {
    StageExecutionSummary summary;
    const fs::path root(project.working_directory);
    auto dirs = SampleEntries(root, true);
    auto files = SampleEntries(root, false);
    auto extensions = SummarizeExtensions(CollectExtensionCounts(root));
    auto candidate_files = CollectCandidateFiles(root);
    auto build_files = CollectBuildFiles(candidate_files);
    auto core_dirs = CollectCoreDirectories(candidate_files);
    std::string entry_hint = DetectEntryHint(candidate_files);

    std::string preview_output;
    int preview_rc = RunCommandCapture("cd " + ShellQuote(project.working_directory) + " && make -n 2>&1", preview_output);
    auto preview_lines = SummarizeCommandOutput(preview_output, 3);
    auto preview_failures = ExtractFailureLines(preview_output, 3);
    std::string preview_headline = preview_rc == 0 ? FirstLine(preview_output) : "未检测到可用的 make 预览输出";

    summary.project_observations.push_back("工作目录: " + project.working_directory);
    summary.project_observations.push_back("入口线索: " + entry_hint);
    summary.project_observations.push_back("构建文件: " + (build_files.empty() ? std::string("未识别") : build_files.front() + (build_files.size() > 1 ? " 等" : "")));
    summary.project_observations.push_back("核心目录: " + (core_dirs.empty() ? std::string("未识别") : core_dirs.front() + (core_dirs.size() > 1 ? " 等" : "")));
    summary.project_observations.push_back("子目录概览: " + (dirs.empty() ? std::string("无") : dirs.front() + (dirs.size() > 1 ? " 等" : "")));
    summary.project_observations.push_back("顶层文件: " + (files.empty() ? std::string("无") : files.front() + (files.size() > 1 ? " 等" : "")));
    summary.project_observations.push_back(std::string("构建预览: ") + (preview_rc == 0 ? "可用" : "不可用"));
    summary.key_files = candidate_files.empty() ? files : candidate_files;

    summary.build_summary.return_code = preview_rc;
    summary.build_summary.command_ran = true;
    summary.build_summary.success = preview_rc == 0;
    summary.build_summary.headline = preview_headline;
    summary.build_summary.details = preview_lines;
    summary.build_summary.failure_lines = preview_failures;

    if (is_analysis_stage(stage.name)) {
        summary.implementation_notes.push_back("源码类型分布: " + (extensions.empty() ? std::string("无") : extensions.front()));
        if (extensions.size() > 1) {
            summary.implementation_notes.push_back("主要技术栈延伸: " + extensions[1]);
        }
        if (!build_files.empty()) {
            summary.implementation_notes.push_back("优先围绕构建入口理解工程: " + build_files.front());
        }
        if (!core_dirs.empty()) {
            summary.implementation_notes.push_back("优先关注核心目录: " + core_dirs.front());
        }
        summary.implementation_notes.push_back("构建入口预览: " + preview_headline);
        summary.validation_points = stage.work_items;
        AppendLines(summary.validation_points, preview_lines, "构建预览: ");
        summary.next_actions = {
            "确定本阶段要关注的关键文件和模块",
            "把目录观察结果整理成可执行目标",
            "为下一阶段补齐实现范围与验收项",
        };
        summary.risk_summary = "风险较低，主要风险在于需求边界仍不够具体。";
        summary.result_summary = "已完成当前工程结构、构建方式和入口线索的真实分析。";
    } else if (is_design_stage(stage.name)) {
        summary.implementation_notes = stage.work_items;
        if (!build_files.empty()) {
            summary.implementation_notes.push_back("优先对齐构建文件: " + build_files.front());
        }
        if (!candidate_files.empty()) {
            summary.implementation_notes.push_back("优先从关键文件切入: " + candidate_files.front());
        }
        summary.implementation_notes.push_back("优先围绕现有状态机和界面布局做增量修改");
        summary.validation_points = {
            "阶段输出按分析/设计/执行语义区分",
            "任务树或问题面板可映射真实工作项",
            "验收说明来自真实扫描与构建结果",
        };
        AppendLines(summary.validation_points, preview_lines, "构建预览: ");
        summary.next_actions = {
            "优先修改关键文件并保留现有快捷键交互",
            "执行阶段时输出构建摘要和下一步动作",
            "将结构化结果同步到历史视图和复核视图",
        };
        summary.risk_summary = "主要风险在于展示信息不一致，需要同步更新左右栏和历史记录。";
        summary.result_summary = "已形成可直接落地到当前代码结构的实施方案。";
    } else {
        summary.executor_summary = RunStageExecutor(project, stage, candidate_files);
        std::string build_output;
        int build_rc = RunCommandCapture("cd " + ShellQuote(project.working_directory) + " && make 2>&1", build_output);
        auto build_lines = SummarizeCommandOutput(build_output, 4);
        auto failure_lines = ExtractFailureLines(build_output, 4);
        auto related_files = ExtractRelatedFilesFromOutput(build_output, 4);
        std::string build_headline = FirstLine(build_output);
        if (build_headline.empty()) {
            build_headline = build_rc == 0 ? "构建完成" : "构建失败";
        }

        summary.implementation_notes = stage.work_items;
        if (summary.executor_summary.available) {
            summary.implementation_notes.push_back("已触发 Claude Code 执行真实代码修改任务");
            summary.implementation_notes.push_back("执行摘要: " + summary.executor_summary.headline);
        } else {
            summary.implementation_notes.push_back("未检测到 Claude Code 命令，当前仅执行本地构建检查");
        }
        summary.validation_points = {
            "确认执行器是否真实触发代码修改",
            "确认构建命令真实执行",
            "提炼关键输出供人工验收",
            "记录关键信号或下一步建议",
        };
        AppendLines(summary.validation_points, summary.executor_summary.details, "执行器输出: ");
        AppendLines(summary.validation_points, build_lines, "构建输出: ");
        AppendLines(summary.validation_points, failure_lines, "失败信号: ");
        AppendLines(summary.validation_points, related_files, "相关文件: ");
        if (build_rc == 0) {
            summary.next_actions = {"根据验收清单逐项确认结果", "检查关键文件是否反映预期变化", "确认是否可以推进下一阶段"};
            if (summary.executor_summary.available && summary.executor_summary.success) {
                summary.result_summary = "已触发 Claude Code 修改当前目录代码，并完成真实构建检查。";
                summary.risk_summary = "当前风险可控，后续重点是人工验收修改结果。";
            } else if (summary.executor_summary.available && !summary.executor_summary.success) {
                summary.result_summary = "Claude Code 已触发但执行未完全成功，构建仍已真实执行。";
                summary.risk_summary = "执行器阶段存在失败信号，需要核对执行输出与实际改动。";
            } else {
                summary.result_summary = "已基于当前目录真实执行构建检查，但未接入 Claude Code 自动修改。";
                summary.risk_summary = "当前构建已完成，但执行链路降级为本地检查，仍需人工核对修改是否完整。";
            }
        } else {
            summary.next_actions = {
                failure_lines.empty() ? "查看构建摘要中的关键信号" : "优先处理报错: " + failure_lines.front(),
                related_files.empty() ? "定位相关文件后再重试" : "优先检查相关文件: " + related_files.front(),
                summary.executor_summary.available && !summary.executor_summary.success ? "先核对 Claude Code 执行输出再继续" : "必要时人工确认风险和修改范围",
            };
            summary.risk_summary = failure_lines.empty() ? "构建未完成，需要优先处理关键信号后再继续。" : "构建失败，需先修复关键报错再继续。";
            if (summary.executor_summary.available && !summary.executor_summary.success) {
                summary.risk_summary += " Claude Code 执行也未完全成功。";
            }
            summary.result_summary = failure_lines.empty() ? "已执行真实构建，但当前存在失败信号。" : "已执行真实构建，并提炼出关键信号。";
            if (!related_files.empty()) {
                summary.key_files.insert(summary.key_files.begin(), related_files.begin(), related_files.end());
            }
        }
        summary.build_summary.return_code = build_rc;
        summary.build_summary.command_ran = true;
        summary.build_summary.success = build_rc == 0;
        summary.build_summary.headline = build_headline;
        summary.build_summary.details = build_lines;
        summary.build_summary.failure_lines = failure_lines;
        summary.build_summary.related_files = related_files;
    }

    return summary;
}

std::vector<std::string> BuildAcceptanceEvidence(const StageExecutionSummary& summary) {
    std::vector<std::string> evidence;
    AppendLines(evidence, summary.validation_points);
    AppendLines(evidence, summary.executor_summary.failure_lines, "执行器关键信号: ");
    AppendLines(evidence, summary.executor_summary.details, "执行器输出: ");
    AppendLines(evidence, summary.build_summary.failure_lines, "构建关键信号: ");
    AppendLines(evidence, summary.build_summary.details, "构建输出: ");
    AppendLines(evidence, summary.build_summary.related_files, "相关文件: ");
    if (!summary.result_summary.empty()) {
        AppendUniqueLine(evidence, "结果摘要: " + summary.result_summary);
    }
    if (!summary.risk_summary.empty()) {
        AppendUniqueLine(evidence, "风险提示: " + summary.risk_summary);
    }
    return evidence;
}

std::string BuildStageOutput(const Project& project, const ProjectStage& stage) {
    const auto& summary = stage.execution_summary;
    std::string output = "工作目录: " + project.working_directory + "\n";
    output += "阶段目标: " + stage.goal + "\n\n";
    output += "结果摘要: " + summary.result_summary + "\n\n";
    output += "项目观察\n" + JoinLines(summary.project_observations, "  - ") + "\n\n";
    output += "关键文件\n" + JoinLines(summary.key_files, "  - ") + "\n\n";
    output += "实施说明\n" + JoinLines(summary.implementation_notes, "  - ") + "\n\n";
    output += "复核重点\n" + JoinLines(summary.validation_points, "  - ") + "\n\n";
    if (summary.executor_summary.available || summary.executor_summary.attempted) {
        output += "执行器摘要\n";
        output += "  - 命令可用: " + std::string(summary.executor_summary.available ? "是" : "否") + "\n";
        if (!summary.executor_summary.command.empty()) {
            output += "  - 执行命令: " + summary.executor_summary.command + "\n";
        }
        output += "  - 结论: " + summary.executor_summary.headline + "\n";
        if (!summary.executor_summary.details.empty()) {
            output += JoinLines(summary.executor_summary.details, "  - 关键输出: ") + "\n";
        }
        if (!summary.executor_summary.failure_lines.empty()) {
            output += JoinLines(summary.executor_summary.failure_lines, "  - 关键信号: ") + "\n";
        }
        output += "\n";
    }
    if (summary.build_summary.command_ran) {
        output += "构建摘要\n";
        output += "  - 返回状态: " + std::to_string(summary.build_summary.return_code) + "\n";
        output += "  - 结论: " + summary.build_summary.headline + "\n";
        output += JoinLines(summary.build_summary.details, "  - 关键输出: ") + "\n";
        if (!summary.build_summary.failure_lines.empty()) {
            output += JoinLines(summary.build_summary.failure_lines, "  - 关键信号: ") + "\n";
        }
        if (!summary.build_summary.related_files.empty()) {
            output += JoinLines(summary.build_summary.related_files, "  - 相关文件: ") + "\n";
        }
        output += "\n";
    }
    output += "下一步\n" + JoinLines(summary.next_actions, "  - ");
    return output;
}

}  // namespace runtime
