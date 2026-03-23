
可以。下面我给你一个用 **Qt5 实现“类 Claude Code 的 IDE 工具”** 的完整思路和一个**可落地的基础版本代码框架**。

你这里的“类 cluade code”我理解为：

- 左侧：项目文件树
- 中间：代码编辑器
- 右侧：AI 助手聊天面板
- 底部：终端 / 日志输出
- 支持打开工程、编辑文件、保存文件、执行命令、与大模型交互

如果你愿意，我后面还可以继续帮你扩展成：

1. **完整可编译工程**
2. **接入 OpenAI / Claude / 本地大模型 API**
3. **代码高亮 / 自动补全 / LSP**
4. **多标签编辑器**
5. **类似 VS Code 的布局与主题**

---

# 一、功能目标

先定义一个基础版 IDE：

## 1. 核心功能
- 项目目录浏览
- 多文件打开
- 代码编辑
- 文件保存
- 底部终端输出
- AI 对话面板
- AI 根据当前文件内容给出建议

## 2. 可选增强
- 语法高亮
- 行号
- 自动补全
- 查找替换
- Git 面板
- 插件机制
- LSP（Language Server Protocol）

---

# 二、Qt5 技术选型

推荐使用 **Qt Widgets**，因为更适合 IDE 桌面工具开发。

## 主要组件
- `QMainWindow`：主窗口
- `QTreeView + QFileSystemModel`：项目文件树
- `QTabWidget`：多标签编辑器
- `QPlainTextEdit`：代码编辑器
- `QTextEdit`：AI 聊天面板
- `QLineEdit / QTextEdit`：输入问题
- `QProcess`：执行 shell 命令
- `QSplitter`：可拖拽布局
- `QDockWidget`：侧边栏/底部面板
- `QNetworkAccessManager`：调用 AI API

---

# 三、整体架构设计

建议拆成以下类：

```text
MainWindow                主界面
 ├── ProjectTree          项目文件树
 ├── EditorManager        管理多个编辑器标签
 │    └── CodeEditor      代码编辑器
 ├── TerminalWidget       终端输出
 ├── ChatWidget           AI 助手聊天窗口
 └── AiService            调用大模型接口
```

---

# 四、项目目录结构

```text
ClaudeLikeIDE/
├── main.cpp
├── mainwindow.h
├── mainwindow.cpp
├── codeeditor.h
├── codeeditor.cpp
├── chatwidget.h
├── chatwidget.cpp
├── terminalwidget.h
├── terminalwidget.cpp
├── aiservice.h
├── aiservice.cpp
├── syntaxhighlighter.h
├── syntaxhighlighter.cpp
└── ClaudeLikeIDE.pro
```

---

# 五、基础工程代码

---

## 1）`ClaudeLikeIDE.pro`

```pro
QT += core gui widgets network

CONFIG += c++11

TARGET = ClaudeLikeIDE
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    codeeditor.cpp \
    chatwidget.cpp \
    terminalwidget.cpp \
    aiservice.cpp \
    syntaxhighlighter.cpp

HEADERS += \
    mainwindow.h \
    codeeditor.h \
    chatwidget.h \
    terminalwidget.h \
    aiservice.h \
    syntaxhighlighter.h
```

---

## 2）`main.cpp`

```cpp
#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.showMaximized();
    return a.exec();
}
```

---

## 3）`mainwindow.h`

```cpp
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeView>
#include <QFileSystemModel>
#include <QTabWidget>
#include <QSplitter>
#include <QDockWidget>
#include <QMap>

#include "codeeditor.h"
#include "chatwidget.h"
#include "terminalwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void openFolder();
    void openFile(const QModelIndex &index);
    void saveCurrentFile();
    void runCommand();
    void onAskAiForCurrentFile();

private:
    void setupUi();
    void setupMenu();
    CodeEditor* currentEditor();
    QString currentFilePath();

private:
    QFileSystemModel *fileModel;
    QTreeView *projectTree;
    QTabWidget *editorTabs;
    ChatWidget *chatWidget;
    TerminalWidget *terminalWidget;

    QMap<QWidget*, QString> editorPathMap;
};

#endif // MAINWINDOW_H
```

---

## 4）`mainwindow.cpp`

