#ifndef __ACPRE_H__
#define __ACPRE_H__

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>

extern "C" {
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
}

/*
=============================================================
ACPRE(AI Coding Project Runtime Environment) TUI
=============================================================
- Use C++ FTXUI (https://github.com/ArthurSonzogni/FTXUI)
`ftxui-component` must be first in the linking order relative to the other FTXUI libraries
*/
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/component/event.hpp>

enum class TaskStatus {
    PENDING,
    IN_PROGRESS,
    REVIEW,
    COMPLETED,
    BLOCKED
};

struct AcceptanceTest {
    std::string description;
    bool passed;
    std::string notes;
};

struct IterationRecord {
    int iteration_number;
    std::string action;  // "代码生成", "代码修改"
    std::string test_result;  // "测试失败", "测试部分通过", "测试通过"
    std::string reason;  // Failure reason or success note
    std::string status_icon;  // "🟢", "🟡", "🔴"
};

struct Task {
    std::string name;
    TaskStatus status;
    int iteration_count;
    std::vector<IterationRecord> iteration_history;
};

struct BugInfo {
    std::string description;
    std::string severity;  // "高", "中", "低"
    std::string location;  // File and line number
    std::string root_cause;
    std::vector<std::string> fix_steps;
    int files_modified;
    int lines_added;
    std::string risk_level;  // "低", "中", "高"
};

class AcpreTui
{
public:
    static AcpreTui& get_instance(void) noexcept;

#define APP_LOG_MAX     2048
	enum class Level {
		ERROR = 0,
		WARN,
		NORMAL,
		INFO,
		DEBUG,
	};
    void log_print(const Level level, const char *fmt, ...);

    int run(const int argc, char *argv[]);

private:
    AcpreTui(void) noexcept;
	~AcpreTui(void) noexcept;
    //
    AcpreTui(const AcpreTui&) = delete;
    AcpreTui& operator= (const AcpreTui&) = delete;
    AcpreTui(AcpreTui&&) = delete;
    AcpreTui& operator= (AcpreTui&&) = delete;

private:
    std::mutex mLogLock; /* std::lock_guard<std::mutex> lock(mLogLock); */
    Level mMaxLevel;
};

#define GET_ACPRE()    AcpreTui::get_instance()

#define A_LOGP(fmt, arg...)			GET_ACPRE().log_print(AcpreTui::Level::NORMAL, fmt, ##arg)
#define A_LOGE(tag, fmt, arg...)	GET_ACPRE().log_print(AcpreTui::Level::ERROR,   "[ACPRE-TUI-ERROR]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGW(tag, fmt, arg...)	GET_ACPRE().log_print(AcpreTui::Level::WARN,    "[ACPRE-TUI-WARN-]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGI(tag, fmt, arg...)	GET_ACPRE().log_print(AcpreTui::Level::INFO,    "[ACPRE-TUI-INFO-]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGD(tag, fmt, arg...)	GET_ACPRE().log_print(AcpreTui::Level::DEBUG,   "[ACPRE-TUI-DEBUG]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)

#endif /* __AI_CODING_PROJECT_RUNTIME_ENV_H__ */
