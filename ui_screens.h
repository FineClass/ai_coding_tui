#pragma once

#include "acpre.h"

bool route_workspace_event(const std::shared_ptr<AppState>& state, ftxui::Event event);
ftxui::Component CreateTypeSelectionScreen(std::shared_ptr<AppState> state);
ftxui::Component CreateNameInputScreen(std::shared_ptr<AppState> state);
ftxui::Component CreateMainWorkspace(std::shared_ptr<AppState> state);