```cpp
#include "mainwindow.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QInputDialog>
#include <QAction>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUi();
    setupMenu();
    setWindowTitle("Claude-like IDE (Qt5)");
    resize(1400, 900);
}

void MainWindow::setupUi()
{
    fileModel = new QFileSystemModel(this);
    fileModel->setRootPath(QDir::currentPath());

    projectTree = new QTreeView(this);
    projectTree->setModel(fileModel);
    projectTree->setRootIndex(fileModel->index(QDir::currentPath()));
    connect(projectTree, &QTreeView::doubleClicked, this, &MainWindow::openFile);

    editorTabs = new QTabWidget(this);
    editorTabs->setTabsClosable(true);
    connect(editorTabs, &QTabWidget::tabCloseRequested, [=](int index){
        QWidget *w = editorTabs->widget(index);
        editorPathMap.remove(w);
        editorTabs->removeTab(index);
        delete w;
    });

    chatWidget = new ChatWidget(this);
    connect(chatWidget, &ChatWidget::askAiForCurrentFile, this, &MainWindow::onAskAiForCurrentFile);

    terminalWidget = new TerminalWidget(this);

    QSplitter *mainSplitter = new QSplitter(this);
    QSplitter *centerSplitter = new QSplitter(Qt::Horizontal, this);

    centerSplitter->addWidget(editorTabs);
    centerSplitter->addWidget(chatWidget);
    centerSplitter->setStretchFactor(0, 3);
    centerSplitter->setStretchFactor(1, 1);

    mainSplitter->addWidget(projectTree);
    mainSplitter->addWidget(centerSplitter);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 4);

    setCentralWidget(mainSplitter);

    QDockWidget *terminalDock = new QDockWidget("终端 / 输出", this);
    terminalDock->setWidget(terminalWidget);
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
}

void MainWindow::setupMenu()
{
    QMenu *fileMenu = menuBar()->addMenu("文件");
    QAction *openFolderAction = fileMenu->addAction("打开文件夹");
    QAction *saveAction = fileMenu->addAction("保存当前文件");

    connect(openFolderAction, &QAction::triggered, this, &MainWindow::openFolder);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentFile);

    QMenu *runMenu = menuBar()->addMenu("运行");
    QAction *runCmdAction = runMenu->addAction("执行命令");
    connect(runCmdAction, &QAction::triggered, this, &MainWindow::runCommand);
}

void MainWindow::openFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择项目目录");
    if (dir.isEmpty()) return;

    projectTree->setRootIndex(fileModel->setRootPath(dir));
}

void MainWindow::openFile(const QModelIndex &index)
{
    QString path = fileModel->filePath(index);
    QFileInfo info(path);

    if (info.isDir()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开文件");
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    CodeEditor *editor = new CodeEditor(this);
    editor->setPlainText(content);

    int tabIndex = editorTabs->addTab(editor, info.fileName());
    editorTabs->setCurrentIndex(tabIndex);
    editorPathMap[editor] = path;
}

void MainWindow::saveCurrentFile()
{
    CodeEditor *editor = currentEditor();
    if (!editor) return;

    QString path = currentFilePath();
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法保存文件");
        return;
    }

    QTextStream out(&file);
    out << editor->toPlainText();
    file.close();

    statusBar()->showMessage("文件已保存: " + path, 3000);
}

CodeEditor *MainWindow::currentEditor()
{
    return qobject_cast<CodeEditor*>(editorTabs->currentWidget());
}

QString MainWindow::currentFilePath()
{
    QWidget *w = editorTabs->currentWidget();
    return editorPathMap.value(w);
}

void MainWindow::runCommand()
{
    QString cmd = QInputDialog::getText(this, "执行命令", "请输入命令:");
    if (cmd.isEmpty()) return;
    terminalWidget->runCommand(cmd);
}

void MainWindow::onAskAiForCurrentFile()
{
    CodeEditor *editor = currentEditor();
    if (!editor) {
        chatWidget->appendMessage("System", "当前没有打开文件。");
        return;
    }

    QString filePath = currentFilePath();
    QString content = editor->toPlainText();

    QString prompt = QString("请分析下面这个文件，并给出改进建议。\n文件路径: %1\n内容:\n%2")
            .arg(filePath, content);

    chatWidget->sendPrompt(prompt);
}
```

