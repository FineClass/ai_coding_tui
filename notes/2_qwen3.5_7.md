# 💻 Textual 代码实现框架

基于我们优化的界面设计，以下是完整的 Textual 应用框架：

---

## 📁 项目结构

```
ai_coding_tui/
├── app.py                 # 主应用入口
├── models/                # 数据模型
│   ├── __init__.py
│   ├── project.py         # 项目模型
│   ├── task.py            # 任务模型
│   └── iteration.py       # 迭代模型
├── screens/               # 界面屏幕
│   ├── __init__.py
│   ├── home.py            # 主界面
│   ├── project_type.py    # 项目类型选择
│   └── dialogs.py         # 对话框
├── widgets/               # 自定义组件
│   ├── __init__.py
│   ├── project_nav.py     # 项目导航
│   ├── workspace.py       # 主工作区
│   ├── context_panel.py   # 上下文面板
│   └── status_bar.py      # 状态栏
├── services/              # 业务逻辑
│   ├── __init__.py
│   ├── workflow.py        # 工作流引擎
│   └── ai_client.py       # AI 客户端
└── styles/                # 样式
    └── main.tcss          # Textual CSS
```

---

## 1️⃣ 数据模型 (`models/`)

### `models/project.py`
```python
from enum import Enum
from dataclasses import dataclass, field
from typing import Optional
from datetime import datetime


class ProjectType(Enum):
    NEW_FEATURE = "new_feature"      # 新功能/优化功能
    BUG_FIX = "bug_fix"              # 修复缺陷


class ProjectStage(Enum):
    # 类型1阶段
    REQUIREMENT = "requirement"      # 需求探讨
    DESIGN = "design"                # 方案设计
    ITERATION = "iteration"          # 任务迭代
    
    # 类型2阶段
    PROBLEM_DEFINE = "problem_define"    # 明确问题
    SOLUTION = "solution"                # 修复方案
    EXECUTION = "execution"              # 执行迭代


class StageStatus(Enum):
    PENDING = "pending"      # 待开始 ⚪
    IN_PROGRESS = "in_progress"  # 进行中 🟢
    COMPLETED = "completed"  # 已完成 ✅


@dataclass
class Project:
    id: str
    name: str
    project_type: ProjectType
    created_at: datetime = field(default_factory=datetime.now)
    
    # 阶段状态
    current_stage: ProjectStage = ProjectStage.REQUIREMENT
    stage_statuses: dict = field(default_factory=dict)
    
    # 任务相关
    tasks: list = field(default_factory=list)
    current_task_index: int = 0
    
    # 迭代相关
    current_iteration: int = 0
    max_iterations: int = 5
    
    # 验收清单（类型1专用）
    acceptance_items: list = field(default_factory=list)
    
    # 文档路径
    requirement_doc: Optional[str] = None
    design_doc: Optional[str] = None
    
    def get_stage_status(self, stage: ProjectStage) -> StageStatus:
        return self.stage_statuses.get(stage, StageStatus.PENDING)
    
    def set_stage_status(self, stage: ProjectStage, status: StageStatus):
        self.stage_statuses[stage] = status
    
    def get_available_stages(self) -> list[ProjectStage]:
        """根据项目类型返回可用阶段"""
        if self.project_type == ProjectType.NEW_FEATURE:
            return [ProjectStage.REQUIREMENT, ProjectStage.DESIGN, ProjectStage.ITERATION]
        else:
            return [ProjectStage.PROBLEM_DEFINE, ProjectStage.SOLUTION, ProjectStage.EXECUTION]
```

### `models/task.py`
```python
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


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
    
    # 迭代信息
    iteration_count: int = 0
    max_iterations: int = 5
    
    # 测试信息
    test_passed: int = 0
    test_failed: int = 0
    test_total: int = 0
    
    # 代码信息
    code_files: list = field(default_factory=list)
    
    # 迭代历史
    iteration_history: list = field(default_factory=list)
    
    def can_continue_iteration(self) -> bool:
        return self.iteration_count < self.max_iterations
    
    def needs_human_intervention(self) -> bool:
        return self.iteration_count >= self.max_iterations and not self.is_completed()
    
    def is_completed(self) -> bool:
        return self.status == TaskStatus.COMPLETED
```

