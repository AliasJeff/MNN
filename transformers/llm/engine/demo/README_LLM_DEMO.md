# MNN LLM Batching & JSON Benchmark

本工具是对 MNN `llm_demo` 的增强，增加了**Batching 并行推理**支持，并能够自动导出**JSON 格式的性能报告**。

## 功能

1.  **JSON 报告**：运行结束后，会在输入文件的**同级目录**生成同名 `.json` 文件（例如 `prompts.txt` -> `prompts.json`）。报告包含：
    - **Performance**: Prefill/Decode 耗时、吞吐量 (Tokens/s)。
    - **Results**: 完整的 Prompt 和 Response（Batch 模式下仅保存 Prompt）。
2.  **Batching**: 支持通过命令行设置 `batch_size`，显著提升吞吐量。

## 编译方法

略

## 使用命令

```Bash
./llm_demo <config.json> <prompt.txt> [max_tokens] [batch_size]
```

- config.json: 模型配置。
- prompt.txt: 提示词文件。
- max_tokens: 最大生成长度（默认 -1）。
- batch_size: 批大小（默认 1）。

## 示例

1. 串行测试 (Batch=1) 屏幕会有打字机效果，生成的 JSON 包含完整对话。

```Bash
./llm_demo qwen-config.json prompts.txt 200 1
```

2. 并行测试 (Batch=8) 屏幕只打印进度，生成的 JSON 包含高吞吐量数据。

```Bash
./llm_demo qwen-config.json prompts.txt 200 8
```

生成的 JSON 示例:

```JSON
{
    "performance": {
        "batch_size": 1,
        "decode_speed_tok_s": 45.2
    },
    "results": [
        {
            "id": 0,
            "prompt": "Hello",
            "response": "Hi there!"
        }
    ]
}
```