---

## 5）`codeeditor.h`

```cpp
#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);
};

#endif // CODEEDITOR_H
```

---

## 6）`codeeditor.cpp`

```cpp
#include "codeeditor.h"
#include <QFont>

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(11);
    setFont(font);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));
}
```

---

## 7）`terminalwidget.h`

```cpp
#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QProcess>

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    void runCommand(const QString &cmd);

private:
    QTextEdit *output;
    QLineEdit *input;
    QProcess *process;
};

#endif // TERMINALWIDGET_H
```

---

## 8）`terminalwidget.cpp`

```cpp
#include "terminalwidget.h"
#include <QVBoxLayout>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent)
{
    output = new QTextEdit(this);
    output->setReadOnly(true);

    input = new QLineEdit(this);
    input->setPlaceholderText("输入 shell 命令并回车...");

    process = new QProcess(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(output);
    layout->addWidget(input);
    setLayout(layout);

    connect(input, &QLineEdit::returnPressed, [=]() {
        QString cmd = input->text().trimmed();
        if (!cmd.isEmpty()) {
            runCommand(cmd);
            input->clear();
        }
    });

    connect(process, &QProcess::readyReadStandardOutput, [=]() {
        output->append(process->readAllStandardOutput());
    });

    connect(process, &QProcess::readyReadStandardError, [=]() {
        output->append(process->readAllStandardError());
    });
}

void TerminalWidget::runCommand(const QString &cmd)
{
    output->append(QString(">>> %1").arg(cmd));

#ifdef Q_OS_WIN
    process->start("cmd", QStringList() << "/c" << cmd);
#else
    process->start("bash", QStringList() << "-c" << cmd);
#endif
}
```

---

## 9）`chatwidget.h`

```cpp
#ifndef CHATWIDGET_H
#define CHATWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>

#include "aiservice.h"

class ChatWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ChatWidget(QWidget *parent = nullptr);
    void appendMessage(const QString &role, const QString &text);
    void sendPrompt(const QString &prompt);

signals:
    void askAiForCurrentFile();

private:
    QTextEdit *chatView;
    QTextEdit *inputEdit;
    QPushButton *sendBtn;
    QPushButton *analyzeCurrentBtn;

    AiService *aiService;
};

#endif // CHATWIDGET_H
```

---

## 10）`chatwidget.cpp`

```cpp
#include "chatwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
{
    chatView = new QTextEdit(this);
    chatView->setReadOnly(true);

    inputEdit = new QTextEdit(this);
    inputEdit->setPlaceholderText("输入你的问题...");

    sendBtn = new QPushButton("发送", this);
    analyzeCurrentBtn = new QPushButton("分析当前文件", this);

    aiService = new AiService(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(chatView);
    layout->addWidget(inputEdit);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(analyzeCurrentBtn);
    btnLayout->addWidget(sendBtn);
    layout->addLayout(btnLayout);

    connect(sendBtn, &QPushButton::clicked, [=]() {
        QString prompt = inputEdit->toPlainText().trimmed();
        if (prompt.isEmpty()) return;

        appendMessage("User", prompt);
        inputEdit->clear();
        sendPrompt(prompt);
    });

    connect(analyzeCurrentBtn, &QPushButton::clicked, this, &ChatWidget::askAiForCurrentFile);

    connect(aiService, &AiService::responseReady, [=](const QString &resp) {
        appendMessage("AI", resp);
    });

    connect(aiService, &AiService::errorOccurred, [=](const QString &err) {
        appendMessage("Error", err);
    });
}

void ChatWidget::appendMessage(const QString &role, const QString &text)
{
    chatView->append(QString("<b>%1:</b><br/>%2<br/>").arg(role, text.toHtmlEscaped().replace("\n", "<br/>")));
}

void ChatWidget::sendPrompt(const QString &prompt)
{
    aiService->sendMessage(prompt);
}
```

---

## 11）`aiservice.h`

