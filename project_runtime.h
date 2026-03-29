#pragma once

#include "domain.h"

namespace runtime {

StageExecutionSummary BuildStageSummary(const Project& project, ProjectStage& stage);
std::string BuildStageOutput(const Project& project, const ProjectStage& stage);

}  // namespace runtime
