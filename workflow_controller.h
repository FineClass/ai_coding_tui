#pragma once

#include "domain.h"

namespace workflow {

int CountPassedAcceptanceTests(const ProjectStage& stage);
bool CanCompleteStage(const ProjectStage& stage);
void SyncDerivedProjectState(Project& project);
bool PrepareCurrentStageForExecution(Project& project, bool is_processing, int& processing_progress);
bool CompleteCurrentStage(Project& project, bool& is_processing);
bool RetryCurrentStage(Project& project, bool& is_processing);
bool ConfirmNextAcceptanceTest(ProjectStage& stage);

}  // namespace workflow
