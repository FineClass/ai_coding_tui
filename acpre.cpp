#include "acpre.h"

AcpreTui& AcpreTui::get_instance(void) noexcept
{
   /* C++11保证了静态局部对象是线程安全的 */
    static AcpreTui instance;
    return instance; 
}

AcpreTui::AcpreTui(void) noexcept
{
}

AcpreTui::~AcpreTui(void) noexcept
{
}

void AcpreTui::log_print(const Level level, const char *fmt, ...)
{
	{
		std::lock_guard<std::mutex> lock(mLogLock);
		if(level > mMaxLevel) {
			return;
		}
	}

	char logBuf[APP_LOG_MAX];
    memset(logBuf, 0, sizeof(logBuf));

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(logBuf, sizeof(logBuf) - 1, fmt, args);
    va_end(args);
    if(len <= 0) {
        len = 0;
    }
    if((size_t)len > (sizeof(logBuf) - 1)) {
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
    }
}

// ============================================================================
// 1. 数据模型 (Data Models)
// ============================================================================

enum class ProjectType {
    NEW_FEATURE,
    BUG_FIX,
    REFACTOR
};

std::string ProjectTypeToString(ProjectType type) {
    switch (type) {
        case ProjectType::NEW_FEATURE: return "🔵 新功能开发";
        case ProjectType::BUG_FIX:     return "🔴 缺陷修复";
        case ProjectType::REFACTOR:    return "🟡 代码重构";
    }
    return "未知类型";
}

struct ProjectStage {
    std::string name;
    std::string status; // "pending", "running", "done"
    std::string output;

    // New fields for iteration management
    int iteration_count = 0;
    int max_iterations = 5;
    TaskStatus task_status = TaskStatus::PENDING;
    std::vector<AcceptanceTest> acceptance_tests;
    std::vector<std::string> conversation_history;
    std::vector<IterationRecord> iteration_history;
    bool needs_human_intervention = false;
};

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
    } else { // 迭代阶段
        tests.push_back({"代码已实现", false, ""});
        tests.push_back({"单元测试已通过", false, ""});
        tests.push_back({"集成测试已通过", false, ""});
        tests.push_back({"符合设计文档要求", false, ""});
    }

    return tests;
}

struct Project {
    std::string name;
    ProjectType type;
    int current_stage_index = 0;
    std::vector<ProjectStage> stages;

    // For NEW_FEATURE: task tree
    std::vector<Task> tasks;

    // For BUG_FIX: bug information
    BugInfo bug_info;

    Project(std::string n, ProjectType t) : name(n), type(t) {
        // Type-specific workflows
        if (type == ProjectType::NEW_FEATURE) {
            stages = {
                {"需求讨论", "pending", ""},
                {"方案设计", "pending", ""},
                {"任务迭代", "pending", ""}
            };

            // Initialize task tree for NEW_FEATURE
            tasks = {
                {"任务1: 用户模型", TaskStatus::PENDING, 0, {}},
                {"任务2: 数据库设计", TaskStatus::PENDING, 0, {}},
                {"任务3: 登录接口", TaskStatus::PENDING, 0, {}},
                {"任务4: 会话管理", TaskStatus::PENDING, 0, {}},
                {"任务5: 权限控制", TaskStatus::PENDING, 0, {}}
            };

        } else if (type == ProjectType::BUG_FIX) {
            stages = {
                {"问题定义", "pending", ""},
                {"修复方案", "pending", ""},
                {"执行迭代", "pending", ""}
            };

            // Initialize bug info for BUG_FIX
            bug_info.description = "登录接口空指针异常";
            bug_info.severity = "高";
            bug_info.location = "UserService.java:45";
            bug_info.root_cause = "用户对象未判空，缺少异常处理";
            bug_info.fix_steps = {
                "1. 添加空值检查",
                "2. 添加异常捕获",
                "3. 添加单元测试"
            };
            bug_info.files_modified = 1;
            bug_info.lines_added = 25;
            bug_info.risk_level = "低";

        } else { // REFACTOR
            stages = {
                {"重构分析", "pending", ""},
                {"方案设计", "pending", ""},
                {"代码迭代", "pending", ""}
            };
        }

        // Initialize acceptance tests for each stage
        for (auto& stage : stages) {
            stage.acceptance_tests = generate_default_acceptance_tests(stage.name);
        }
    }
};

// ============================================================================
// 2. 应用状态 (App State)
// ============================================================================

enum class AppScreen {
    SELECT_TYPE,
    INPUT_NAME,
    MAIN_WORKSPACE
};

enum class WorkspaceView {
    NORMAL,
    ITERATION_HISTORY,
    GLOBAL_VALIDATION
};

class AppState {
public:
    AppScreen screen = AppScreen::SELECT_TYPE;
    WorkspaceView workspace_view = WorkspaceView::NORMAL;
    std::shared_ptr<Project> project = nullptr;
    std::string input_name = "";
    bool running = true;

    // 模拟异步任务
    bool is_processing = false;
    std::mutex state_mutex;
    std::thread processing_thread;
    int processing_progress = 0;

    std::vector<std::function<void()>> listeners;

    void AddListener(std::function<void()> callback) {
        listeners.push_back(callback);
    }

    void NotifyListeners() {
        for (auto& listener : listeners) {
            listener();
        }
    }