### `models/iteration.py`
```python
from dataclasses import dataclass
from datetime import datetime
from enum import Enum


class IterationResult(Enum):
    SUCCESS = "success"        # 🟢
    PARTIAL = "partial"        # 🟡
    FAILED = "failed"          # 🔴


@dataclass
class IterationRecord:
    iteration_num: int
    timestamp: datetime
    action: str                    # 代码生成/代码修改/测试等
    result: IterationResult
    message: str                   # 结果描述/错误信息
    test_passed: int = 0
    test_failed: int = 0
```

---

## 2️⃣ 主应用入口 (`app.py`)

```python
from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical
from textual.screen import Screen
from textual.widgets import Footer, Header

from models.project import Project, ProjectType, ProjectStage
from screens.project_type import ProjectTypeScreen
from screens.home import HomeScreen
from widgets.status_bar import StatusBar


class AICodingApp(App):
    CSS_PATH = "styles/main.tcss"
    
    def __init__(self):
        super().__init__()
        self.current_project: Project | None = None
    
    def on_mount(self) -> None:
        """应用启动时显示项目类型选择"""
        self.push_screen(ProjectTypeScreen())
    
    def create_project(self, project_type: ProjectType, name: str) -> None:
        """创建新项目并进入主界面"""
        self.current_project = Project(
            id=f"proj_{name.lower().replace(' ', '_')}",
            name=name,
            project_type=project_type,
        )
        # 初始化阶段状态
        for stage in self.current_project.get_available_stages():
            self.current_project.set_stage_status(stage, "pending")
        
        # 设置第一个阶段为进行中
        first_stage = self.current_project.get_available_stages()[0]
        self.current_project.set_stage_status(first_stage, "in_progress")
        self.current_project.current_stage = first_stage
        
        # 进入主界面
        self.push_screen(HomeScreen(self.current_project))
    
    def compose(self) -> ComposeResult:
        yield Header()
        yield Footer()


if __name__ == "__main__":
    app = AICodingApp()
    app.run()
```

---

## 3️⃣ 项目类型选择屏幕 (`screens/project_type.py`)

```python
from textual.screen import Screen
from textual.containers import Container, Vertical
from textual.widgets import Static, Button
from textual.app import ComposeResult
from textual.binding import Binding

from models.project import ProjectType


class ProjectTypeScreen(Screen):
    BINDINGS = [
        Binding("up", "cursor_up", "↑ 上"),
        Binding("down", "cursor_down", "↓ 下"),
        Binding("enter", "select", "确认"),
        Binding("q", "quit", "退出"),
    ]
    
    def __init__(self):
        super().__init__()
        self.selected_index = 0
        self.options = [
            (ProjectType.NEW_FEATURE, "新功能/优化功能", 
             "从零开发新功能、现有功能优化\n流程：需求探讨 → 方案设计 → 任务迭代"),
            (ProjectType.BUG_FIX, "修复缺陷",
             "Bug 修复、安全漏洞修补\n流程：明确问题 → 修复方案 → 执行迭代"),
        ]
    
    def compose(self) -> ComposeResult:
        yield Static("选择项目类型", classes="title")
        
        with Container(classes="type-selector"):
            for idx, (ptype, title, desc) in enumerate(self.options):
                yield self._create_option_card(idx, ptype, title, desc)
        
        yield Static("↑↓ 选择  Enter 确认  q 退出", classes="help-text")
    
    def _create_option_card(self, idx: int, ptype: ProjectType, title: str, desc: str):
        card = Container(
            Static(title, classes="card-title"),
            Static(desc, classes="card-desc"),
            Button("选择此类型", classes="select-btn"),
            classes="option-card",
            id=f"option-{idx}",
        )
        if idx == self.selected_index:
            card.add_class("selected")
        return card
    
    def action_cursor_up(self):
        self.selected_index = (self.selected_index - 1) % len(self.options)
        self._update_selection()
    
    def action_cursor_down(self):
        self.selected_index = (self.selected_index + 1) % len(self.options)
        self._update_selection()
    
    def _update_selection(self):
        for idx in range(len(self.options)):
            card = self.query_one(f"#option-{idx}", Container)
            if idx == self.selected_index:
                card.add_class("selected")
            else:
                card.remove_class("selected")
    
    def action_select(self):
        ptype, title, _ = self.options[self.selected_index]
        self.app.create_project(ptype, title)


# 样式 (styles/main.tcss)
"""
ProjectTypeScreen {
    align: center middle;
}

.title {
    text-align: center;
    text-style: bold;
    padding: 1 2;
}

.type-selector {
    layout: vertical;
    width: 80;
    height: auto;
    padding: 1 2;
}

.option-card {
    border: solid $primary;
    padding: 1 2;
    margin: 1 0;
}

.option-card.selected {
    border: solid $accent;
    background: $surface;
}

.card-title {
    text-style: bold;
    color: $text;
}

.card-desc {
    color: $text-muted;
    padding: 1 0;
}

.select-btn {
    align: center middle;
}

.help-text {
    text-align: center;
    padding: 1;
    color: $text-muted;
}
"""
```

