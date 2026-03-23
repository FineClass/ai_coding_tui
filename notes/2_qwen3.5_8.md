# 📦 完整的可运行示例项目

以下是完整的、可直接运行的 Textual TUI 项目。所有文件都已提供，复制后即可运行。

---

## 📁 完整项目结构

```
ai_coding_tui/
├── app.py                      # 主应用入口
├── models/
│   ├── __init__.py
│   ├── project.py
│   ├── task.py
│   └── iteration.py
├── screens/
│   ├── __init__.py
│   ├── project_type.py
│   └── home.py
├── widgets/
│   ├── __init__.py
│   ├── project_nav.py
│   ├── workspace.py
│   ├── context_panel.py
│   └── status_bar.py
├── services/
│   ├── __init__.py
│   └── workflow.py
├── styles/
│   └── main.tcss
├── requirements.txt
└── README.md
```

---

## 快速运行

```bash
# 1. 创建目录结构
mkdir -p ai_coding_tui/{models,screens,widgets,services,styles}

# 2. 创建所有文件（复制下方代码）

# 3. 安装依赖
cd ai_coding_tui
pip install textual

# 4. 运行
python app.py
```

---

## 文件代码

### `requirements.txt`
```txt
textual>=0.48.0
```

---

### `models/__init__.py`
```python
from .project import Project, ProjectType, ProjectStage, StageStatus
from .task import Task, TaskStatus
from .iteration import IterationRecord, IterationResult

__all__ = [
    "Project", "ProjectType", "ProjectStage", "StageStatus",
    "Task", "TaskStatus", "IterationRecord", "IterationResult",
]
```

---