```cpp
#ifndef AISERVICE_H
#define AISERVICE_H

#include <QObject>
#include <QNetworkAccessManager>

class AiService : public QObject
{
    Q_OBJECT
public:
    explicit AiService(QObject *parent = nullptr);
    void sendMessage(const QString &message);

signals:
    void responseReady(const QString &response);
    void errorOccurred(const QString &error);

private:
    QNetworkAccessManager *manager;
};

#endif // AISERVICE_H
```

---

## 12）`aiservice.cpp`

这里先写一个**模拟实现**，保证项目能跑起来。  
后面可以替换成真实 API 调用。

```cpp
#include "aiservice.h"
#include <QTimer>

AiService::AiService(QObject *parent)
    : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
}

void AiService::sendMessage(const QString &message)
{
    Q_UNUSED(message);

    // 这里先做一个模拟返回，后续可替换成真实 API 请求
    QTimer::singleShot(1000, [=]() {
        emit responseReady("这是一个模拟 AI 回复。你可以把 AiService 替换为真实的大模型 API 调用。");
    });
}
```

---

# 六、接入真实 AI API

如果你想接入真正的 Claude/OpenAI/本地模型，可以在 `AiService::sendMessage()` 里用 `QNetworkAccessManager` 发 HTTP 请求。

下面给你一个通用 JSON POST 示例：

---

## 替换 `aiservice.cpp`

```cpp
#include "aiservice.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>

AiService::AiService(QObject *parent)
    : QObject(parent)
{
    manager = new QNetworkAccessManager(this);
}

void AiService::sendMessage(const QString &message)
{
    QUrl url("https://api.openai.com/v1/chat/completions"); 
    QNetworkRequest request(url);

    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer YOUR_API_KEY");

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", message}
    });

    QJsonObject body;
    body["model"] = "gpt-4o-mini";
    body["messages"] = messages;

    QNetworkReply *reply = manager->post(request, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(reply->errorString());
            reply->deleteLater();
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();

        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QString content = choices[0].toObject()["message"].toObject()["content"].toString();
            emit responseReady(content);
        } else {
            emit errorOccurred("未获取到有效回复");
        }

        reply->deleteLater();
    });
}
```

---

# 七、增加代码高亮

可以先给 C++ 做个简单语法高亮。

## `syntaxhighlighter.h`

```cpp
#ifndef SYNTAXHIGHLIGHTER_H
#define SYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class SyntaxHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> rules;
};

#endif // SYNTAXHIGHLIGHTER_H
```

---

## `syntaxhighlighter.cpp`

```cpp
#include "syntaxhighlighter.h"

SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    QTextCharFormat keywordFormat;
    keywordFormat.setForeground(Qt::blue);
    keywordFormat.setFontWeight(QFont::Bold);

    QStringList keywords = {
        "\\bclass\\b", "\\bint\\b", "\\bfloat\\b", "\\bdouble\\b",
        "\\breturn\\b", "\\bif\\b", "\\belse\\b", "\\bfor\\b",
        "\\bwhile\\b", "\\bpublic\\b", "\\bprivate\\b", "\\bprotected\\b",
        "\\bvoid\\b", "\\bconst\\b", "\\binclude\\b"
    };

    for (const QString &pattern : keywords) {
        rules.append({QRegularExpression(pattern), keywordFormat});
    }

    QTextCharFormat stringFormat;
    stringFormat.setForeground(Qt::darkGreen);
    rules.append({QRegularExpression("\".*\""), stringFormat});

    QTextCharFormat commentFormat;
    commentFormat.setForeground(Qt::gray);
    rules.append({QRegularExpression("//[^\n]*"), commentFormat});
}

void SyntaxHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightRule &rule : rules) {
        QRegularExpressionMatchIterator i = rule.pattern.globalMatch(text);
        while (i.hasNext()) {
            auto match = i.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
```

---

## 在 `CodeEditor` 中启用高亮

修改 `codeeditor.cpp`：

```cpp
#include "codeeditor.h"
#include "syntaxhighlighter.h"
#include <QFont>

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    font.setPointSize(11);
    setFont(font);
    setTabStopDistance(4 * fontMetrics().horizontalAdvance(' '));

    new SyntaxHighlighter(document());
}
```

---

# 八、如何做得更像 Claude Code / VS Code

如果你希望它“更像专业 IDE”，下一步建议这样扩展：

---

## 1. 行号功能
给 `CodeEditor` 增加左边行号区域：