---

## 4️⃣ 主界面 (`screens/home.py`)

```python
from textual.screen import Screen
from textual.containers import Container, Horizontal, Vertical
from textual.widgets import Static
from textual.app import ComposeResult

from models.project import Project
from widgets.project_nav import ProjectNav
from widgets.workspace import Workspace
from widgets.context_panel import ContextPanel
from widgets.status_bar import StatusBar


class HomeScreen(Screen):
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
    
    def compose(self) -> ComposeResult:
        # 主布局：三栏
        with Horizontal(classes="main-layout"):
            # 左侧：项目导航 (20%)
            yield ProjectNav(self.project, id="project-nav")
            
            # 中间：主工作区 (55%)
            yield Workspace(self.project, id="workspace")
            
            # 右侧：上下文面板 (25%)
            yield ContextPanel(self.project, id="context-panel")
        
        # 底部：状态栏
        yield StatusBar(self.project, id="status-bar")
    
    def on_project_stage_changed(self, event) -> None:
        """处理阶段切换事件"""
        self.project.current_stage = event.new_stage
        # 刷新所有组件
        self.query_one("#project-nav").refresh()
        self.query_one("#workspace").refresh()
        self.query_one("#context-panel").refresh()
        self.query_one("#status-bar").refresh()
    
    def on_task_updated(self, event) -> None:
        """处理任务更新事件"""
        self.query_one("#project-nav").refresh()
        self.query_one("#context-panel").refresh()
        self.query_one("#status-bar").refresh()
```

---

## 5️⃣ 项目导航组件 (`widgets/project_nav.py`)

```python
from textual.widgets import Static, Tree
from textual.app import ComposeResult
from textual.message import Message

from models.project import Project, ProjectStage, StageStatus


class ProjectNav(Static):
    """左侧项目导航"""
    
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
    
    def compose(self) -> ComposeResult:
        with Vertical(classes="nav-container"):
            # 项目信息
            yield Static(f"📁 {self.project.name}", classes="project-title")
            
            # 阶段列表
            yield Static("阶段列表", classes="section-title")
            yield self._create_stage_list()
            
            # 任务树
            yield Static("任务树", classes="section-title")
            yield self._create_task_tree()
    
    def _create_stage_list(self) -> Static:
        """创建阶段列表显示"""
        stages = self.project.get_available_stages()
        lines = []
        
        stage_icons = {
            StageStatus.PENDING: "⚪",
            StageStatus.IN_PROGRESS: "🔵",
            StageStatus.COMPLETED: "✅",
        }
        
        stage_names = {
            ProjectStage.REQUIREMENT: "① 需求",
            ProjectStage.DESIGN: "② 设计",
            ProjectStage.ITERATION: "③ 迭代",
            ProjectStage.PROBLEM_DEFINE: "① 明确",
            ProjectStage.SOLUTION: "② 方案",
            ProjectStage.EXECUTION: "③ 执行",
        }
        
        for stage in stages:
            status = self.project.get_stage_status(stage)
            icon = stage_icons.get(status, "⚪")
            name = stage_names.get(stage, stage.value)
            
            if stage == self.project.current_stage:
                lines.append(f"● {name}   {icon}")
            else:
                lines.append(f"  {name}   {icon}")
        
        return Static("\n".join(lines), classes="stage-list")
    
    def _create_task_tree(self) -> Static:
        """创建任务树显示"""
        if not self.project.tasks:
            return Static("(暂无)", classes="task-tree-empty")
        
        lines = []
        task_icons = {
            "pending": "⚪",
            "in_progress": "🔵",
            "completed": "✅",
            "blocked": "🔴",
        }
        
        for idx, task in enumerate(self.project.tasks):
            icon = task_icons.get(task.status.value, "⚪")
            prefix = "●" if idx == self.project.current_task_index else " "
            lines.append(f"{prefix} {icon} {task.name}")
        
        return Static("\n".join(lines), classes="task-tree")
    
    def refresh(self) -> None:
        """刷新导航显示"""
        self.remove_children()
        for child in self.compose():
            self.mount(child)
```

