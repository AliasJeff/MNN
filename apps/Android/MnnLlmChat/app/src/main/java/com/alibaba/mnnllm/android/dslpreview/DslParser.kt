package com.alibaba.mnnllm.android.dslpreview

import org.json.JSONArray
import org.json.JSONObject

/**
 * Parses DSL JSON output from the UI generator model into HTML.
 * Conversion logic mirrors [app_ratio.py]: beautify rules, ratio tags, and Tailwind rendering.
 */
object DslParser {

    private const val DEFAULT_WIDTH = 375
    private const val DEFAULT_HEIGHT = 812

    fun parse(
        dslText: String,
        width: Int = DEFAULT_WIDTH,
        height: Int = DEFAULT_HEIGHT,
        applyBeautify: Boolean = true,
        showFrame: Boolean = false
    ): String {
        return try {
            val jsonText = extractJson(dslText)
            val node = JSONObject(jsonText)
            val ratioTag = getRatioTag(width, height)
            val adapted = transformNode(node, ratioTag, applyBeautify)
            val bodyInner = jsonToHtml(adapted)
            wrapInTemplate(bodyInner, width, height, showFrame)
        } catch (e: Exception) {
            buildErrorHtml(dslText, e.message ?: "Unknown error")
        }
    }

    private fun extractJson(text: String): String {
        var trimmed = text.trim()
        val fencePattern = Regex("^```(?:json)?\\s*([\\s\\S]*?)\\s*```$", RegexOption.IGNORE_CASE)
        val fenceMatch = fencePattern.find(trimmed)
        if (fenceMatch != null) {
            trimmed = fenceMatch.groupValues[1].trim()
        }
        val start = trimmed.indexOf('{')
        val end = trimmed.lastIndexOf('}')
        if (start >= 0 && end > start) {
            return trimmed.substring(start, end + 1)
        }
        return trimmed
    }

    private fun mergeTokens(original: String, additions: List<String>): String {
        val tokens = original.split(Regex("\\s+")).filter { it.isNotEmpty() }
        val seen = mutableSetOf<String>()
        val result = mutableListOf<String>()
        for (token in tokens) {
            if (!seen.contains(token)) {
                result.add(token)
                seen.add(token)
            }
        }
        for (addition in additions) {
            if (!seen.contains(addition)) {
                result.add(addition)
                seen.add(addition)
            }
        }
        return result.joinToString(" ")
    }

    private fun computeBeautifyAdditions(tokensSet: Set<String>): List<String> {
        val add = mutableListOf<String>()
        if ("min-h-screen" in tokensSet && "flex" in tokensSet) {
            add += listOf("md:mx-auto", "md:max-w-4xl")
        }
        if ("grid-cols-2" in tokensSet) {
            add += listOf(
                "md:grid-cols-3",
                "lg:grid-cols-[repeat(auto-fit,minmax(220px,1fr))]"
            )
        }
        if ("rounded-lg" in tokensSet && "p-4" in tokensSet) {
            add += listOf("max-w-sm", "w-full", "mx-auto")
        }
        if (tokensSet.any { it.startsWith("text-") }) {
            add += listOf("leading-relaxed")
        }
        return add.distinct()
    }

    private fun getRatioTag(width: Int, height: Int): String {
        val ratio = width.toDouble() / height.toDouble()
        return when {
            ratio in 0.9..1.4 -> "ratio-4-3"
            ratio > 1.4 -> "ratio-16-9"
            else -> "ratio-mobile"
        }
    }

    private fun transformNode(
        node: JSONObject,
        ratioTag: String?,
        applyBeautify: Boolean
    ): JSONObject {
        val result = JSONObject(node.toString())

        if (result.has("className") && result.get("className") is String) {
            val orig = result.getString("className")
            val tokensSet = orig.split(Regex("\\s+")).filter { it.isNotEmpty() }.toSet()
            var finalClasses = orig
            if (applyBeautify) {
                finalClasses = mergeTokens(finalClasses, computeBeautifyAdditions(tokensSet))
            }
            if (ratioTag != null) {
                finalClasses = mergeTokens(finalClasses, listOf(ratioTag))
            }
            result.put("className", finalClasses)
        }

        if (result.has("children") && result.get("children") is JSONArray) {
            val children = result.getJSONArray("children")
            val newChildren = JSONArray()
            for (i in 0 until children.length()) {
                val child = children.optJSONObject(i)
                if (child != null) {
                    newChildren.put(transformNode(child, ratioTag, applyBeautify))
                }
            }
            result.put("children", newChildren)
        }
        return result
    }

    private fun jsonToHtml(node: JSONObject): String {
        val tag = node.optString("name", "div")
        val className = node.optString("className", "")
        val params = node.optJSONObject("params") ?: JSONObject()
        val children = node.optJSONArray("children") ?: JSONArray()

        val attrParts = mutableListOf<String>()
        if (className.isNotEmpty()) {
            attrParts.add("class=\"${escapeHtmlAttr(className)}\"")
        }
        var innerText = ""
        for (key in params.keys()) {
            val value = params.opt(key)
            if (key == "textContent") {
                innerText = value?.toString() ?: ""
            } else {
                attrParts.add("$key=\"${escapeHtmlAttr(value?.toString() ?: "")}\"")
            }
        }

        val attrsStr = attrParts.joinToString(" ")
        val childContent = buildString {
            for (i in 0 until children.length()) {
                val child = children.optJSONObject(i)
                if (child != null) {
                    append(jsonToHtml(child))
                }
            }
        }
        return "<$tag $attrsStr>$innerText$childContent</$tag>"
    }

    private fun wrapInTemplate(
        bodyContent: String,
        width: Int,
        height: Int,
        showFrame: Boolean
    ): String {
        val frameStyle = if (showFrame) {
            "box-shadow: 0 25px 50px -12px rgba(0, 0, 0, 0.25); border-radius: 20px;"
        } else {
            ""
        }
        val canvasStyle = """
            body { background-color: #f3f4f6; display: flex; justify-content: center; padding: 20px 0; margin: 0; }
            .phone-canvas {
                width: ${width}px; height: ${height}px;
                background-color: white; overflow-y: auto; position: relative;
                $frameStyle
            }
        """.trimIndent()
        return """
            <!DOCTYPE html>
            <html lang="zh-CN">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <script src="https://cdn.tailwindcss.com"></script>
                <style>$canvasStyle</style>
            </head>
            <body>
                <div class="phone-canvas">$bodyContent</div>
            </body>
            </html>
        """.trimIndent()
    }

    private fun buildErrorHtml(dslText: String, errorMessage: String): String {
        val escaped = escapeHtmlText(dslText)
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
                    h3 { color: #c62828; margin-top: 0; }
                    .error { color: #c62828; margin-bottom: 12px; }
                </style>
            </head>
            <body>
                <div class="dsl-container">
                    <h3>UI Preview Error</h3>
                    <p class="error">${escapeHtmlText(errorMessage)}</p>
                    <pre>$escaped</pre>
                </div>
            </body>
            </html>
        """.trimIndent()
    }

    private fun escapeHtmlAttr(text: String): String {
        return text
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace("\"", "&quot;")
    }

    private fun escapeHtmlText(text: String): String {
        return text
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
    }
}
