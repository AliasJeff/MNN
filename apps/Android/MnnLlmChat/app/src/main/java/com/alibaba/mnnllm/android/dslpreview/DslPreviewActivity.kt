package com.alibaba.mnnllm.android.dslpreview

import android.os.Bundle
import android.view.MenuItem
import android.webkit.WebView
import android.webkit.WebViewClient
import androidx.appcompat.app.AppCompatActivity
import com.alibaba.mnnllm.android.R

class DslPreviewActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_dsl_preview)

        val toolbar = findViewById<com.google.android.material.appbar.MaterialToolbar>(R.id.toolbar_preview)
        setSupportActionBar(toolbar)
        supportActionBar?.setDisplayHomeAsUpEnabled(true)
        supportActionBar?.title = getString(R.string.preview_ui)

        val dslText = intent.getStringExtra(EXTRA_DSL_CONTENT) ?: ""

        val webView = findViewById<WebView>(R.id.webview_preview)
        webView.settings.javaScriptEnabled = true
        webView.settings.domStorageEnabled = true
        webView.webViewClient = WebViewClient()

        // Use WebView measured size for ratio tag; base URL required for external Tailwind CDN
        webView.post {
            val width = webView.width.takeIf { it > 0 } ?: resources.displayMetrics.widthPixels
            val height = webView.height.takeIf { it > 0 } ?: resources.displayMetrics.heightPixels
            val html = DslParser.parse(
                dslText = dslText,
                width = width,
                height = height,
                applyBeautify = true,
                showFrame = false
            )
            webView.loadDataWithBaseURL("https://localhost/", html, "text/html", "UTF-8", null)
        }
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        if (item.itemId == android.R.id.home) {
            finish()
            return true
        }
        return super.onOptionsItemSelected(item)
    }

    companion object {
        const val EXTRA_DSL_CONTENT = "dsl_content"
    }
}