---

## 6️⃣ 主工作区组件 (`widgets/workspace.py`)

```python
from textual.widgets import Static, Input, Button, TextArea
from textual.containers import Container, Vertical
from textual.app import ComposeResult

from models.project import Project, ProjectStage


class Workspace(Static):
    """中间主工作区"""
    
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
    
    def compose(self) -> ComposeResult:
        with Container(classes="workspace-container"):
            # 根据当前阶段渲染不同内容
            if self.project.current_stage in [ProjectStage.REQUIREMENT, ProjectStage.PROBLEM_DEFINE]:
                yield self._render_discussion_view()
            elif self.project.current_stage in [ProjectStage.DESIGN, ProjectStage.SOLUTION]:
                yield self._render_design_view()
            elif self.project.current_stage in [ProjectStage.ITERATION, ProjectStage.EXECUTION]:
                yield self._render_iteration_view()
    
    def _render_discussion_view(self) -> Container:
        """需求探讨/问题明确视图"""
        return Container(
            Static("需求探讨" if self.project.current_stage == ProjectStage.REQUIREMENT else "明确问题", 
                   classes="view-title"),
            Static("AI: 请描述...", classes="ai-message"),
            Static("👤: 我想...", classes="user-message"),
            Input(placeholder="输入内容...", id="discussion-input"),
            Button("发送", id="send-btn"),
            classes="discussion-view",
        )
    
    def _render_design_view(self) -> Container:
        """方案设计/修复方案视图"""
        return Container(
            Static("方案设计" if self.project.current_stage == ProjectStage.DESIGN else "修复方案",
                   classes="view-title"),
            Static("架构图/方案内容...", classes="design-content"),
            Button("确认方案", id="confirm-design-btn"),
            Button("修改", id="modify-design-btn"),
            classes="design-view",
        )
    
    def _render_iteration_view(self) -> Container:
        """任务迭代/执行视图"""
        if self.project.current_stage == ProjectStage.ITERATION:
            # 类型1：显示局部+全局迭代
            return Container(
                Static("── 局部迭代 ──", classes="section-divider"),
                Static("任务: 实现登录接口", classes="task-info"),
                Static("状态: 🟡 迭代中 (2/5)", classes="iteration-status"),
                TextArea(code="...", language="python", read_only=True, classes="code-preview"),
                Static("测试结果: ❌ 2/5 通过", classes="test-result"),
                Button("继续迭代", id="continue-iteration-btn"),
                Button("人工介入", id="human-intervention-btn"),
                
                Static("── 全局迭代 ──", classes="section-divider"),
                Button("启动全局测试", id="global-test-btn"),
                Static("完成度: 2/5 任务", classes="progress-info"),
                classes="iteration-view",
            )
        else:
            # 类型2：简化迭代视图
            return Container(
                Static("执行迭代", classes="view-title"),
                Static("迭代记录...", classes="iteration-log"),
                Button("提交修复", id="submit-fix-btn"),
                classes="execution-view",
            )
    
    def refresh(self) -> None:
        """刷新工作区"""
        self.remove_children()
        for child in self.compose():
            self.mount(child)
```

---

## 7️⃣ 上下文面板组件 (`widgets/context_panel.py`)

