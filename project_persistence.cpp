#include "project_persistence.h"
#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace persistence {
namespace {

std::string project_type_to_string(ProjectType t) {
    switch (t) {
        case ProjectType::NEW_FEATURE: return "NEW_FEATURE";
        case ProjectType::BUG_FIX: return "BUG_FIX";
        case ProjectType::REFACTOR: return "REFACTOR";
    }
    return "NEW_FEATURE";
}

ProjectType project_type_from_string(const std::string& s) {
    if (s == "BUG_FIX") return ProjectType::BUG_FIX;
    if (s == "REFACTOR") return ProjectType::REFACTOR;
    return ProjectType::NEW_FEATURE;
}

std::string task_status_to_string(TaskStatus t) {
    switch (t) {
        case TaskStatus::PENDING: return "PENDING";
        case TaskStatus::IN_PROGRESS: return "IN_PROGRESS";
        case TaskStatus::REVIEW: return "REVIEW";
        case TaskStatus::COMPLETED: return "COMPLETED";
        case TaskStatus::BLOCKED: return "BLOCKED";
    }
    return "PENDING";
}

TaskStatus task_status_from_string(const std::string& s) {
    if (s == "IN_PROGRESS") return TaskStatus::IN_PROGRESS;
    if (s == "REVIEW") return TaskStatus::REVIEW;
    if (s == "COMPLETED") return TaskStatus::COMPLETED;
    if (s == "BLOCKED") return TaskStatus::BLOCKED;
    return TaskStatus::PENDING;
}

std::string stage_state_to_string(StageState t) {
    switch (t) {
        case StageState::PENDING: return "PENDING";
        case StageState::READY: return "READY";
        case StageState::RUNNING: return "RUNNING";
        case StageState::REVIEW: return "REVIEW";
        case StageState::BLOCKED: return "BLOCKED";
        case StageState::DONE: return "DONE";
    }
    return "PENDING";
}

StageState stage_state_from_string(const std::string& s) {
    if (s == "READY") return StageState::READY;
    if (s == "RUNNING") return StageState::RUNNING;
    if (s == "REVIEW") return StageState::REVIEW;
    if (s == "BLOCKED") return StageState::BLOCKED;
    if (s == "DONE") return StageState::DONE;
    return StageState::PENDING;
}

std::string action_state_to_string(ActionState t) {
    switch (t) {
        case ActionState::IDLE: return "IDLE";
        case ActionState::PROCESSING: return "PROCESSING";
        case ActionState::SUCCESS: return "SUCCESS";
        case ActionState::FAILURE: return "FAILURE";
        case ActionState::PARTIAL: return "PARTIAL";
    }
    return "IDLE";
}

ActionState action_state_from_string(const std::string& s) {
    if (s == "PROCESSING") return ActionState::PROCESSING;
    if (s == "SUCCESS") return ActionState::SUCCESS;
    if (s == "FAILURE") return ActionState::FAILURE;
    if (s == "PARTIAL") return ActionState::PARTIAL;
    return ActionState::IDLE;
}

void to_json(json& j, const AcceptanceTest& t) {
    j = json{{"description", t.description}, {"passed", t.passed}, {"notes", t.notes}};
}

void from_json(const json& j, AcceptanceTest& t) {
    j.at("description").get_to(t.description);
    j.at("passed").get_to(t.passed);
    j.at("notes").get_to(t.notes);
}

void to_json(json& j, const IterationRecord& r) {
    j = json{
        {"iteration_number", r.iteration_number}, {"action", r.action},
        {"test_result", r.test_result}, {"reason", r.reason},
        {"status_icon", r.status_icon}, {"summary", r.summary},
        {"next_step", r.next_step}
    };
}

void from_json(const json& j, IterationRecord& r) {
    j.at("iteration_number").get_to(r.iteration_number);
    j.at("action").get_to(r.action);
    j.at("test_result").get_to(r.test_result);
    j.at("reason").get_to(r.reason);
    j.at("status_icon").get_to(r.status_icon);
    j.at("summary").get_to(r.summary);
    j.at("next_step").get_to(r.next_step);
}

void to_json(json& j, const StageBuildSummary& s) {
    j = json{
        {"return_code", s.return_code}, {"headline", s.headline},
        {"details", s.details}, {"failure_lines", s.failure_lines},
        {"related_files", s.related_files}, {"command_ran", s.command_ran},
        {"success", s.success}
    };
}

void from_json(const json& j, StageBuildSummary& s) {
    j.at("return_code").get_to(s.return_code);
    j.at("headline").get_to(s.headline);
    j.at("details").get_to(s.details);
    j.at("failure_lines").get_to(s.failure_lines);
    j.at("related_files").get_to(s.related_files);
    j.at("command_ran").get_to(s.command_ran);
    j.at("success").get_to(s.success);
}

void to_json(json& j, const StageExecutorSummary& s) {
    j = json{
        {"available", s.available}, {"attempted", s.attempted},
        {"return_code", s.return_code}, {"command", s.command},
        {"prompt", s.prompt}, {"headline", s.headline},
        {"details", s.details}, {"failure_lines", s.failure_lines},
        {"success", s.success}
    };
}

void from_json(const json& j, StageExecutorSummary& s) {
    j.at("available").get_to(s.available);
    j.at("attempted").get_to(s.attempted);
    j.at("return_code").get_to(s.return_code);
    j.at("command").get_to(s.command);
    j.at("prompt").get_to(s.prompt);
    j.at("headline").get_to(s.headline);
    j.at("details").get_to(s.details);
    j.at("failure_lines").get_to(s.failure_lines);
    j.at("success").get_to(s.success);
}

void to_json(json& j, const StageExecutionSummary& s) {
    j = json{
        {"project_observations", s.project_observations}, {"key_files", s.key_files},
        {"implementation_notes", s.implementation_notes}, {"validation_points", s.validation_points},
        {"next_actions", s.next_actions}, {"risk_summary", s.risk_summary},
        {"result_summary", s.result_summary},
        {"executor_summary", json()}, {"build_summary", json()}
    };
    to_json(j["executor_summary"], s.executor_summary);
    to_json(j["build_summary"], s.build_summary);
}

void from_json(const json& j, StageExecutionSummary& s) {
    j.at("project_observations").get_to(s.project_observations);
    j.at("key_files").get_to(s.key_files);
    j.at("implementation_notes").get_to(s.implementation_notes);
    j.at("validation_points").get_to(s.validation_points);
    j.at("next_actions").get_to(s.next_actions);
    j.at("risk_summary").get_to(s.risk_summary);
    j.at("result_summary").get_to(s.result_summary);
    from_json(j.at("executor_summary"), s.executor_summary);
    from_json(j.at("build_summary"), s.build_summary);
}

void to_json(json& j, const Task& t) {
    j = json{
        {"name", t.name}, {"status", task_status_to_string(t.status)},
        {"iteration_count", t.iteration_count}
    };
    j["iteration_history"] = json::array();
    for (const auto& r : t.iteration_history) {
        json rj;
        to_json(rj, r);
        j["iteration_history"].push_back(rj);
    }
}

void from_json(const json& j, Task& t) {
    j.at("name").get_to(t.name);
    t.status = task_status_from_string(j.at("status").get<std::string>());
    j.at("iteration_count").get_to(t.iteration_count);
    t.iteration_history.clear();
    for (const auto& rj : j.at("iteration_history")) {
        IterationRecord r;
        from_json(rj, r);
        t.iteration_history.push_back(r);
    }
}

void to_json(json& j, const BugInfo& b) {
    j = json{
        {"description", b.description}, {"severity", b.severity},
        {"location", b.location}, {"root_cause", b.root_cause},
        {"fix_steps", b.fix_steps}, {"files_modified", b.files_modified},
        {"lines_added", b.lines_added}, {"risk_level", b.risk_level}
    };
}

void from_json(const json& j, BugInfo& b) {
    j.at("description").get_to(b.description);
    j.at("severity").get_to(b.severity);
    j.at("location").get_to(b.location);
    j.at("root_cause").get_to(b.root_cause);
    j.at("fix_steps").get_to(b.fix_steps);
    j.at("files_modified").get_to(b.files_modified);
    j.at("lines_added").get_to(b.lines_added);
    j.at("risk_level").get_to(b.risk_level);
}

void to_json(json& j, const ProjectStage& s) {
    j = json{
        {"name", s.name},
        {"stage_state", stage_state_to_string(s.stage_state)},
        {"action_state", action_state_to_string(s.action_state)},
        {"output", s.output}, {"goal", s.goal},
        {"work_items", s.work_items},
        {"iteration_count", s.iteration_count},
        {"max_iterations", s.max_iterations},
        {"conversation_history", s.conversation_history}
    };
    j["execution_summary"] = json();
    to_json(j["execution_summary"], s.execution_summary);
    j["acceptance_tests"] = json::array();
    for (const auto& t : s.acceptance_tests) {
        json tj;
        to_json(tj, t);
        j["acceptance_tests"].push_back(tj);
    }
    j["iteration_history"] = json::array();
    for (const auto& r : s.iteration_history) {
        json rj;
        to_json(rj, r);
        j["iteration_history"].push_back(rj);
    }
}

void from_json(const json& j, ProjectStage& s) {
    j.at("name").get_to(s.name);
    s.stage_state = stage_state_from_string(j.at("stage_state").get<std::string>());
    s.action_state = action_state_from_string(j.at("action_state").get<std::string>());
    j.at("output").get_to(s.output);
    j.at("goal").get_to(s.goal);
    j.at("work_items").get_to(s.work_items);
    j.at("iteration_count").get_to(s.iteration_count);
    j.at("max_iterations").get_to(s.max_iterations);
    j.at("conversation_history").get_to(s.conversation_history);
    from_json(j.at("execution_summary"), s.execution_summary);
    s.acceptance_tests.clear();
    for (const auto& tj : j.at("acceptance_tests")) {
        AcceptanceTest t;
        from_json(tj, t);
        s.acceptance_tests.push_back(t);
    }
    s.iteration_history.clear();
    for (const auto& rj : j.at("iteration_history")) {
        IterationRecord r;
        from_json(rj, r);
        s.iteration_history.push_back(r);
    }
}

json project_to_json(const Project& p) {
    json j;
    j["name"] = p.name;
    j["type"] = project_type_to_string(p.type);
    j["working_directory"] = p.working_directory;
    j["current_stage_index"] = p.current_stage_index;
    j["stages"] = json::array();
    for (const auto& s : p.stages) {
        json sj;
        to_json(sj, s);
        j["stages"].push_back(sj);
    }
    j["tasks"] = json::array();
    for (const auto& t : p.tasks) {
        json tj;
        to_json(tj, t);
        j["tasks"].push_back(tj);
    }
    json bj;
    to_json(bj, p.bug_info);
    j["bug_info"] = bj;
    return j;
}

void project_from_json(const json& j, Project& p) {
    j.at("name").get_to(p.name);
    p.type = project_type_from_string(j.at("type").get<std::string>());
    j.at("working_directory").get_to(p.working_directory);
    j.at("current_stage_index").get_to(p.current_stage_index);
    p.stages.clear();
    for (const auto& sj : j.at("stages")) {
        ProjectStage s;
        from_json(sj, s);
        p.stages.push_back(s);
    }
    p.tasks.clear();
    for (const auto& tj : j.at("tasks")) {
        Task t;
        from_json(tj, t);
        p.tasks.push_back(t);
    }
    from_json(j.at("bug_info"), p.bug_info);
}

}  // anonymous namespace

std::string DefaultSavePath(const std::string& working_directory) {
    return working_directory + "/.acpre_state.json";
}

bool SaveFileExists(const std::string& working_directory) {
    std::error_code ec;
    return fs::exists(DefaultSavePath(working_directory), ec);
}

bool SaveProject(const Project& project, const std::string& path) {
    try {
        json j = project_to_json(project);
        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << j.dump(2);
        return out.good();
    } catch (...) {
        return false;
    }
}

bool LoadProject(Project& project, const std::string& path) {
    try {
        std::ifstream in(path);
        if (!in.is_open()) return false;
        json j = json::parse(in);
        project_from_json(j, project);
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace persistence