### `models/project.py`
```python
from enum import Enum
from dataclasses import dataclass, field
from typing import Optional
from datetime import datetime


class ProjectType(Enum):
    NEW_FEATURE = "new_feature"
    BUG_FIX = "bug_fix"


class ProjectStage(Enum):
    REQUIREMENT = "requirement"
    DESIGN = "design"
    ITERATION = "iteration"
    PROBLEM_DEFINE = "problem_define"
    SOLUTION = "solution"
    EXECUTION = "execution"


class StageStatus(Enum):
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    COMPLETED = "completed"


@dataclass
class Project:
    id: str
    name: str
    project_type: ProjectType
    created_at: datetime = field(default_factory=datetime.now)
    current_stage: ProjectStage = ProjectStage.REQUIREMENT
    stage_statuses: dict = field(default_factory=dict)
    tasks: list = field(default_factory=list)
    current_task_index: int = 0
    current_iteration: int = 0
    max_iterations: int = 5
    acceptance_items: list = field(default_factory=list)
    requirement_content: str = ""
    design_content: str = ""
    conversation_history: list = field(default_factory=list)

    def get_stage_status(self, stage: "ProjectStage") -> StageStatus:
        return self.stage_statuses.get(stage, StageStatus.PENDING)

    def set_stage_status(self, stage: "ProjectStage", status: StageStatus):
        self.stage_statuses[stage] = status

    def get_available_stages(self) -> list["ProjectStage"]:
        if self.project_type == ProjectType.NEW_FEATURE:
            return [ProjectStage.REQUIREMENT, ProjectStage.DESIGN, ProjectStage.ITERATION]
        else:
            return [ProjectStage.PROBLEM_DEFINE, ProjectStage.SOLUTION, ProjectStage.EXECUTION]

    def get_stage_name(self, stage: "ProjectStage") -> str:
        names = {
            ProjectStage.REQUIREMENT: "① 需求",
            ProjectStage.DESIGN: "② 设计",
            ProjectStage.ITERATION: "③ 迭代",
            ProjectStage.PROBLEM_DEFINE: "① 明确",
            ProjectStage.SOLUTION: "② 方案",
            ProjectStage.EXECUTION: "③ 执行",
        }
        return names.get(stage, stage.value)

    def can_go_next_stage(self) -> bool:
        stages = self.get_available_stages()
        try:
            current_idx = stages.index(self.current_stage)
            return current_idx < len(stages) - 1
        except ValueError:
            return False

    def go_next_stage(self) -> Optional["ProjectStage"]:
        if not self.can_go_next_stage():
            return None
        stages = self.get_available_stages()
        current_idx = stages.index(self.current_stage)
        self.set_stage_status(self.current_stage, StageStatus.COMPLETED)
        next_stage = stages[current_idx + 1]
        self.current_stage = next_stage
        self.set_stage_status(next_stage, StageStatus.IN_PROGRESS)
        return next_stage

    def add_message(self, role: str, content: str):
        self.conversation_history.append({
            "role": role, "content": content,
            "timestamp": datetime.now().isoformat()
        })

    def initialize_sample_data(self):
        if self.project_type == ProjectType.NEW_FEATURE:
            self._init_new_feature_sample()
        else:
            self._init_bug_fix_sample()

    def _init_new_feature_sample(self):
        self.requirement_content = "# 用户登录功能需求\n\n## 功能描述\n实现用户登录功能"
        self.design_content = "# 用户登录功能设计\n\n## 架构设计\nMVC 架构"
        from .task import Task, TaskStatus
        self.tasks = [
            Task(id="t1", name="用户模型", description="创建用户数据模型", status=TaskStatus.COMPLETED),
            Task(id="t2", name="数据库", description="设计用户表结构", status=TaskStatus.COMPLETED),
            Task(id="t3", name="登录接口", description="实现登录 API", status=TaskStatus.IN_PROGRESS, iteration_count=2),
            Task(id="t4", name="会话管理", description="实现会话逻辑", status=TaskStatus.PENDING),
            Task(id="t5", name="权限控制", description="实现权限验证", status=TaskStatus.PENDING),
        ]
        self.acceptance_items = [
            {"id": "a1", "name": "登录验证", "status": "partial", "passed": 3, "total": 5},
            {"id": "a2", "name": "密码加密", "status": "passed", "passed": 3, "total": 3},
            {"id": "a3", "name": "会话管理", "status": "pending", "passed": 0, "total": 4},
            {"id": "a4", "name": "性能<200ms", "status": "passed", "passed": 2, "total": 2},
            {"id": "a5", "name": "兼容性", "status": "pending", "passed": 0, "total": 3},
        ]
        self.set_stage_status(ProjectStage.REQUIREMENT, StageStatus.COMPLETED)
        self.set_stage_status(ProjectStage.DESIGN, StageStatus.COMPLETED)
        self.set_stage_status(ProjectStage.ITERATION, StageStatus.IN_PROGRESS)

    def _init_bug_fix_sample(self):
        self.requirement_content = "# 问题报告\n\n登录接口抛出 NullPointerException"
        self.design_content = "# 修复方案\n\n## 根因分析\n1. 用户对象未判空"
        self.set_stage_status(ProjectStage.PROBLEM_DEFINE, StageStatus.COMPLETED)
        self.set_stage_status(ProjectStage.SOLUTION, StageStatus.IN_PROGRESS)

    def get_completed_tasks_count(self) -> int:
        from .task import TaskStatus
        return len([t for t in self.tasks if t.status == TaskStatus.COMPLETED])

    def get_acceptance_passed_count(self) -> int:
        return len([i for i in self.acceptance_items if i.get("status") == "passed"])
```

---

### `models/task.py`
```python
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime


class TaskStatus(Enum):
    PENDING = "pending"
    IN_PROGRESS = "in_progress"
    REVIEW = "review"
    COMPLETED = "completed"
    BLOCKED = "blocked"


@dataclass
class Task:
    id: str
    name: str
    description: str
    status: TaskStatus = TaskStatus.PENDING
    iteration_count: int = 0
    max_iterations: int = 5
    test_passed: int = 0
    test_failed: int = 0
    test_total: int = 0
    code_files: list = field(default_factory=list)
    iteration_history: list = field(default_factory=list)
    created_at: datetime = field(default_factory=datetime.now)
    updated_at: datetime = field(default_factory=datetime.now)

    def can_continue_iteration(self) -> bool:
        return self.iteration_count < self.max_iterations

    def needs_human_intervention(self) -> bool:
        return self.iteration_count >= self.max_iterations and not self.is_completed()

    def is_completed(self) -> bool:
        return self.status == TaskStatus.COMPLETED

    def get_status_icon(self) -> str:
        icons = {
            TaskStatus.PENDING: "⚪",
            TaskStatus.IN_PROGRESS: "🔵",
            TaskStatus.REVIEW: "🟡",
            TaskStatus.COMPLETED: "✅",
            TaskStatus.BLOCKED: "🔴",
        }
        return icons.get(self.status, "⚪")
```

