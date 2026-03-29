#include "acpre.h"
#include "project_runtime.h"
#include "ui_screens.h"
#include "workflow_controller.h"

#include <chrono>
#include <utility>

AcpreTui& AcpreTui::get_instance(void) noexcept {
    static AcpreTui instance;
    return instance;
}

AcpreTui::AcpreTui(void) noexcept {
}

AcpreTui::~AcpreTui(void) noexcept {
}

void AcpreTui::log_print(const Level level, const char *fmt, ...) {
    {
        std::lock_guard<std::mutex> lock(mLogLock);
        if (level > mMaxLevel) {
            return;
        }
    }

    char logBuf[APP_LOG_MAX];
    memset(logBuf, 0, sizeof(logBuf));

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(logBuf, sizeof(logBuf) - 1, fmt, args);
    va_end(args);
    if (len <= 0) {
        len = 0;
    }
    if ((size_t)len > (sizeof(logBuf) - 1)) {
        len = sizeof(logBuf);
        logBuf[sizeof(logBuf) - 6] = '<';
        logBuf[sizeof(logBuf) - 5] = '.';
        logBuf[sizeof(logBuf) - 4] = '.';
        logBuf[sizeof(logBuf) - 3] = '.';
        logBuf[sizeof(logBuf) - 2] = '>';
        logBuf[sizeof(logBuf) - 1] = '\n';
    }
    {
        std::lock_guard<std::mutex> lock(mLogLock);
        len = write(STDOUT_FILENO, logBuf, len);
        (void)len;
    }
}

void AppState::AddListener(std::function<void()> callback) {
    listeners.push_back(callback);
}

void AppState::NotifyListeners() {
    for (auto& listener : listeners) {
        listener();
    }
}

void AppState::run_stage_iteration() {
    if (!project || project->current_stage_index >= (int)project->stages.size()) {
        return;
    }

    ProjectStage* current_stage = nullptr;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        current_stage = &project->stages[project->current_stage_index];
    }

    std::vector<std::string> logs = {
        "开始检查当前目录中的真实工程状态",
        "汇总本阶段要处理的实际工作项",
        "整理可执行结果并刷新界面摘要",
    };

    for (size_t i = 0; i < logs.size(); ++i) {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            processing_progress = static_cast<int>((i + 1) * 100 / logs.size());
            current_stage->conversation_history.push_back(logs[i]);
        }
        NotifyListeners();
    }

    ProjectStage stage_snapshot;
    std::shared_ptr<Project> project_snapshot;
    {
        std::lock_guard<std::mutex> lock(state_mutex);
        project_snapshot = project;
        stage_snapshot = *current_stage;
    }

    auto summary = runtime::BuildStageSummary(*project_snapshot, stage_snapshot);
    stage_snapshot.execution_summary = summary;
    auto output = runtime::BuildStageOutput(*project_snapshot, stage_snapshot);
    auto acceptance_evidence = runtime::BuildAcceptanceEvidence(summary);

    {
        std::lock_guard<std::mutex> lock(state_mutex);
        current_stage->execution_summary = std::move(summary);
        current_stage->output = std::move(output);

        IterationRecord record;
        record.iteration_number = current_stage->iteration_count;
        record.action = is_analysis_stage(current_stage->name) ? "结构分析" : (is_design_stage(current_stage->name) ? "方案整理" : "构建检查");
        if (!is_execution_stage(current_stage->name)) {
            record.test_result = "待人工确认";
            record.status_icon = "🟡";
        } else if (current_stage->execution_summary.build_summary.success && current_stage->execution_summary.executor_summary.available && current_stage->execution_summary.executor_summary.success) {
            record.test_result = "验收已确认";
            record.status_icon = "🟢";
        } else if (current_stage->execution_summary.build_summary.success) {
            record.test_result = "待进一步复核";
            record.status_icon = "🟡";
        } else {
            record.test_result = "存在失败信号";
            record.status_icon = "🔴";
        }
        record.reason = current_stage->execution_summary.risk_summary;
        record.summary = current_stage->execution_summary.result_summary;
        record.next_step = current_stage->execution_summary.next_actions.empty() ? "等待验收确认" : current_stage->execution_summary.next_actions.front();
        current_stage->iteration_history.push_back(record);

        for (size_t i = 0; i < current_stage->acceptance_tests.size(); ++i) {
            auto& test = current_stage->acceptance_tests[i];
            test.passed = false;
            if (i < acceptance_evidence.size()) {
                test.notes = acceptance_evidence[i];
            } else {
                test.notes = current_stage->execution_summary.result_summary;
            }
        }

        current_stage->stage_state = StageState::REVIEW;
        if (!is_execution_stage(current_stage->name)) {
            current_stage->action_state = ActionState::IDLE;
        } else if (current_stage->execution_summary.build_summary.success && current_stage->execution_summary.executor_summary.available && current_stage->execution_summary.executor_summary.success) {
            current_stage->action_state = ActionState::SUCCESS;
        } else if (current_stage->execution_summary.build_summary.success) {
            current_stage->action_state = ActionState::PARTIAL;
        } else {
            current_stage->action_state = ActionState::FAILURE;
        }
        is_processing = false;
        processing_progress = 0;
        workflow::SyncDerivedProjectState(*project);
    }

    NotifyListeners();
}

void AppState::start_current_stage() {
    if (!project || project->current_stage_index >= (int)project->stages.size()) {
        return;
    }
    if (!workflow::PrepareCurrentStageForExecution(*project, is_processing, processing_progress)) {
        return;
    }

    is_processing = true;
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
    processing_thread = std::thread(&AppState::run_stage_iteration, this);
}

AppState::~AppState() {
    if (processing_thread.joinable()) {
        processing_thread.join();
    }
}

int AcpreTui::run(const int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    auto state = std::make_shared<AppState>();
    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto type_screen = CreateTypeSelectionScreen(state);
    auto name_screen = CreateNameInputScreen(state);
    ftxui::Component workspace_screen = nullptr;

    int tab_index = 0;
    auto main_container = ftxui::Container::Tab({type_screen, name_screen}, &tab_index);
    auto main_component = ftxui::Container::Vertical({main_container});

    auto main_renderer = ftxui::Renderer(main_component, [&] {
        if (state->screen == AppScreen::SELECT_TYPE) {
            tab_index = 0;
            return type_screen->Render();
        }
        if (state->screen == AppScreen::INPUT_NAME) {
            tab_index = 1;
            return name_screen->Render();
        }
        if (!workspace_screen) {
            workspace_screen = CreateMainWorkspace(state);
        }
        return workspace_screen->Render();
    });

    auto event_router = ftxui::CatchEvent(main_renderer, [&](ftxui::Event event) {
        if (event == ftxui::Event::CtrlQ) {
            screen.Exit();
            return true;
        }
        if (state->screen == AppScreen::SELECT_TYPE) {
            return type_screen->OnEvent(event);
        }
        if (state->screen == AppScreen::INPUT_NAME) {
            return name_screen->OnEvent(event);
        }
        if (workspace_screen) {
            return workspace_screen->OnEvent(event);
        }
        return false;
    });

    std::thread([&] {
        while (state->running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            screen.PostEvent(ftxui::Event::Custom);
        }
    }).detach();

    screen.Loop(event_router);
    state->running = false;
    return 0;
}
