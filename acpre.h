#ifndef __ACPRE_H__
#define __ACPRE_H__

#include "domain.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C" {
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
}

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

enum class AppScreen {
    SELECT_TYPE,
    INPUT_NAME,
    MAIN_WORKSPACE,
};

class AppState {
public:
    AppScreen screen = AppScreen::SELECT_TYPE;
    UiSessionState ui_session;
    std::shared_ptr<Project> project = nullptr;
    std::string input_name;
    bool running = true;
    bool is_processing = false;
    bool has_saved_state = false;
    std::mutex state_mutex;
    std::thread processing_thread;
    int processing_progress = 0;
    std::vector<std::function<void()>> listeners;

    void AddListener(std::function<void()> callback);
    void NotifyListeners();
    void run_stage_iteration();
    void start_current_stage();
    ~AppState();
};

class AcpreTui {
public:
    static AcpreTui& get_instance(void) noexcept;

#define APP_LOG_MAX 2048
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
    AcpreTui(const AcpreTui&) = delete;
    AcpreTui& operator=(const AcpreTui&) = delete;
    AcpreTui(AcpreTui&&) = delete;
    AcpreTui& operator=(AcpreTui&&) = delete;

private:
    std::mutex mLogLock;
    Level mMaxLevel = Level::INFO;
};

#define GET_ACPRE() AcpreTui::get_instance()

#define A_LOGP(fmt, arg...)         GET_ACPRE().log_print(AcpreTui::Level::NORMAL, fmt, ##arg)
#define A_LOGE(tag, fmt, arg...)    GET_ACPRE().log_print(AcpreTui::Level::ERROR,   "[ACPRE-TUI-ERROR]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGW(tag, fmt, arg...)    GET_ACPRE().log_print(AcpreTui::Level::WARN,    "[ACPRE-TUI-WARN-]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGI(tag, fmt, arg...)    GET_ACPRE().log_print(AcpreTui::Level::INFO,    "[ACPRE-TUI-INFO-]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)
#define A_LOGD(tag, fmt, arg...)    GET_ACPRE().log_print(AcpreTui::Level::DEBUG,   "[ACPRE-TUI-DEBUG]<%s::%s(),L%d> " fmt, tag, __func__, __LINE__, ##arg)

#endif