    // 生成阶段特定的模拟输出
    std::string generate_stage_output(const std::string& stage_name, int iteration) {
        std::string output;

        if (stage_name == "需求讨论" || stage_name == "问题定义" || stage_name == "重构分析") {
            output = "📝 分析阶段 - 迭代 " + std::to_string(iteration) + "\n\n";
            output += "1. 收集需求信息...\n";
            output += "   - 分析用户需求和业务场景\n";
            output += "   - 识别关键功能点和约束条件\n";
            output += "   - 确定技术可行性\n\n";
            output += "2. 生成需求文档...\n";
            output += "   - 功能需求: 已明确\n";
            output += "   - 非功能需求: 已定义\n";
            output += "   - 验收标准: 已制定\n\n";
            output += "3. 评审结果:\n";
            output += "   ✓ 需求清晰无歧义\n";
            output += "   ✓ 验收标准可量化\n";
            output += "   ✓ 技术方案可行\n";
        } else if (stage_name == "方案设计" || stage_name == "修复方案") {
            output = "🎨 设计阶段 - 迭代 " + std::to_string(iteration) + "\n\n";
            output += "1. 架构设计...\n";
            output += "   - 模块划分: 完成\n";
            output += "   - 接口定义: 完成\n";
            output += "   - 数据流设计: 完成\n\n";
            output += "2. 技术选型...\n";
            output += "   - 核心技术栈: 已确定\n";
            output += "   - 依赖库选择: 已评估\n";
            output += "   - 性能预估: 已完成\n\n";
            output += "3. 设计文档:\n";
            output += "   ✓ 架构图已绘制\n";
            output += "   ✓ 接口规范已定义\n";
            output += "   ✓ 实现路径已明确\n";
        } else {
            output = "⚙️ 实现阶段 - 迭代 " + std::to_string(iteration) + "\n\n";
            output += "1. 代码实现...\n";
            output += "   - 核心功能: 已实现\n";
            output += "   - 边界处理: 已完成\n";
            output += "   - 错误处理: 已添加\n\n";
            output += "2. 单元测试...\n";
            output += "   - 测试用例: 15个\n";
            output += "   - 通过率: 100%\n";
            output += "   - 代码覆盖率: 92%\n\n";
            output += "3. 集成测试...\n";
            output += "   - 功能测试: 通过\n";
            output += "   - 性能测试: 通过\n";
            output += "   - 兼容性测试: 通过\n\n";
            output += "4. 代码审查:\n";
            output += "   ✓ 符合编码规范\n";
            output += "   ✓ 无安全漏洞\n";
            output += "   ✓ 满足设计要求\n";
        }

        return output;
    }

    // 异步模拟AI处理
    void simulate_ai_processing() {
        if (!project || project->current_stage_index >= (int)project->stages.size()) return;

        auto& stage = project->stages[project->current_stage_index];

        // 模拟处理过程（分多个步骤）
        for (int step = 1; step <= 5; step++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(600));

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                processing_progress = step * 20;

                // 添加对话历史
                if (step == 1) {
                    stage.conversation_history.push_back("🤖 AI: 开始分析任务需求...");
                } else if (step == 2) {
                    stage.conversation_history.push_back("🤖 AI: 生成解决方案...");
                } else if (step == 3) {
                    stage.conversation_history.push_back("🤖 AI: 执行实现步骤...");
                } else if (step == 4) {
                    stage.conversation_history.push_back("🤖 AI: 运行测试验证...");
                } else if (step == 5) {
                    stage.conversation_history.push_back("🤖 AI: 评估验收标准...");
                }
            }
        }

        // 处理完成，更新状态
        {
            std::lock_guard<std::mutex> lock(state_mutex);

            // 生成输出
            stage.output = generate_stage_output(stage.name, stage.iteration_count);

            // 创建迭代记录
            IterationRecord record;
            record.iteration_number = stage.iteration_count;

            if (stage.iteration_count == 1) {
                record.action = "代码生成";
            } else {
                record.action = "代码修改";
            }

            // 模拟测试结果（前几次可能失败）
            if (stage.iteration_count == 1 && stage.iteration_count < stage.max_iterations - 1) {
                record.test_result = "测试失败";
                record.reason = "空指针异常";
                record.status_icon = "🔴";
            } else if (stage.iteration_count == 2 && stage.iteration_count < stage.max_iterations - 1) {
                record.test_result = "测试部分通过";
                record.reason = "边界条件未处理";
                record.status_icon = "🟡";
            } else {
                record.test_result = "测试通过";
                record.reason = "✅ 任务完成";
                record.status_icon = "🟢";
            }

            stage.iteration_history.push_back(record);

            // 自动评估验收测试（模拟成功场景）
            for (auto& test : stage.acceptance_tests) {
                test.passed = true;
                test.notes = "AI自动验证通过";
            }

            stage.conversation_history.push_back("✅ 所有验收测试通过");
            stage.status = "done";
            stage.task_status = TaskStatus::COMPLETED;
            is_processing = false;
            processing_progress = 0;
        }

        NotifyListeners();
    }

    void start_current_stage() {
        if (!project || project->current_stage_index >= (int)project->stages.size()) return;

        auto& stage = project->stages[project->current_stage_index];

        // Check iteration limit
        if (stage.iteration_count >= stage.max_iterations) {
            stage.needs_human_intervention = true;
            stage.output = "⚠️ 已达到最大迭代次数 (" + std::to_string(stage.max_iterations) +
                          ")，需要人工介入评估";
            return;
        }

        stage.status = "running";
        stage.task_status = TaskStatus::IN_PROGRESS;
        stage.iteration_count++;
        is_processing = true;
        processing_progress = 0;

        // 清空之前的输出
        stage.output = "";
        stage.conversation_history.push_back("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        stage.conversation_history.push_back("开始第 " + std::to_string(stage.iteration_count) + " 次迭代");

        // 启动异步处理线程
        if (processing_thread.joinable()) {
            processing_thread.join();
        }
        processing_thread = std::thread(&AppState::simulate_ai_processing, this);
    }

    ~AppState() {
        if (processing_thread.joinable()) {
            processing_thread.join();
        }
    }
    
    void complete_current_stage() {
        if (!project || project->current_stage_index >= (int)project->stages.size()) return;

        auto& stage = project->stages[project->current_stage_index];

        // Check acceptance tests
        bool all_tests_passed = true;
        for (const auto& test : stage.acceptance_tests) {
            if (!test.passed) {
                all_tests_passed = false;
                break;
            }
        }

        if (!all_tests_passed) {
            stage.output = "❌ 验收测试未全部通过，请继续迭代或手动标记测试";
            stage.status = "pending"; // Reset to allow retry
            is_processing = false;
            return;
        }

        // All tests passed, complete stage
        stage.status = "done";
        stage.task_status = TaskStatus::COMPLETED;
        stage.output = "✅ 完成：第 " + std::to_string(stage.iteration_count) +
                       " 次迭代，所有验收测试通过";
        is_processing = false;

        // Move to next stage
        if (project->current_stage_index < (int)project->stages.size() - 1) {
            project->current_stage_index++;
            project->stages[project->current_stage_index].status = "pending";
        }
    }

    void retry_current_stage() {
        if (!project || project->current_stage_index >= (int)project->stages.size()) return;

        auto& stage = project->stages[project->current_stage_index];
        stage.status = "pending";
        stage.needs_human_intervention = false;
        is_processing = false;
    }
};