```python
from textual.widgets import Static, Button
from textual.containers import Container, Vertical
from textual.app import ComposeResult

from models.project import Project, ProjectStage


class ContextPanel(Static):
    """右侧上下文面板 - 动态内容"""
    
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
    
    def compose(self) -> ComposeResult:
        with Vertical(classes="context-container"):
            # 根据阶段和项目类型动态渲染
            content = self._get_context_content()
            for widget in content:
                yield widget
    
    def _get_context_content(self) -> list:
        """根据当前阶段返回上下文内容"""
        stage = self.project.current_stage
        
        if self.project.project_type.value == "new_feature":
            return self._get_new_feature_context(stage)
        else:
            return self._get_bug_fix_context(stage)
    
    def _get_new_feature_context(self, stage: ProjectStage) -> list:
        """新功能类型的上下文"""
        if stage == ProjectStage.REQUIREMENT:
            return [
                Static("── 需求文档摘要 ──", classes="section-title"),
                Static("📄 条目数: 4/10\n📝 最后更新: 2分钟前", classes="summary-info"),
                Static("── 待澄清问题 ──", classes="section-title"),
                Static("• 用户类型有哪些？\n• 支持第三方登录吗？", classes="question-list"),
                Button("生成文档", id="gen-req-doc-btn"),
                Button("进入设计阶段", id="next-stage-btn", classes="primary-btn"),
            ]
        
        elif stage == ProjectStage.DESIGN:
            return [
                Static("── 设计文档摘要 ──", classes="section-title"),
                Static("📄 模块数: 3/5\n🏗️ 架构风格: MVC", classes="summary-info"),
                Static("── 技术选型 ──", classes="section-title"),
                Static("• 框架: FastAPI\n• 数据库: PostgreSQL", classes="tech-stack"),
                Static("── 风险项 ──", classes="section-title"),
                Static("⚠️ 第三方依赖: 2", classes="risk-item"),
                Button("进入任务迭代", id="next-stage-btn", classes="primary-btn"),
            ]
        
        elif stage == ProjectStage.ITERATION:
            return [
                Static("── 当前任务 ──", classes="section-title"),
                Static("🟡 状态: 迭代中\n🔄 迭代: 2/5\n⏱️ 耗时: 12分钟", classes="task-status"),
                Static("── 测试结果 ──", classes="section-title"),
                Static("✅ 通过: 3\n❌ 失败: 2", classes="test-summary"),
                Button("查看失败详情", id="view-failures-btn"),
                
                Static("── 验收清单 ──", classes="section-title"),
                Static("□ 登录验证 [3/5]\n□ 密码加密 [✅]\n□ 会话管理 [⚪]", classes="acceptance-list"),
                Button("执行全部验收测试", id="run-acceptance-btn", classes="primary-btn"),
            ]
        
        return [Static("无上下文信息", classes="empty-context")]
    
    def _get_bug_fix_context(self, stage: ProjectStage) -> list:
        """修复缺陷类型的上下文"""
        if stage == ProjectStage.PROBLEM_DEFINE:
            return [
                Static("── 问题详情 ──", classes="section-title"),
                Static("🔴 严重级别: 高\n📍 模块: 用户服务\n📋 Issue: #123", classes="issue-info"),
                Static("── 影响范围 ──", classes="section-title"),
                Static("• 登录功能\n• 用户信息接口", classes="impact-list"),
                Button("定位代码", id="locate-code-btn"),
                Button("进入修复方案", id="next-stage-btn", classes="primary-btn"),
            ]
        
        elif stage == ProjectStage.SOLUTION:
            return [
                Static("── 方案摘要 ──", classes="section-title"),
                Static("📝 修改文件: 1\n➕ 新增代码: 25 行", classes="solution-info"),
                Static("── 风险评估 ──", classes="section-title"),
                Static("🟢 风险等级: 低\n🟢 影响范围: 局部", classes="risk-assessment"),
                Button("确认方案", id="confirm-solution-btn", classes="primary-btn"),
            ]
        
        elif stage == ProjectStage.EXECUTION:
            return [
                Static("── 迭代状态 ──", classes="section-title"),
                Static("🟢 状态: 进行中\n🔄 迭代: 1/3", classes="iteration-status"),
                Static("── 回归测试 ──", classes="section-title"),
                Static("✅ 通过: 15/15", classes="regression-test"),
                Static("── 修改文件 ──", classes="section-title"),
                Static("📝 UserService.java:45-60", classes="modified-files"),
                Button("提交修复", id="submit-fix-btn", classes="primary-btn"),
            ]
        
        return [Static("无上下文信息", classes="empty-context")]
    
    def refresh(self) -> None:
        """刷新上下文面板"""
        self.remove_children()
        for child in self.compose():
            self.mount(child)
```

---

## 8️⃣ 状态栏组件 (`widgets/status_bar.py`)