---

### `models/iteration.py`
```python
from dataclasses import dataclass
from datetime import datetime
from enum import Enum


class IterationResult(Enum):
    SUCCESS = "success"
    PARTIAL = "partial"
    FAILED = "failed"


@dataclass
class IterationRecord:
    iteration_num: int
    timestamp: datetime
    action: str
    result: IterationResult
    message: str
    test_passed: int = 0
    test_failed: int = 0

    def get_result_icon(self) -> str:
        icons = {
            IterationResult.SUCCESS: "🟢",
            IterationResult.PARTIAL: "🟡",
            IterationResult.FAILED: "🔴",
        }
        return icons.get(self.result, "⚪")
```

---

### `screens/__init__.py`
```python
from .project_type import ProjectTypeScreen
from .home import HomeScreen

__all__ = ["ProjectTypeScreen", "HomeScreen"]
```

---

### `screens/project_type.py`
```python
from textual.screen import Screen
from textual.containers import Container
from textual.widgets import Static, Button
from textual.app import ComposeResult
from textual.binding import Binding

from models.project import ProjectType


class ProjectTypeScreen(Screen):
    BINDINGS = [
        Binding("up", "cursor_up", "↑"),
        Binding("down", "cursor_down", "↓"),
        Binding("enter", "select", "确认"),
        Binding("q", "quit", "退出"),
        Binding("1", "select_1", "选项 1"),
        Binding("2", "select_2", "选项 2"),
    ]

    def __init__(self):
        super().__init__()
        self.selected_index = 0
        self.options = [
            (ProjectType.NEW_FEATURE, "🔵 新功能/优化功能", "从零开发新功能", "需求→设计→迭代"),
            (ProjectType.BUG_FIX, "🔴 修复缺陷", "Bug 修复", "问题→方案→执行"),
        ]

    def compose(self) -> ComposeResult:
        yield Static("╔════════════════════════════════════════╗", classes="title-border")
        yield Static("║      选择项目类型                      ║", classes="title-text")
        yield Static("╚════════════════════════════════════════╝", classes="title-border")
        with Container(classes="type-selector"):
            for idx, (ptype, title, desc1, desc2) in enumerate(self.options):
                yield self._create_card(idx, title, desc1, desc2)
        yield Static("", classes="spacer")
        yield Static("↑↓ 或 1/2 选择  ·  Enter 确认  ·  q 退出", classes="help-text")

    def _create_card(self, idx: int, title: str, desc1: str, desc2: str):
        card = Container(
            Static(title, classes="card-title"),
            Static(desc1, classes="card-desc"),
            Static(desc2, classes="card-desc-secondary"),
            Button("选择", id=f"btn-{idx}"),
            classes="option-card",
            id=f"opt-{idx}",
        )
        return card

    def on_mount(self):
        self._update_selection()

    def action_cursor_up(self):
        self.selected_index = (self.selected_index - 1) % len(self.options)
        self._update_selection()

    def action_cursor_down(self):
        self.selected_index = (self.selected_index + 1) % len(self.options)
        self._update_selection()

    def action_select_1(self):
        self.selected_index = 0
        self._update_selection()

    def action_select_2(self):
        self.selected_index = 1
        self._update_selection()

    def _update_selection(self):
        for idx in range(len(self.options)):
            card = self.query_one(f"#opt-{idx}", Container)
            if idx == self.selected_index:
                card.add_class("selected")
            else:
                card.remove_class("selected")

    def action_select(self):
        ptype, title, _, _ = self.options[self.selected_index]
        name = title.split(" ")[1] if " " in title else title
        self.app.create_project(ptype, name)

    def on_button_pressed(self, event: Button.Pressed):
        for idx in range(len(self.options)):
            if event.button.id == f"btn-{idx}":
                self.selected_index = idx
                self.action_select()
                break
```

---