// ============================================================================
// 3. 屏幕组件实现 (Screen Components)
// ============================================================================

// --- 屏幕 1: 选择项目类型 ---
ftxui::Component CreateTypeSelectionScreen(std::shared_ptr<AppState> state) {
    auto selected = std::make_shared<int>(0);

    auto renderer = ftxui::Renderer([state, selected] {
        std::vector<ftxui::Element> children;

        // 标题
        children.push_back(ftxui::text("选择项目类型") | ftxui::center | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        children.push_back(ftxui::emptyElement());

        // 卡片 1: 新功能/优化功能
        std::vector<ftxui::Element> card1;
        card1.push_back(ftxui::text("🔵 新功能/优化功能") | ftxui::bold | ftxui::color(ftxui::Color::Blue));
        card1.push_back(ftxui::emptyElement());
        card1.push_back(ftxui::text("适用场景：从零开发新功能、现有功能优化"));
        card1.push_back(ftxui::text("流程：需求探讨 → 方案设计 → 任务迭代(局部+全局)"));
        card1.push_back(ftxui::text("产物：需求文档、设计文档、验收测试清单"));
        card1.push_back(ftxui::emptyElement());
        card1.push_back(ftxui::text("[ 选择此类型 ]") | ftxui::center);

        auto card1_box = ftxui::vbox(card1) | ftxui::border;
        if (*selected == 0) {
            card1_box = card1_box | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
        }
        children.push_back(card1_box);
        children.push_back(ftxui::emptyElement());

        // 卡片 2: 修复缺陷
        std::vector<ftxui::Element> card2;
        card2.push_back(ftxui::text("🔴 修复缺陷") | ftxui::bold | ftxui::color(ftxui::Color::Red));
        card2.push_back(ftxui::emptyElement());
        card2.push_back(ftxui::text("适用场景：Bug 修复、安全漏洞修补"));
        card2.push_back(ftxui::text("流程：明确问题 → 修复方案 → 执行迭代"));
        card2.push_back(ftxui::text("产物：问题报告、修复方案、回归测试结果"));
        card2.push_back(ftxui::emptyElement());
        card2.push_back(ftxui::text("[ 选择此类型 ]") | ftxui::center);

        auto card2_box = ftxui::vbox(card2) | ftxui::border;
        if (*selected == 1) {
            card2_box = card2_box | ftxui::color(ftxui::Color::Yellow) | ftxui::bold;
        }
        children.push_back(card2_box);
        children.push_back(ftxui::emptyElement());

        // 快捷键提示
        children.push_back(ftxui::text("快捷键：↑↓ 选择  Enter 确认  q 退出") | ftxui::center | ftxui::dim);

        return ftxui::vbox(children) | ftxui::center | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 80);
    });

    return ftxui::CatchEvent(renderer, [state, selected](ftxui::Event event) {
        if (event == ftxui::Event::ArrowUp) {
            *selected = (*selected == 0) ? 1 : 0;
            return true;
        } else if (event == ftxui::Event::ArrowDown) {
            *selected = (*selected == 1) ? 0 : 1;
            return true;
        } else if (event == ftxui::Event::Return) {
            if (*selected == 0) {
                state->project = std::make_shared<Project>("MyProject", ProjectType::NEW_FEATURE);
            } else {
                state->project = std::make_shared<Project>("MyProject", ProjectType::BUG_FIX);
            }
            state->screen = AppScreen::INPUT_NAME;
            state->NotifyListeners();
            return true;
        }
        return false;
    });
}

// --- 屏幕 2: 输入项目名称 ---
ftxui::Component CreateNameInputScreen(std::shared_ptr<AppState> state) {
    auto input = ftxui::Input(&state->input_name, "输入项目名称");

    auto submit_btn = ftxui::Button("开始创建", [state] {
        if (!state->input_name.empty()) {
            state->project->name = state->input_name;
            state->screen = AppScreen::MAIN_WORKSPACE;
            state->start_current_stage();
            state->NotifyListeners();
        }
    });

    auto cancel_btn = ftxui::Button("返回", [state] {
        state->screen = AppScreen::SELECT_TYPE;
        state->input_name = "";
        state->NotifyListeners();
    });

    auto container = ftxui::Container::Vertical({input, submit_btn, cancel_btn});

    return ftxui::Renderer(container, [container, state] {
        std::vector<ftxui::Element> children;

        // 标题
        children.push_back(ftxui::text("项目设置") | ftxui::center | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        children.push_back(ftxui::emptyElement());

        // 项目类型信息卡片
        std::vector<ftxui::Element> info_card;
        std::string type_name = (state->project->type == ProjectType::NEW_FEATURE) ?
            "🔵 新功能/优化功能" : "🔴 修复缺陷";
        info_card.push_back(ftxui::text("已选择类型：" + type_name) | ftxui::bold);
        info_card.push_back(ftxui::emptyElement());

        if (state->project->type == ProjectType::NEW_FEATURE) {
            info_card.push_back(ftxui::text("流程：需求探讨 → 方案设计 → 任务迭代"));
        } else {
            info_card.push_back(ftxui::text("流程：明确问题 → 修复方案 → 执行迭代"));
        }

        children.push_back(ftxui::vbox(info_card) | ftxui::border | ftxui::color(ftxui::Color::Blue));
        children.push_back(ftxui::emptyElement());

        // 输入卡片
        std::vector<ftxui::Element> input_card;
        input_card.push_back(ftxui::text("📝 项目名称") | ftxui::bold);
        input_card.push_back(ftxui::emptyElement());
        input_card.push_back(container->Render());

        children.push_back(ftxui::vbox(input_card) | ftxui::border);
        children.push_back(ftxui::emptyElement());

        // 快捷键提示
        children.push_back(ftxui::text("Tab 切换 · Enter 确认 · Esc 返回") | ftxui::center | ftxui::dim);

        return ftxui::vbox(children) | ftxui::center | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 70);
    });
}