- 自定义 `LineNumberArea`
- 重写 `resizeEvent()`
- 监听 `blockCountChanged`
- 在 `paintEvent()` 中绘制行号

这是 Qt 做编辑器的经典方式。

---

## 2. 自动补全
可以使用：
- `QCompleter`
- 或接入 `clangd` / `pylsp` / `typescript-language-server`

推荐：
- C/C++：`clangd`
- Python：`pylsp`
- JS/TS：`typescript-language-server`

---

## 3. 代码诊断
通过 LSP 返回：
- 语法错误
- 警告
- 跳转定义
- 悬浮提示
- 重命名符号

---

## 4. 真正的终端
当前 `QProcess` 只是命令执行器，不是真正交互式终端。  
要做更完整终端，可以考虑：

- Linux/macOS：pty
- Windows：ConPTY
- 或嵌入第三方终端控件

---

## 5. AI 深度集成
可以让 AI 支持：

- 分析当前文件
- 分析选中代码
- 解释报错
- 自动修复代码
- 生成函数
- 重构建议
- 基于整个项目上下文回答问题

### 示例交互
- “解释当前报错”
- “为这个类生成单元测试”
- “重构这个函数”
- “找出内存泄漏风险”
- “为当前项目生成 README”

---

## 6. 项目上下文索引
如果做成真正“类 Claude Code”的体验，建议给工程建立索引：

- 扫描项目文件
- 忽略 `build/`、`.git/`
- 维护文件缓存
- 将当前打开文件 + 邻近文件一起发给模型
- 大项目可以用向量检索

---

# 九、建议的进阶类设计

如果你想长期维护，建议这么拆：

```text
MainWindow
ProjectExplorerWidget
EditorManager
CodeEditor
LineNumberArea
TerminalWidget
ChatWidget
AiService
ProjectIndexer
SettingsManager
ThemeManager
LspClient
```

---

# 十、界面布局建议

推荐布局：

```text
+---------------------------------------------------------------+
| 菜单栏 / 工具栏                                                |
+---------------------------------------------------------------+
| 文件树        | 编辑器标签区                   | AI 助手       |
|               |                               |               |
|               |                               |               |
|               |                               |               |
+---------------------------------------------------------------+
| 终端 / 编译输出 / 调试信息                                     |
+---------------------------------------------------------------+
```

---

# 十一、实际开发顺序

建议你按这个顺序做：

## 第一步：基础 IDE
- 文件树
- 多标签编辑器
- 保存文件
- 终端

## 第二步：AI 面板
- 聊天输入
- 调用 API
- 分析当前文件

## 第三步：编辑增强
- 语法高亮
- 行号
- 查找替换

## 第四步：智能 IDE
- 自动补全
- LSP
- 错误提示
- 跳转定义

## 第五步：AI 工程化
- 项目索引
- 上下文理解
- 代码修改建议
- 一键应用补丁

---

# 十二、你现在就可以做的最小版本

如果你的目标是**快速做一个“像 Claude Code 的工具”原型**，最低版本只要：

- `QMainWindow`
- `QTreeView`
- `QTabWidget`
- `QPlainTextEdit`
- `QTextEdit`
- `QProcess`
- `QNetworkAccessManager`

就能跑起来。

---

# 十三、我建议你直接做成这个版本

我可以继续帮你生成以下任意内容：

### 方案 A：完整 Qt5 可编译工程
我直接把上面代码整理成一套完整工程文件给你。

### 方案 B：增强版 IDE
加入：
- 行号
- 深色主题
- 查找替换
- 多标签管理
- 文件修改状态提示

### 方案 C：接入真实 AI
支持：
- OpenAI
- Claude API
- 本地 Ollama

### 方案 D：做成“类 VS Code”
加入：
- 左侧 activity bar
- 底部 panel
- 自定义主题
- 图标资源
- 命令面板

---

如果你愿意，我下一条可以**直接给你输出一份“完整可编译的 Qt5 IDE 工程源码”**，包括：

1. 主窗口  
2. 文件树  
3. 多标签编辑器  
4. 行号  
5. 语法高亮  
6. 终端  
7. AI 对话面板  
8. 调用 API 示例  

如果你要，我就直接继续给你完整代码。