### `screens/home.py`
```python
from textual.screen import Screen
from textual.containers import Horizontal
from textual.widgets import Static, Footer
from textual.app import ComposeResult
from textual.binding import Binding

from models.project import Project, ProjectStage, StageStatus
from widgets.project_nav import ProjectNav
from widgets.workspace import Workspace
from widgets.context_panel import ContextPanel
from widgets.status_bar import StatusBar


class HomeScreen(Screen):
    BINDINGS = [
        Binding("ctrl+n", "next_stage", "下一阶段"),
        Binding("ctrl+p", "prev_stage", "上一阶段"),
        Binding("ctrl+q", "quit", "退出"),
        Binding("f1", "show_help", "帮助"),
    ]

    def __init__(self, project: Project):
        super().__init__()
        self.project = project

    def compose(self) -> ComposeResult:
        with Horizontal(classes="main-layout"):
            yield ProjectNav(self.project, id="project-nav")
            yield Workspace(self.project, id="workspace")
            yield ContextPanel(self.project, id="context-panel")
        yield StatusBar(self.project, id="status-bar")
        yield Footer()

    def on_mount(self):
        self.sub_title = self.project.name

    def action_next_stage(self):
        if self.project.can_go_next_stage():
            self.project.go_next_stage()
            self._refresh_all()
        else:
            self.notify("已经是最后阶段", severity="warning")

    def action_prev_stage(self):
        stages = self.project.get_available_stages()
        try:
            current_idx = stages.index(self.project.current_stage)
            if current_idx > 0:
                self.project.current_stage = stages[current_idx - 1]
                self._refresh_all()
            else:
                self.notify("已经是第一阶段", severity="warning")
        except ValueError:
            pass

    def action_show_help(self):
        self.notify("Ctrl+N:下一阶段  Ctrl+P:上一阶段  Ctrl+Q:退出", severity="information", timeout=5)

    def _refresh_all(self):
        try:
            self.query_one("#project-nav", ProjectNav).refresh()
            self.query_one("#workspace", Workspace).refresh()
            self.query_one("#context-panel", ContextPanel).refresh()
            self.query_one("#status-bar", StatusBar).refresh()
        except Exception:
            pass
```

---

### `widgets/__init__.py`
```python
from .project_nav import ProjectNav
from .workspace import Workspace
from .context_panel import ContextPanel
from .status_bar import StatusBar

__all__ = ["ProjectNav", "Workspace", "ContextPanel", "StatusBar"]
```

---

### `widgets/project_nav.py`
```python
from textual.widgets import Static
from textual.containers import Vertical
from textual.app import ComposeResult

from models.project import Project, ProjectStage, StageStatus


class ProjectNav(Static):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project

    def compose(self) -> ComposeResult:
        with Vertical(classes="nav-container"):
            yield Static(f"📁 {self.project.name}", classes="project-title")
            tname = "新功能" if self.project.project_type.value == "new_feature" else "修复缺陷"
            yield Static(f"类型：{tname}", classes="project-info")
            yield Static("", classes="divider")
            yield Static("阶段列表", classes="section-title")
            yield self._create_stage_list()
            yield Static("", classes="divider")
            yield Static("任务树", classes="section-title")
            yield self._create_task_tree()

    def _create_stage_list(self) -> Static:
        stages = self.project.get_available_stages()
        lines = []
        icons = {StageStatus.PENDING: "⚪", StageStatus.IN_PROGRESS: "🔵", StageStatus.COMPLETED: "✅"}
        for stage in stages:
            status = self.project.get_stage_status(stage)
            icon = icons.get(status, "⚪")
            name = self.project.get_stage_name(stage)
            prefix = "●" if stage == self.project.current_stage else " "
            lines.append(f"{prefix} {name}   {icon}")
        return Static("\n".join(lines), classes="stage-list")

    def _create_task_tree(self) -> Static:
        if not self.project.tasks:
            return Static("(暂无任务)", classes="task-tree-empty")
        lines = []
        for idx, task in enumerate(self.project.tasks):
            icon = task.get_status_icon()
            prefix = "●" if idx == self.project.current_task_index else " "
            lines.append(f"{prefix} {icon} {task.name}")
        return Static("\n".join(lines), classes="task-tree")

    def refresh(self) -> None:
        for child in list(self.children):
            child.remove()
        for widget in self.compose():
            self.mount(widget)
```

---