// --- 屏幕 3: 主工作区 (三栏布局) ---
ftxui::Component CreateMainWorkspace(std::shared_ptr<AppState> state) {
    // 左侧：阶段导航 - 使用 Renderer 动态生成按钮
    auto nav_selected = std::make_shared<int>(state->project->current_stage_index);

    auto nav_renderer = ftxui::Renderer([state, nav_selected] {
        std::vector<ftxui::Element> nav_items;

        // 项目信息头部
        std::string type_icon = (state->project->type == ProjectType::NEW_FEATURE) ? "🔵" : "🔴";
        nav_items.push_back(ftxui::text(type_icon + " " + state->project->name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        nav_items.push_back(ftxui::separator());

        // 阶段标题
        nav_items.push_back(ftxui::text("📋 项目阶段") | ftxui::bold);
        nav_items.push_back(ftxui::emptyElement());

        for (size_t i = 0; i < state->project->stages.size(); ++i) {
            const auto& stage = state->project->stages[i];
            std::string icon = (stage.status == "done") ? "✅" :
                               (stage.status == "running") ? "⏳" : "⏸️";
            std::string label = icon + " " + stage.name;

            bool is_selected = ((int)i == state->project->current_stage_index);
            auto element = ftxui::text(label);
            if (is_selected) {
                element = element | ftxui::bold | ftxui::color(ftxui::Color::Yellow) | ftxui::bgcolor(ftxui::Color::GrayDark);
            } else if (stage.status == "done") {
                element = element | ftxui::color(ftxui::Color::Green);
            } else if (stage.status == "running") {
                element = element | ftxui::color(ftxui::Color::Yellow);
            } else {
                element = element | ftxui::dim;
            }
            nav_items.push_back(element);
        }

        // 类型1（新功能）：在任务迭代阶段显示任务树
        if (state->project->type == ProjectType::NEW_FEATURE &&
            state->project->current_stage_index == 2) {  // 任务迭代阶段
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
                } else if (task.status == TaskStatus::IN_PROGRESS) {
                    task_icon = "🟡";
                    task_color = ftxui::Color::Yellow;
                } else {
                    task_icon = "⚪";
                    task_color = ftxui::Color::White;
                }

                std::string task_label = task_icon + " " + task.name;
                auto task_element = ftxui::text(task_label) | ftxui::color(task_color);
                nav_items.push_back(task_element);
            }
        }

        return ftxui::vbox(nav_items) | ftxui::border | ftxui::flex;
    });

    // 中间：工作区内容
    auto workspace_renderer = ftxui::Renderer([state] {
        if (!state->project) return ftxui::text("Error");

        const auto& stage = state->project->stages[state->project->current_stage_index];

        // 根据视图模式渲染不同内容
        if (state->workspace_view == WorkspaceView::ITERATION_HISTORY) {
            // 局部迭代状态可视化
            std::vector<ftxui::Element> content;
            content.push_back(ftxui::text("局部迭代详情 - " + stage.name) | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
            content.push_back(ftxui::separator());
            content.push_back(ftxui::emptyElement());

            if (stage.iteration_history.empty()) {
                content.push_back(ftxui::text("暂无迭代记录") | ftxui::dim);
            } else {
                content.push_back(ftxui::text("迭代历史：") | ftxui::bold);
                content.push_back(ftxui::emptyElement());

                // 表头
                std::vector<ftxui::Element> header_row;
                header_row.push_back(ftxui::text("迭代") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
                header_row.push_back(ftxui::separator());
                header_row.push_back(ftxui::text("操作") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
                header_row.push_back(ftxui::separator());
                header_row.push_back(ftxui::text("测试结果") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15));
                header_row.push_back(ftxui::separator());
                header_row.push_back(ftxui::text("原因/详情") | ftxui::bold | ftxui::flex);
                content.push_back(ftxui::hbox(header_row) | ftxui::border);

                // 迭代记录行
                for (const auto& record : stage.iteration_history) {
                    std::vector<ftxui::Element> row;
                    row.push_back(ftxui::text("迭代" + std::to_string(record.iteration_number)) |
                                 ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
                    row.push_back(ftxui::separator());
                    row.push_back(ftxui::text(record.status_icon + " " + record.action) |
                                 ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
                    row.push_back(ftxui::separator());

                    auto test_element = ftxui::text(record.test_result) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15);
                    if (record.test_result == "测试通过") {
                        test_element = test_element | ftxui::color(ftxui::Color::Green);
                    } else if (record.test_result == "测试部分通过") {
                        test_element = test_element | ftxui::color(ftxui::Color::Yellow);
                    } else {
                        test_element = test_element | ftxui::color(ftxui::Color::Red);
                    }
                    row.push_back(test_element);
                    row.push_back(ftxui::separator());
                    row.push_back(ftxui::text(record.reason) | ftxui::flex);

                    content.push_back(ftxui::hbox(row) | ftxui::border);
                }
            }

            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::separator());

            // 迭代统计
            std::string iter_stat = "迭代限制：已用 " + std::to_string(stage.iteration_count) + "/" +
                                   std::to_string(stage.max_iterations) + " 次  [剩余 " +
                                   std::to_string(stage.max_iterations - stage.iteration_count) + " 次]";
            content.push_back(ftxui::text(iter_stat) | ftxui::bold);

            if (stage.iteration_count >= stage.max_iterations) {
                content.push_back(ftxui::text("人工介入阈值：已达到最大迭代次数") | ftxui::color(ftxui::Color::Red) | ftxui::bold);
            } else if (stage.iteration_count >= 3) {
                content.push_back(ftxui::text("人工介入阈值：接近上限，建议关注") | ftxui::color(ftxui::Color::Yellow));
            } else {
                content.push_back(ftxui::text("人工介入阈值：超过 " + std::to_string(stage.max_iterations) + " 次自动请求确认"));
            }

            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::text("按 Esc 返回正常视图") | ftxui::dim | ftxui::center);

            return ftxui::vbox(content) | ftxui::flex;

        } else if (state->workspace_view == WorkspaceView::GLOBAL_VALIDATION) {
            // 全局迭代状态可视化
            std::vector<ftxui::Element> content;
            content.push_back(ftxui::text("项目全局验证 - 设计文档绑定") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
            content.push_back(ftxui::separator());
            content.push_back(ftxui::emptyElement());

            content.push_back(ftxui::text("设计文档要求 → 验收测试映射：") | ftxui::bold);
            content.push_back(ftxui::emptyElement());

            // 表头
            std::vector<ftxui::Element> header_row;
            header_row.push_back(ftxui::text("设计文档条目") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 25));
            header_row.push_back(ftxui::separator());
            header_row.push_back(ftxui::text("测试用例") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
            header_row.push_back(ftxui::separator());
            header_row.push_back(ftxui::text("状态") | ftxui::bold | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8));
            header_row.push_back(ftxui::separator());
            header_row.push_back(ftxui::text("详情") | ftxui::bold | ftxui::flex);
            content.push_back(ftxui::hbox(header_row) | ftxui::border);

            // 验收测试行
            int test_num = 1;
            for (const auto& test : stage.acceptance_tests) {
                std::vector<ftxui::Element> row;
                row.push_back(ftxui::text(test.description) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 25));
                row.push_back(ftxui::separator());
                row.push_back(ftxui::text("TC-" + std::string(3 - std::to_string(test_num).length(), '0') +
                             std::to_string(test_num)) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 12));
                row.push_back(ftxui::separator());

                std::string status_icon = test.passed ? "✅" : "❌";
                auto status_element = ftxui::text(status_icon) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 8);
                if (test.passed) {
                    status_element = status_element | ftxui::color(ftxui::Color::Green);
                } else {
                    status_element = status_element | ftxui::color(ftxui::Color::Red);
                }
                row.push_back(status_element);
                row.push_back(ftxui::separator());

                std::string detail = test.passed ? test.notes : "未通过";
                row.push_back(ftxui::text(detail) | ftxui::flex);

                content.push_back(ftxui::hbox(row) | ftxui::border);
                test_num++;
            }

            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::separator());

            // 整体进度
            int passed = 0;
            int total = stage.acceptance_tests.size();
            for (const auto& test : stage.acceptance_tests) {
                if (test.passed) passed++;
            }

            int percentage = total > 0 ? (passed * 100 / total) : 0;
            std::string progress_text = "整体进度：" + std::to_string(passed) + "/" +
                                       std::to_string(total) + " 通过 (" +
                                       std::to_string(percentage) + "%)";
            auto progress_element = ftxui::text(progress_text) | ftxui::bold;
            if (passed == total) {
                progress_element = progress_element | ftxui::color(ftxui::Color::Green);
            } else {
                progress_element = progress_element | ftxui::color(ftxui::Color::Yellow);
            }
            content.push_back(progress_element);

            content.push_back(ftxui::emptyElement());

            // 警告信息
            if (passed < total) {
                content.push_back(ftxui::text("⚠️ 警告：验收清单未全部通过，不允许标记项目完成") |
                                 ftxui::color(ftxui::Color::Red) | ftxui::bold);
            } else {
                content.push_back(ftxui::text("✅ 所有验收测试通过，可以完成当前阶段") |
                                 ftxui::color(ftxui::Color::Green) | ftxui::bold);
            }

            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::text("按 Esc 返回正常视图") | ftxui::dim | ftxui::center);

            return ftxui::vbox(content) | ftxui::flex;

        } else {
            // 正常视图
            std::vector<ftxui::Element> content;

            // Stage header with iteration counter
            std::string header = "当前阶段: " + stage.name;
            if (stage.iteration_count > 0) {
                header += " [迭代 " + std::to_string(stage.iteration_count) + "/" +
                          std::to_string(stage.max_iterations) + "]";
            }
            content.push_back(ftxui::text(header) | ftxui::bold | ftxui::color(ftxui::Color::Yellow));
            content.push_back(ftxui::separator());

            // Status display
            if (stage.needs_human_intervention) {
                content.push_back(ftxui::text("⚠️ 需要人工介入") | ftxui::color(ftxui::Color::Red) | ftxui::bold);
                content.push_back(ftxui::text(stage.output));
                content.push_back(ftxui::emptyElement());
                content.push_back(ftxui::text("按 R 重置并重试") | ftxui::dim);
            } else if (stage.status == "running") {
                content.push_back(ftxui::text("🤖 AI 正在处理中...") | ftxui::color(ftxui::Color::Green) | ftxui::bold);
                content.push_back(ftxui::emptyElement());

                // Progress bar
                int progress = state->processing_progress;
                std::string progress_bar = "[";
                for (int i = 0; i < 20; i++) {
                    if (i < progress / 5) {
                        progress_bar += "█";
                    } else {
                        progress_bar += "░";
                    }
                }
                progress_bar += "] " + std::to_string(progress) + "%";
                content.push_back(ftxui::text(progress_bar) | ftxui::color(ftxui::Color::Cyan));
                content.push_back(ftxui::emptyElement());

                // Conversation history (last 5 messages)
                if (!stage.conversation_history.empty()) {
                    content.push_back(ftxui::text("💬 处理日志:") | ftxui::bold);
                    size_t start = stage.conversation_history.size() > 5 ?
                                  stage.conversation_history.size() - 5 : 0;
                    for (size_t i = start; i < stage.conversation_history.size(); ++i) {
                        content.push_back(ftxui::text("  " + stage.conversation_history[i]) | ftxui::dim);
                    }
                }
            } else if (stage.status == "done") {
                // Show output
                if (!stage.output.empty()) {
                    // Split output by newlines for better display
                    std::string line;
                    std::istringstream stream(stage.output);
                    while (std::getline(stream, line)) {
                        content.push_back(ftxui::text(line));
                    }
                }

                content.push_back(ftxui::emptyElement());
                if (state->project->current_stage_index < (int)state->project->stages.size() - 1) {
                    content.push_back(ftxui::text("按 Space 进入下一阶段") | ftxui::dim);
                } else {
                    content.push_back(ftxui::text("🎉 项目全部完成!") | ftxui::bold | ftxui::color(ftxui::Color::Blue));
                }
            } else {
                content.push_back(ftxui::text("等待开始...") | ftxui::dim);
                content.push_back(ftxui::emptyElement());
                content.push_back(ftxui::text("按 Space 开始此阶段") | ftxui::dim);
            }

            // Acceptance tests section
            if (!stage.acceptance_tests.empty() && stage.status != "running") {
                content.push_back(ftxui::emptyElement());
                content.push_back(ftxui::separator());
                content.push_back(ftxui::text("📋 验收测试清单") | ftxui::bold);

                for (size_t i = 0; i < stage.acceptance_tests.size(); ++i) {
                    const auto& test = stage.acceptance_tests[i];
                    std::string icon = test.passed ? "✅" : "❌";
                    std::string label = icon + " " + test.description;
                    auto element = ftxui::text(label);
                    if (test.passed) {
                        element = element | ftxui::color(ftxui::Color::Green);
                    }
                    content.push_back(element);
                }

                if (stage.status == "pending") {
                    content.push_back(ftxui::emptyElement());
                    content.push_back(ftxui::text("按 T 切换测试状态 (手动模式)") | ftxui::dim);
                }
            }

            // 视图切换提示
            content.push_back(ftxui::emptyElement());
            content.push_back(ftxui::separator());
            content.push_back(ftxui::text("按 I 查看迭代历史 · 按 V 查看全局验证") | ftxui::dim | ftxui::center);

            return ftxui::vbox(content) | ftxui::flex;
        }
    });

    // 右侧：上下文/日志
    auto context_renderer = ftxui::Renderer([state] {
        std::vector<ftxui::Element> sections;

        // 当前阶段信息卡片
        std::vector<ftxui::Element> current_info;
        const auto& current_stage = state->project->stages[state->project->current_stage_index];

        current_info.push_back(ftxui::text("📊 当前阶段") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        current_info.push_back(ftxui::separator());
        current_info.push_back(ftxui::text(current_stage.name) | ftxui::bold);

        if (current_stage.iteration_count > 0) {
            std::string iter_text = "迭代: " + std::to_string(current_stage.iteration_count) + "/" +
                                   std::to_string(current_stage.max_iterations);
            auto iter_element = ftxui::text(iter_text);
            if (current_stage.iteration_count >= current_stage.max_iterations) {
                iter_element = iter_element | ftxui::color(ftxui::Color::Red);
            } else if (current_stage.iteration_count >= 3) {
                iter_element = iter_element | ftxui::color(ftxui::Color::Yellow);
            }
            current_info.push_back(iter_element);
        }

        // Acceptance test progress
        int passed_tests = 0;
        int total_tests = current_stage.acceptance_tests.size();
        for (const auto& test : current_stage.acceptance_tests) {
            if (test.passed) passed_tests++;
        }
        if (total_tests > 0) {
            std::string test_text = "测试: " + std::to_string(passed_tests) + "/" +
                                   std::to_string(total_tests);
            auto test_element = ftxui::text(test_text);
            if (passed_tests == total_tests) {
                test_element = test_element | ftxui::color(ftxui::Color::Green);
            } else if (passed_tests > 0) {
                test_element = test_element | ftxui::color(ftxui::Color::Yellow);
            }
            current_info.push_back(test_element);
        }

        sections.push_back(ftxui::vbox(current_info) | ftxui::border);
        sections.push_back(ftxui::emptyElement());

        // 类型特定信息卡片
        if (state->project->type == ProjectType::NEW_FEATURE &&
            state->project->current_stage_index == 2) {  // 任务迭代阶段
            // 显示任务进度
            std::vector<ftxui::Element> task_progress;
            task_progress.push_back(ftxui::text("📊 任务进度") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
            task_progress.push_back(ftxui::separator());

            int completed_tasks = 0;
            for (const auto& task : state->project->tasks) {
                if (task.status == TaskStatus::COMPLETED) completed_tasks++;
            }

            std::string progress_text = "完成: " + std::to_string(completed_tasks) + "/" +
                                       std::to_string(state->project->tasks.size());
            task_progress.push_back(ftxui::text(progress_text) | ftxui::bold);

            sections.push_back(ftxui::vbox(task_progress) | ftxui::border);
            sections.push_back(ftxui::emptyElement());

        } else if (state->project->type == ProjectType::BUG_FIX) {
            // 显示问题信息
            std::vector<ftxui::Element> bug_info_panel;
            bug_info_panel.push_back(ftxui::text("🔴 问题信息") | ftxui::bold | ftxui::color(ftxui::Color::Red));
            bug_info_panel.push_back(ftxui::separator());

            bug_info_panel.push_back(ftxui::text("严重级别: " + state->project->bug_info.severity) | ftxui::bold);
            bug_info_panel.push_back(ftxui::text("位置: " + state->project->bug_info.location));

            if (state->project->current_stage_index >= 1) {  // 修复方案阶段及之后
                bug_info_panel.push_back(ftxui::emptyElement());
                bug_info_panel.push_back(ftxui::text("修改文件: " + std::to_string(state->project->bug_info.files_modified)));
                bug_info_panel.push_back(ftxui::text("新增代码: " + std::to_string(state->project->bug_info.lines_added) + " 行"));
                bug_info_panel.push_back(ftxui::text("风险评估: " + state->project->bug_info.risk_level));
            }

            sections.push_back(ftxui::vbox(bug_info_panel) | ftxui::border);
            sections.push_back(ftxui::emptyElement());
        }

        // 所有阶段状态卡片
        std::vector<ftxui::Element> stages_info;
        stages_info.push_back(ftxui::text("📋 所有阶段") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        stages_info.push_back(ftxui::separator());

        for (size_t i = 0; i < state->project->stages.size(); ++i) {
            const auto& s = state->project->stages[i];
            std::string icon = (s.status == "done") ? "✅" :
                              (s.status == "running") ? "⏳" : "⏸️";
            std::string label = icon + " " + s.name;

            if (s.iteration_count > 0) {
                label += " [" + std::to_string(s.iteration_count) + "]";
            }

            ftxui::Element line = ftxui::text(label);
            if (s.status == "done") line = line | ftxui::color(ftxui::Color::Green);
            else if (s.status == "running") line = line | ftxui::color(ftxui::Color::Yellow);
            else if (s.needs_human_intervention) line = line | ftxui::color(ftxui::Color::Red) | ftxui::bold;
            else line = line | ftxui::dim;

            stages_info.push_back(line);
        }

        sections.push_back(ftxui::vbox(stages_info) | ftxui::border);
        sections.push_back(ftxui::emptyElement());

        // 快捷键卡片
        std::vector<ftxui::Element> shortcuts;
        shortcuts.push_back(ftxui::text("⌨️  快捷键") | ftxui::bold | ftxui::color(ftxui::Color::Cyan));
        shortcuts.push_back(ftxui::separator());
        shortcuts.push_back(ftxui::text("Space: 开始/下一阶段") | ftxui::dim);
        shortcuts.push_back(ftxui::text("T: 切换测试状态") | ftxui::dim);
        shortcuts.push_back(ftxui::text("C: 完成当前阶段") | ftxui::dim);
        shortcuts.push_back(ftxui::text("R: 重试") | ftxui::dim);
        shortcuts.push_back(ftxui::text("I: 迭代历史") | ftxui::dim);
        shortcuts.push_back(ftxui::text("V: 全局验证") | ftxui::dim);
        shortcuts.push_back(ftxui::text("Ctrl+Q: 退出") | ftxui::dim);

        sections.push_back(ftxui::vbox(shortcuts) | ftxui::border);

        return ftxui::vbox(sections) | ftxui::flex;
    });

    // 组合三栏
    auto main_container = ftxui::Container::Horizontal({nav_renderer, workspace_renderer, context_renderer});

    // 添加键盘事件处理
    auto component_with_events = ftxui::CatchEvent(main_container, [state](ftxui::Event event) {
        if (event == ftxui::Event::Character(' ')) {
            // Space 键处理
            if (state->workspace_view != WorkspaceView::NORMAL) return false;
            const auto& stage = state->project->stages[state->project->current_stage_index];
            if (stage.status == "pending" && !stage.needs_human_intervention) {
                state->start_current_stage();
                return true;
            } else if (stage.status == "done" &&
                       state->project->current_stage_index < (int)state->project->stages.size() - 1) {
                state->complete_current_stage();
                state->start_current_stage();
                return true;
            }
        } else if (event == ftxui::Event::Character('t') || event == ftxui::Event::Character('T')) {
            // Toggle acceptance tests (mock - cycles through tests)
            if (state->workspace_view != WorkspaceView::NORMAL) return false;
            auto& stage = state->project->stages[state->project->current_stage_index];
            if (!stage.acceptance_tests.empty()) {
                // Find first unpassed test and mark it passed
                for (auto& test : stage.acceptance_tests) {
                    if (!test.passed) {
                        test.passed = true;
                        break;
                    }
                }
            }
            return true;
        } else if (event == ftxui::Event::Character('r') || event == ftxui::Event::Character('R')) {
            // Retry after human intervention
            if (state->workspace_view != WorkspaceView::NORMAL) return false;
            auto& stage = state->project->stages[state->project->current_stage_index];
            if (stage.needs_human_intervention) {
                state->retry_current_stage();
                return true;
            }
        } else if (event == ftxui::Event::Character('c') || event == ftxui::Event::Character('C')) {
            // Complete stage (attempts to move to next)
            if (state->workspace_view != WorkspaceView::NORMAL) return false;
            auto& stage = state->project->stages[state->project->current_stage_index];
            if (stage.status == "running") {
                state->complete_current_stage();
                return true;
            }
        } else if (event == ftxui::Event::Character('i') || event == ftxui::Event::Character('I')) {
            // 切换到迭代历史视图
            state->workspace_view = WorkspaceView::ITERATION_HISTORY;
            state->NotifyListeners();
            return true;
        } else if (event == ftxui::Event::Character('v') || event == ftxui::Event::Character('V')) {
            // 切换到全局验证视图
            state->workspace_view = WorkspaceView::GLOBAL_VALIDATION;
            state->NotifyListeners();
            return true;
        } else if (event == ftxui::Event::Escape) {
            // 返回正常视图
            if (state->workspace_view != WorkspaceView::NORMAL) {
                state->workspace_view = WorkspaceView::NORMAL;
                state->NotifyListeners();
                return true;
            }
        }
        return false;
    });

    return ftxui::Renderer(component_with_events, [state, nav_renderer, workspace_renderer, context_renderer] {
        // 三栏布局
        std::vector<ftxui::Element> panels = {
            nav_renderer->Render() | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 25),
            workspace_renderer->Render() | ftxui::flex,
            context_renderer->Render() | ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 25)
        };
        auto main_panels = ftxui::hbox(panels);

        // 状态栏
        std::vector<ftxui::Element> status_items;

        // 项目类型
        std::string type_icon = (state->project->type == ProjectType::NEW_FEATURE) ? "📁 新功能" : "📁 修复缺陷";
        auto type_color = (state->project->type == ProjectType::NEW_FEATURE) ? ftxui::Color::Blue : ftxui::Color::Red;
        status_items.push_back(ftxui::text("[" + type_icon + "]") | ftxui::color(type_color) | ftxui::bold);

        // 当前阶段
        const auto& current_stage = state->project->stages[state->project->current_stage_index];
        std::string stage_text = "[阶段: " + current_stage.name + " " +
                                std::to_string(state->project->current_stage_index + 1) + "]";
        auto stage_color = (current_stage.status == "done") ? ftxui::Color::Green :
                          (current_stage.status == "running") ? ftxui::Color::Yellow : ftxui::Color::White;
        status_items.push_back(ftxui::text(stage_text) | ftxui::color(stage_color));

        // 迭代计数
        if (current_stage.iteration_count > 0) {
            std::string iter_text = "[迭代: " + std::to_string(current_stage.iteration_count) + "/" +
                                   std::to_string(current_stage.max_iterations) + "]";
            auto iter_color = (current_stage.iteration_count >= current_stage.max_iterations) ? ftxui::Color::Red :
                             (current_stage.iteration_count >= 3) ? ftxui::Color::Yellow : ftxui::Color::Green;
            status_items.push_back(ftxui::text(iter_text) | ftxui::color(iter_color));
        }

        // 验收测试进度
        int passed_tests = 0;
        int total_tests = current_stage.acceptance_tests.size();
        for (const auto& test : current_stage.acceptance_tests) {
            if (test.passed) passed_tests++;
        }
        if (total_tests > 0) {
            std::string test_text = "[验收: " + std::to_string(passed_tests) + "/" +
                                   std::to_string(total_tests);
            if (passed_tests == total_tests) {
                test_text += " ✅]";
            } else {
                test_text += "]";
            }
            auto test_color = (passed_tests == total_tests) ? ftxui::Color::Green :
                             (passed_tests > 0) ? ftxui::Color::Yellow : ftxui::Color::White;
            status_items.push_back(ftxui::text(test_text) | ftxui::color(test_color));
        }

        // 系统状态
        std::string status_icon = current_stage.needs_human_intervention ? "[🔴]" :
                                 state->is_processing ? "[🟡]" : "[🟢]";
        auto status_color = current_stage.needs_human_intervention ? ftxui::Color::Red :
                           state->is_processing ? ftxui::Color::Yellow : ftxui::Color::Green;
        status_items.push_back(ftxui::text(status_icon) | ftxui::color(status_color) | ftxui::bold);

        auto status_bar = ftxui::hbox(status_items) | ftxui::border;

        // 组合主面板和状态栏
        return ftxui::vbox({
            main_panels | ftxui::flex,
            status_bar
        });
    });
}