```python
from textual.widgets import Static
from textual.app import ComposeResult

from models.project import Project, ProjectStage


class StatusBar(Static):
    """底部状态栏"""
    
    def __init__(self, project: Project):
        super().__init__()
        self.project = project
    
    def compose(self) -> ComposeResult:
        status_text = self._build_status_text()
        yield Static(status_text, classes="status-bar-text")
    
    def _build_status_text(self) -> str:
        """构建状态栏文本"""
        # 项目类型图标
        type_icon = "📁" if self.project.project_type.value == "new_feature" else "🔧"
        type_name = "新功能" if self.project.project_type.value == "new_feature" else "修复缺陷"
        
        # 当前阶段
        stage_map = {
            ProjectStage.REQUIREMENT: "① 需求",
            ProjectStage.DESIGN: "② 设计",
            ProjectStage.ITERATION: "③ 迭代",
            ProjectStage.PROBLEM_DEFINE: "① 明确",
            ProjectStage.SOLUTION: "② 方案",
            ProjectStage.EXECUTION: "③ 执行",
        }
        stage_text = stage_map.get(self.project.current_stage, "?")
        
        # 任务进度
        task_progress = f"{len([t for t in self.project.tasks if t.is_completed()])}/{len(self.project.tasks)}" if self.project.tasks else "0/0"
        
        # 迭代计数
        iteration_text = f"{self.project.current_iteration}/{self.project.max_iterations}"
        
        # 验收进度（仅类型1）
        if self.project.project_type.value == "new_feature" and self.project.acceptance_items:
            passed = len([i for i in self.project.acceptance_items if i.get("status") == "passed"])
            acceptance_text = f"{passed}/{len(self.project.acceptance_items)}"
        else:
            acceptance_text = "N/A"
        
        # 系统状态
        system_status = "🟢"  # 正常
        
        return f"[{type_icon} {type_name}] [{stage_text}] [任务: {task_progress}] [迭代: {iteration_text}] [验收: {acceptance_text}] [{system_status}]"
    
    def refresh(self) -> None:
        """刷新状态栏"""
        self.update(self._build_status_text())
```

---

## 9️⃣ 样式文件 (`styles/main.tcss`)

```css
/* 主布局 */
.main-layout {
    layout: horizontal;
    height: 1fr;
}

#project-nav {
    width: 20%;
    border-right: solid $primary;
    padding: 1 2;
}

#workspace {
    width: 55%;
    padding: 1 2;
}

#context-panel {
    width: 25%;
    border-left: solid $primary;
    padding: 1 2;
}

#status-bar {
    dock: bottom;
    height: 1;
    background: $surface;
    color: $text;
    padding: 0 2;
}

/* 导航样式 */
.project-title {
    text-style: bold;
    padding: 1 0;
}

.section-title {
    text-style: bold;
    color: $text-muted;
    padding: 1 0 0 0;
}

.stage-list {
    padding: 1 0;
}

.task-tree {
    padding: 1 0;
}

.task-tree-empty {
    color: $text-muted;
    font-style: italic;
}

/* 工作区样式 */
.workspace-container {
    height: 100%;
}

.view-title {
    text-style: bold;
    padding: 1 0;
}

.section-divider {
    color: $text-muted;
    padding: 1 0;
}

.code-preview {
    height: 15;
    border: solid $primary;
    margin: 1 0;
}

/* 上下文面板样式 */
.context-container {
    height: 100%;
}

.summary-info, .task-status, .test-summary, .issue-info {
    background: $surface;
    padding: 1;
    margin: 1 0;
}

.acceptance-list {
    padding: 1 0;
}

.primary-btn {
    margin: 1 0;
}

/* 状态栏样式 */
.status-bar-text {
    text-align: center;
}
```

---

## 🚀 运行应用

```bash
# 安装依赖
pip install textual

# 运行应用
python app.py
```

---

## 📋 框架特点总结

| 特性 | 实现方式 |
|------|---------|
| **双流程支持** | `ProjectType` 枚举区分，动态渲染 |
| **阶段管理** | `ProjectStage` + `StageStatus` 状态机 |
| **动态上下文** | `ContextPanel` 根据阶段返回不同内容 |
| **无冗余信息** | 阶段仅在导航显示，侧边显示上下文 |
| **可扩展性** | 组件化设计，易于添加新功能 |
| **状态同步** | 事件驱动刷新机制 |

---

需要我继续完善：
- 🔄 工作流引擎 (`services/workflow.py`) 实现？
- 💬 AI 对话集成 (`services/ai_client.py`)？
- 🧪 验收测试清单自动生成逻辑？
- 📦 完整的可运行示例项目？