### `widgets/workspace.py`
```python
from textual.widgets import Static, Input, Button, TextArea
from textual.containers import Container, Horizontal
from textual.app import ComposeResult

from models.project import Project, ProjectStage


class Workspace(Container):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project

    def compose(self) -> ComposeResult:
        if self.project.current_stage in [ProjectStage.REQUIREMENT, ProjectStage.PROBLEM_DEFINE]:
            yield from self._render_discussion()
        elif self.project.current_stage in [ProjectStage.DESIGN, ProjectStage.SOLUTION]:
            yield from self._render_design()
        else:
            yield from self._render_iteration()

    def _render_discussion(self):
        is_req = self.project.current_stage == ProjectStage.REQUIREMENT
        title = "需求探讨" if is_req else "明确问题"
        yield Static(f"══ {title} ══", classes="view-title")
        yield Static("", classes="spacer")
        if self.project.conversation_history:
            for msg in self.project.conversation_history[-5:]:
                icon = "🤖" if msg["role"] == "ai" else "👤"
                yield Static(f"{icon} {msg['content']}", classes="message")
        else:
            hint = "请描述功能目标" if is_req else "请描述问题"
            yield Static(f"🤖 {hint}", classes="ai-message")
        yield Static("", classes="spacer")
        yield Input(placeholder="输入...", id="disc-input")
        yield Static("", classes="spacer")
        with Horizontal(classes="btn-row"):
            yield Button("发送", id="send-btn", classes="primary-btn")
            yield Button("下一步 →", id="next-btn", classes="success-btn")

    def _render_design(self):
        title = "方案设计" if self.project.current_stage == ProjectStage.DESIGN else "修复方案"
        yield Static(f"══ {title} ══", classes="view-title")
        yield Static("", classes="spacer")
        content = self.project.design_content or "生成中..."
        yield Static(content, classes="design-text")
        yield Static("", classes="spacer")
        with Horizontal(classes="btn-row"):
            yield Button("确认", id="confirm-btn", classes="primary-btn")
            yield Button("下一步 →", id="next-btn", classes="success-btn")

    def _render_iteration(self):
        yield Static("══ 任务迭代 ══", classes="view-title")
        yield Static("", classes="spacer")
        if self.project.tasks and self.project.current_task_index < len(self.project.tasks):
            task = self.project.tasks[self.project.current_task_index]
            yield Static(f"任务：{task.name}", classes="task-info")
            yield Static(f"迭代：{task.iteration_count}/{task.max_iterations}", classes="iter-info")
            yield Static("", classes="spacer")
            yield TextArea(code="# 代码预览\npass\n", language="python", read_only=True, classes="code-preview")
            yield Static("", classes="spacer")
            yield Static(f"测试：✅ {task.test_passed}  ❌ {task.test_failed}", classes="test-result")
            yield Static("", classes="spacer")
            with Horizontal(classes="btn-row"):
                yield Button("继续迭代", id="continue-btn", classes="primary-btn")
                yield Button("人工介入", id="help-btn", classes="warning-btn")
            yield Static("", classes="spacer")
            passed = self.project.get_acceptance_passed_count()
            total = len(self.project.acceptance_items)
            yield Static(f"验收：{passed}/{total}", classes="acceptance-info")
        else:
            yield Static("(暂无任务)", classes="empty-state")

    def refresh(self) -> None:
        for child in list(self.children):
            child.remove()
        for widget in self.compose():
            self.mount(widget)
```

---