int AcpreTui::run(const int argc, char *argv[])
{
    auto state = std::make_shared<AppState>();

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    // 创建各屏幕组件
    auto type_screen = CreateTypeSelectionScreen(state);
    auto name_screen = CreateNameInputScreen(state);

    // 不直接创建 workspace_screen，因为此时 project 可能还是 nullptr
    ftxui::Component workspace_screen = nullptr;

    // 使用 Tab 容器来管理多个屏幕，根据状态切换
    int tab_index = 0;
    auto main_container = ftxui::Container::Tab(
        {type_screen, name_screen},
        &tab_index
    );

    // 使用 Renderer 根据状态动态渲染不同屏幕并路由事件
    auto main_component = ftxui::Container::Vertical({main_container});

    auto main_renderer = ftxui::Renderer(main_component, [&] {
        if (state->screen == AppScreen::SELECT_TYPE) {
            tab_index = 0;
            return type_screen->Render();
        } else if (state->screen == AppScreen::INPUT_NAME) {
            tab_index = 1;
            return name_screen->Render();
        } else {
            // 延迟创建 workspace_screen，只在需要时创建
            if (!workspace_screen) {
                workspace_screen = CreateMainWorkspace(state);
            }
            return workspace_screen->Render();
        }
    });

    // 添加事件路由 - 确保事件传递到正确的屏幕
    auto event_router = ftxui::CatchEvent(main_renderer, [&](ftxui::Event event) {
        // 先处理全局快捷键
        if (event == ftxui::Event::CtrlQ) {
            screen.Exit();
            return true;
        }

        // 根据当前屏幕路由事件
        if (state->screen == AppScreen::SELECT_TYPE) {
            return type_screen->OnEvent(event);
        } else if (state->screen == AppScreen::INPUT_NAME) {
            return name_screen->OnEvent(event);
        } else if (workspace_screen) {
            return workspace_screen->OnEvent(event);
        }

        return false;
    });

    // 模拟异步更新
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
