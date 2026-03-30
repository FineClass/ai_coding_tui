#pragma once

#include "domain.h"
#include <string>

namespace persistence {

bool SaveProject(const Project& project, const std::string& path);
bool LoadProject(Project& project, const std::string& path);
std::string DefaultSavePath(const std::string& working_directory);
bool SaveFileExists(const std::string& working_directory);

}  // namespace persistence