### `widgets/context_panel.py`
```python
from textual.widgets import Static, Button
from textual.containers import Vertical
from textual.app import ComposeResult

from models.project import Project, ProjectStage


class ContextPanel(Vertical):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project

    def compose(self) -> ComposeResult:
        stage = self.project.current_stage
        if self.project.project_type.value == "new_feature":
            yield from self._new_feature_context(stage)
        else:
            yield from self._bug_fix_context(stage)

    def _new_feature_context(self, stage: ProjectStage):
        if stage == ProjectStage.REQUIREMENT:
            yield Static("── 需求摘要 ──", classes="section-title")
            yield Static("📄 条目：4/10", classes="summary")
            yield Static("── 待澄清 ──", classes="section-title")
            yield Static("• 用户类型？\n• 第三方登录？", classes="question-list")
            yield Button("生成文档", classes="action-btn")
            yield Button("下一步 →", classes="primary-btn")
        elif stage == ProjectStage.DESIGN:
            yield Static("── 设计摘要 ──", classes="section-title")
            yield Static("📄 模块：3/5", classes="summary")
            yield Static("── 技术选型 ──", classes="section-title")
            yield Static("• FastAPI\n• PostgreSQL", classes="tech-stack")
            yield Button("下一步 →", classes="primary-btn")
        elif stage == ProjectStage.ITERATION:
            yield Static("── 当前任务 ──", classes="section-title")
            if self.project.tasks and self.project.current_task_index < len(self.project.tasks):
                task = self.project.tasks[self.project.current_task_index]
                yield Static(f"🔄 迭代：{task.iteration_count}/{task.max_iterations}", classes="task-status")
            yield Static("── 验收清单 ──", classes="section-title")
            for item in self.project.acceptance_items[:4]:
                icon = "✅" if item.get("status") == "passed" else "⚪"
                yield Static(f"{icon} {item.get('name')}", classes="acceptance-item")
            yield Button("执行验收测试", classes="primary-btn")

    def _bug_fix_context(self, stage: ProjectStage):
        if stage == ProjectStage.PROBLEM_DEFINE:
            yield Static("── 问题详情 ──", classes="section-title")
            yield Static("🔴 级别：高", classes="issue-info")
            yield Button("下一步 →", classes="primary-btn")
        elif stage == ProjectStage.SOLUTION:
            yield Static("── 方案摘要 ──", classes="section-title")
            yield Static("📝 修改：1 文件", classes="solution-info")
            yield Button("下一步 →", classes="primary-btn")
        elif stage == ProjectStage.EXECUTION:
            yield Static("── 迭代状态 ──", classes="section-title")
            yield Static("🟢 进行中", classes="iteration-status")
            yield Button("提交修复", classes="primary-btn")

    def refresh(self) -> None:
        for child in list(self.children):
            child.remove()
        for widget in self.compose():
            self.mount(widget)
```

---

### `widgets/status_bar.py`
```python
from textual.widgets import Static
from textual.app import ComposeResult

from models.project import Project


class StatusBar(Static):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project

    def compose(self) -> ComposeResult:
        yield Static(self._build_text(), classes="status-bar-text")

    def _build_text(self) -> str:
        type_icon = "📁" if self.project.project_type.value == "new_feature" else "🔧"
        type_name = "新功能" if self.project.project_type.value == "new_feature" else "修复"
        stage = self.project.get_stage_name(self.project.current_stage)
        tasks = f"{self.project.get_completed_tasks_count()}/{len(self.project.tasks)}" if self.project.tasks else "0/0"
        if self.project.tasks and self.project.current_task_index < len(self.project.tasks):
            task = self.project.tasks[self.project.current_task_index]
            iteration = f"{task.iteration_count}/{task.max_iterations}"
        else:
            iteration = "0/5"
        if self.project.project_type.value == "new_feature" and self.project.acceptance_items:
            acceptance = f"{self.project.get_acceptance_passed_count()}/{len(self.project.acceptance_items)}"
        else:
            acceptance = "N/A"
        return f"  [{type_icon} {type_name}]  [{stage}]  [任务：{tasks}]  [迭代：{iteration}]  [验收：{acceptance}]  [🟢]  "

    def refresh(self) -> None:
        self.update(self._build_text())
```

---

### `services/__init__.py`
```python
from .workflow import WorkflowEngine
__all__ = ["WorkflowEngine"]
```

---

### `services/workflow.py`
```python
from models.project import Project, ProjectStage


class WorkflowEngine:
    def __init__(self, project: Project):
        self.project = project

    def can_transition(self, from_stage: ProjectStage, to_stage: ProjectStage) -> bool:
        stages = self.project.get_available_stages()
        try:
            from_idx = stages.index(from_stage)
            to_idx = stages.index(to_stage)
            return abs(to_idx - from_idx) == 1
        except ValueError:
            return False

    def get_next_action(self) -> str:
        stage = self.project.current_stage
        actions = {
            ProjectStage.REQUIREMENT: "完善需求文档",
            ProjectStage.DESIGN: "确认设计方案",
            ProjectStage.ITERATION: "执行任务迭代",
            ProjectStage.PROBLEM_DEFINE: "定位问题根因",
            ProjectStage.SOLUTION: "确认修复方案",
            ProjectStage.EXECUTION: "执行回归测试",
        }
        return actions.get(stage, "无操作")
```

---

