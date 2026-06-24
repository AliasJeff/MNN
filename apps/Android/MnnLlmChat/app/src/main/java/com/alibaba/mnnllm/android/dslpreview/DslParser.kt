package com.alibaba.mnnllm.android.dslpreview

/**
 * Parses DSL JSON output from the UI generator model into HTML.
 * TODO: Implement actual DSL-to-HTML conversion logic.
 */
object DslParser {

    fun parse(dslJson: String): String {
        // Placeholder: display raw DSL text wrapped in basic HTML
        val escaped = dslJson
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
        return """
            <!DOCTYPE html>
            <html>
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>
                    body { font-family: -apple-system, sans-serif; padding: 16px; background: #f5f5f5; }
                    .dsl-container { background: #fff; border-radius: 8px; padding: 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
                    pre { white-space: pre-wrap; word-break: break-word; font-size: 14px; }
                    h3 { color: #333; margin-top: 0; }
                </style>
            </head>
            <body>
                <div class="dsl-container">
                    <h3>UI Preview (parser not implemented)</h3>
                    <pre>${escaped}</pre>
                </div>
            </body>
            </html>
        """.trimIndent()
    }
}
