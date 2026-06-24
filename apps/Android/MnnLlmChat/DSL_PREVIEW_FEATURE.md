# DSL 预览功能 - 实现说明

## 功能概述

在 MNN Android 聊天 App 中，当用户使用"UI 生成模型"对话时，模型输出完成后在 assistant 消息下方显示"预览 UI"按钮，点击后跳转到新页面用 WebView 全屏渲染由 DSL 解析出的 HTML。

## 已完成的修改

### 修改的文件

| 文件 | 修改内容 |
|------|----------|
| `app/.../model/ModelTypeUtils.kt` | 添加 `isUiGeneratorModel()` 方法，匹配名称含 `ui-gen` 或 `uigen` 的模型 |
| `app/.../chat/model/ChatDataItem.kt` | 添加 `isDslOutput: Boolean` 字段，标记该消息为 DSL 输出 |
| `app/.../chat/ChatActivity.kt` | 在 `onGenerateFinished()` 中，当模型是 UI 生成模型时设置 `isDslOutput = true` |
| `app/.../chat/chatlist/ChatViewHolders.kt` | `AssistantViewHolder` 中绑定预览按钮的显隐和点击跳转逻辑 |
| `app/.../res/layout/item_holder_assistant.xml` | 在 action buttons 上方添加 `btn_preview_ui` MaterialButton |
| `app/.../AndroidManifest.xml` | 注册 `DslPreviewActivity` |
| `app/.../res/values/strings.xml` | 添加 `preview_ui` 字符串资源 |

### 新建的文件

| 文件 | 说明 |
|------|------|
| `app/.../dslpreview/DslPreviewActivity.kt` | 全屏 WebView 预览页面，接收 DSL 文本并渲染 |
| `app/.../dslpreview/DslParser.kt` | DSL → HTML 解析器（当前为占位实现） |
| `app/.../res/layout/activity_dsl_preview.xml` | 预览页布局（Toolbar + WebView） |

### 数据流

```
用户发送消息
  → LLM 推理（流式输出 DSL 文本，显示在聊天气泡中）
  → onGenerateFinished()
      → 检测 isUiGeneratorModel → 设置 recentItem.isDslOutput = true
      → AssistantViewHolder.bind() 检测 isDslOutput
      → 显示"预览 UI"按钮
          → 用户点击
              → 启动 DslPreviewActivity，传入 item.text
                  → DslParser.parse(dsl) → HTML
                  → WebView 渲染
```

## 后续操作

### 1. 修改模型名称匹配规则

当前 `isUiGeneratorModel()` 匹配包含 `ui-gen` 或 `uigen` 的模型名/ID。需要根据实际模型名称调整。

**文件**: `app/src/main/java/com/alibaba/mnnllm/android/model/ModelTypeUtils.kt`

```kotlin
fun isUiGeneratorModel(modelNameOrId: String): Boolean {
    val lower = modelNameOrId.lowercase(Locale.getDefault())
    return lower.contains("ui-gen") || lower.contains("uigen")
    // TODO: 根据实际模型名称修改匹配规则
}
```

### 2. 实现 DslParser.parse()

当前为占位实现（直接展示原始 DSL 文本）。需要根据 DSL JSON 格式实现实际的 HTML 转换逻辑。

**文件**: `app/src/main/java/com/alibaba/mnnllm/android/dslpreview/DslParser.kt`

```kotlin
object DslParser {
    fun parse(dslJson: String): String {
        // TODO: 实现 DSL JSON → HTML 的转换
        // 输入: 模型输出的 DSL JSON 字符串
        // 输出: 可在 WebView 中渲染的完整 HTML 字符串
    }
}
```

需要提供：
- DSL JSON 的具体格式定义（字段、嵌套结构）
- 支持的 UI 组件类型（按钮、列表、输入框、卡片等）
- 样式规范（颜色、字体、间距等）

### 3. 可选增强

- **HTML 中引用远程资源**：如果 HTML 需要加载远程 CSS/JS/图片，已有 `INTERNET` 权限，无需额外处理
- **数据库持久化**：当前 `isDslOutput` 字段未持久化到数据库，重新打开历史会话时按钮不会显示。如需持久化，需修改 `ChatDataManager` 的存取逻辑
- **按钮图标**：当前使用文字按钮，可替换为带图标的按钮以与现有 action buttons 风格一致