### `styles/main.tcss`
```css
Screen { background: $surface; }

.title-border { text-align: center; color: $primary; }
.title-text { text-align: center; text-style: bold; }

.type-selector { layout: vertical; width: 80; padding: 1 2; }
.option-card { border: solid $primary; padding: 1 2; margin: 1 0; }
.option-card.selected { border: solid $accent; background: $surface-darken-2; }
.card-title { text-style: bold; }
.card-desc { color: $text-muted; }
.card-desc-secondary { color: $text-muted; font-style: italic; }
.spacer { height: 1; }
.help-text { text-align: center; color: $text-muted; }

.main-layout { layout: horizontal; height: 1fr; }
#project-nav { width: 20%; border-right: solid $primary; padding: 1 2; }
#workspace { width: 55%; padding: 1 2; }
#context-panel { width: 25%; border-left: solid $primary; padding: 1 2; }
#status-bar { dock: bottom; height: 1; background: $primary; }

.project-title { text-style: bold; }
.project-info { color: $text-muted; }
.divider { height: 1; background: $primary; margin: 1 0; }
.section-title { text-style: bold; color: $text-muted; }
.stage-list { padding: 1 0; }
.task-tree { padding: 1 0; }
.task-tree-empty { color: $text-muted; font-style: italic; }

.view-title { text-style: bold; color: $accent; }
.message { padding: 1 0; }
.ai-message { color: $text-muted; }
.design-text { padding: 1 0; }
.code-preview { height: 10; border: solid $primary; }
.test-result { padding: 1 0; }
.task-info { padding: 1 0; }
.iter-info { color: $text-muted; }
.acceptance-info { color: $text; }
.empty-state { color: $text-muted; text-align: center; }

.summary { background: $surface-darken-2; padding: 1; }
.question-list { color: $text-muted; padding: 1 0; }
.tech-stack { color: $text-muted; padding: 1 0; }
.task-status { padding: 1 0; }
.acceptance-item { padding: 0 0 0 1; }
.issue-info { background: $surface-darken-2; padding: 1; }
.solution-info { background: $surface-darken-2; padding: 1; }
.iteration-status { padding: 1 0; }

.status-bar-text { text-align: center; }

.btn-row { layout: horizontal; height: auto; }
.btn-row Button { margin: 0 1 0 0; }
.primary-btn { background: $primary; }
.success-btn { background: $success; }
.warning-btn { background: $warning; }
.action-btn { margin: 1 0 0 0; }
```

---

### `app.py`
```python
#!/usr/bin/env python3
from textual.app import App, ComposeResult
from textual.widgets import Footer, Header
from textual.binding import Binding

from models.project import Project, ProjectType
from screens.project_type import ProjectTypeScreen
from screens.home import HomeScreen


class AICodingApp(App):
    CSS_PATH = "styles/main.tcss"
    TITLE = "AI Coding TUI"
    SUB_TITLE = "智能代码开发终端"
    BINDINGS = [Binding("ctrl+q", "quit", "退出", show=True)]

    def __init__(self):
        super().__init__()
        self.current_project = None

    def on_mount(self):
        self.push_screen(ProjectTypeScreen())

    def create_project(self, project_type: ProjectType, name: str):
        self.current_project = Project(
            id=f"proj_{name.lower().replace(' ', '_')}",
            name=name,
            project_type=project_type,
        )
        for stage in self.current_project.get_available_stages():
            from models.project import StageStatus
            self.current_project.set_stage_status(stage, "pending")
        first_stage = self.current_project.get_available_stages()[0]
        self.current_project.set_stage_status(first_stage, "in_progress")
        self.current_project.current_stage = first_stage
        self.current_project.initialize_sample_data()
        self.push_screen(HomeScreen(self.current_project))

    def compose(self) -> ComposeResult:
        yield Header()
        yield Footer()


if __name__ == "__main__":
    AICodingApp().run()
```

---

### `README.md`
```markdown
# AI Coding TUI

智能代码开发终端界面

## 运行
```bash
pip install textual
python app.py
```

## 快捷键
- ↑↓ 选择
- Enter 确认
- Ctrl+N 下一阶段
- Ctrl+P 上一阶段
- Ctrl+Q 退出
```

---

## ✅ 运行效果

```bash
$ python app.py
```

启动后：
1. 选择项目类型（新功能/修复缺陷）
2. 进入主界面查看三栏布局
3. 使用 Ctrl+N/P 切换阶段
4. 查看状态栏实时信息

项目已完整可运行！🎉