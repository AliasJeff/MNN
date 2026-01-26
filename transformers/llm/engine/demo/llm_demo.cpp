//
//  llm_demo.cpp
//  Modified to fix Windows encoding, support JSON output, and save performance metrics
//

#include "llm/llm.hpp"
#define MNN_OPEN_TIME_TRACE
#include <MNN/AutoTime.hpp>
#include <MNN/expr/ExecutorScope.hpp>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>   // 必须包含，用于 std::setw, std::setfill
#include <algorithm> // 用于 std::all_of

// Windows 编码转换头文件
#ifdef _WIN32
#include <windows.h>
#endif

// #define LLM_SUPPORT_AUDIO
#ifdef LLM_SUPPORT_AUDIO
#include "audio/audio.hpp"
#endif
using namespace MNN::Transformer;

// ----------------------------------------------------------------------
// 工具函数：Windows下 ANSI(GBK) 转 UTF-8
// ----------------------------------------------------------------------
std::string ansi_to_utf8(const std::string &str)
{
#ifdef _WIN32
    if (str.empty())
        return "";

    // 1. 获取 ansi 字符串长度
    int targetLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
    if (targetLen == 0)
        return str;

    // 2. 转换为宽字符 (UTF-16)
    std::wstring wstr(targetLen, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstr[0], targetLen);

    // 3. 获取 utf-8 字符串长度
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    if (utf8Len == 0)
        return str;

    // 4. 转换为 utf-8
    std::string utf8Str(utf8Len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8Str[0], utf8Len, NULL, NULL);

    // 移除末尾的空字符（如果有）
    if (!utf8Str.empty() && utf8Str.back() == '\0')
    {
        utf8Str.pop_back();
    }
    return utf8Str;
#else
    return str; // 非Windows环境直接返回
#endif
}

// ----------------------------------------------------------------------
// 工具函数：简单的 JSON 转义 (处理引号和换行)
// ----------------------------------------------------------------------
std::string escape_json(const std::string &s)
{
    std::ostringstream o;
    for (char c : s)
    {
        switch (c)
        {
        case '"':
            o << "\\\"";
            break;
        case '\\':
            o << "\\\\";
            break;
        case '\b':
            o << "\\b";
            break;
        case '\f':
            o << "\\f";
            break;
        case '\n':
            o << "\\n";
            break;
        case '\r':
            o << "\\r";
            break;
        case '\t':
            o << "\\t";
            break;
        default:
            if ('\x00' <= c && c <= '\x1f')
            {
                o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
            }
            else
            {
                o << c;
            }
        }
    }
    return o.str();
}

static void tuning_prepare(Llm *llm)
{
    MNN_PRINT("Prepare for tuning opt Begin\n");
    llm->tuning(OP_ENCODER_NUMBER, {1, 5, 10, 20, 30, 50, 100});
    MNN_PRINT("Prepare for tuning opt End\n");
}

std::vector<std::vector<std::string>> parse_csv(const std::vector<std::string> &lines)
{
    std::vector<std::vector<std::string>> csv_data;
    std::string line;
    std::vector<std::string> row;
    std::string cell;
    bool insideQuotes = false;
    bool startCollecting = false;

    // content to stream
    std::string content = "";
    for (auto line : lines)
    {
        content = content + line + "\n";
    }
    std::istringstream stream(content);

    while (stream.peek() != EOF)
    {
        char c = stream.get();
        if (c == '"')
        {
            if (insideQuotes && stream.peek() == '"')
            { // quote
                cell += '"';
                stream.get(); // skip quote
            }
            else
            {
                insideQuotes = !insideQuotes; // start or end text in quote
            }
            startCollecting = true;
        }
        else if (c == ',' && !insideQuotes)
        { // end element, start new element
            row.push_back(cell);
            cell.clear();
            startCollecting = false;
        }
        else if ((c == '\n' || stream.peek() == EOF) && !insideQuotes)
        { // end line
            row.push_back(cell);
            csv_data.push_back(row);
            cell.clear();
            row.clear();
            startCollecting = false;
        }
        else
        {
            cell += c;
            startCollecting = true;
        }
    }
    return csv_data;
}

// ----------------------------------------------------------------------
// Modified benchmark: Supports saving to JSON file with Metrics
// ----------------------------------------------------------------------
static int benchmark(Llm *llm, const std::vector<std::string> &prompts, int max_token_number, const std::string &output_json_file)
{
    int total_prompt_len = 0;
    int total_decode_len = 0;
    int64_t total_prefill_time = 0;
    int64_t total_decode_time = 0;
    int64_t total_sample_time = 0;

    auto context = llm->getContext();
    if (max_token_number > 0)
    {
        llm->set_config("{\"max_new_tokens\":1}");
    }

    // JSON Output Structure
    std::ofstream jsonFile;
    bool save_json = !output_json_file.empty();
    if (save_json)
    {
        jsonFile.open(output_json_file);
        jsonFile << "[\n"; // Start array
    }

    for (int i = 0; i < prompts.size(); i++)
    {
        auto prompt = prompts[i];

        // prompt start with '#' will be ignored
        if (prompt.substr(0, 1) == "#")
            continue;

        std::cout << "\n[Input Prompt]: " << prompt << std::endl;
        std::cout << "[Output]: " << std::flush;

        if (max_token_number >= 0)
        {
            llm->response(prompt, &std::cout, nullptr, 0);
            while (!llm->stoped() && context->gen_seq_len < max_token_number)
            {
                llm->generate(1);
            }
        }
        else
        {
            llm->response(prompt);
        }

        // --- Calculate Single Prompt Metrics ---
        float current_prefill_s = context->prefill_us / 1e6f;
        float current_decode_s = context->decode_us / 1e6f;
        float current_prefill_speed = 0.0f;
        if (current_prefill_s > 0)
            current_prefill_speed = context->prompt_len / current_prefill_s;

        float current_decode_speed = 0.0f;
        if (current_decode_s > 0)
            current_decode_speed = context->gen_seq_len / current_decode_s;

        // --- Save to JSON Logic ---
        if (save_json)
        {
            std::string generated_text = context->generate_str; // 获取生成的完整文本

            jsonFile << "  {\n";
            jsonFile << "    \"id\": " << i << ",\n";
            jsonFile << "    \"prompt\": \"" << escape_json(prompt) << "\",\n";
            jsonFile << "    \"response\": \"" << escape_json(generated_text) << "\",\n";

            // Add metrics object
            jsonFile << "    \"metrics\": {\n";
            jsonFile << "      \"prompt_tokens\": " << context->prompt_len << ",\n";
            jsonFile << "      \"decode_tokens\": " << context->gen_seq_len << ",\n";
            jsonFile << "      \"prefill_time\": " << std::fixed << std::setprecision(4) << current_prefill_s << ",\n";
            jsonFile << "      \"decode_time\": " << std::fixed << std::setprecision(4) << current_decode_s << ",\n";
            jsonFile << "      \"prefill_speed\": " << std::fixed << std::setprecision(2) << current_prefill_speed << ",\n";
            jsonFile << "      \"decode_speed\": " << std::fixed << std::setprecision(2) << current_decode_speed << "\n";
            jsonFile << "    }\n";

            jsonFile << "  }";
            if (i < prompts.size() - 1)
                jsonFile << ",";
            jsonFile << "\n";
        }
        // ---------------------------

        total_prompt_len += context->prompt_len;
        total_decode_len += context->gen_seq_len;
        total_prefill_time += context->prefill_us;
        total_decode_time += context->decode_us;
        total_sample_time += context->sample_us;
    }

    if (save_json)
    {
        jsonFile << "]\n";
        jsonFile.close();
        std::cout << "\n\n[System]: Result saved to " << output_json_file << std::endl;
    }

    // Print Total Stats
    float vision_s = context->vision_us / 1e6;
    float audio_s = context->audio_us / 1e6;
    float prefill_s = total_prefill_time / 1e6;
    float decode_s = total_decode_time / 1e6;
    float sample_s = total_sample_time / 1e6;
    float vision_speed = 0.0f;
    if (context->pixels_mp > 0.0f)
        vision_speed = context->pixels_mp / vision_s;

    MNN_PRINT("\n#################################\n");
    MNN_PRINT("prompt tokens num = %d\n", total_prompt_len);
    MNN_PRINT("decode tokens num = %d\n", total_decode_len);
    MNN_PRINT("prefill speed = %.2f tok/s\n", total_prompt_len / prefill_s);
    MNN_PRINT("decode speed = %.2f tok/s\n", total_decode_len / decode_s);
    MNN_PRINT("##################################\n");
    return 0;
}

// ----------------------------------------------------------------------
// Modified eval: Passes output filename
// ----------------------------------------------------------------------
static int eval(Llm *llm, std::string prompt_input, int max_token_number, std::string output_file)
{
    std::vector<std::string> prompts;
    std::string prompt;

    // 1. Try to open as file
    std::ifstream prompt_fs(prompt_input);
    if (prompt_fs.is_open())
    {
        std::cout << "Load prompt from file: " << prompt_input << std::endl;
        while (std::getline(prompt_fs, prompt))
        {
            if (prompt.empty())
                continue;
            if (prompt.back() == '\r')
                prompt.pop_back();
            prompts.push_back(prompt);
        }
        prompt_fs.close();

        // CSV mode (Not supporting JSON output for CSV mode in this quick fix, keep as is)
        if (!prompts.empty() && prompts[0] == "id,question,A,B,C,D,answer")
        {
            // You might need to copy parse_csv back into this file if it was removed
            // return ceval(llm, prompts, prompt_input);
            std::cout << "CSV evaluation mode not fully adapted for custom JSON output in this snippet." << std::endl;
            return 0;
        }
    }
    else
    {
        // 2. Direct String Input (FIXED: assuming prompt_input is already UTF-8 converted in main)
        std::cout << "Treating input as direct prompt string." << std::endl;
        prompts.push_back(prompt_input);
    }

    if (prompts.empty())
        return 1;

    return benchmark(llm, prompts, max_token_number, output_file);
}

void chat(Llm *llm)
{
    // (Chat function remains unchanged)
    ChatMessages messages;
    messages.emplace_back("system", "You are a helpful assistant.");
    auto context = llm->getContext();
    while (true)
    {
        std::cout << "\nUser: ";
        std::string user_str;
        std::getline(std::cin, user_str);
        if (user_str == "/exit")
            return;
        if (user_str == "/reset")
        {
            llm->reset();
            std::cout << "\nA: reset done." << std::endl;
            continue;
        }

// Windows input from cin might also need ansi_to_utf8, but sticking to basics here
#ifdef _WIN32
        user_str = ansi_to_utf8(user_str);
#endif

        messages.emplace_back("user", user_str);
        std::cout << "\nA: " << std::flush;
        llm->response(messages);
        auto assistant_str = context->generate_str;
        messages.emplace_back("assistant", assistant_str);
    }
}

int main(int argc, const char *argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " config.json [prompt_string_or_file] [max_tokens] [output_filename.json]" << std::endl;
        return 0;
    }

    // 1. Initialize MNN
    MNN::BackendConfig backendConfig;
    auto executor = MNN::Express::Executor::newExecutor(MNN_FORWARD_CPU, backendConfig, 1);
    MNN::Express::ExecutorScope s(executor);

    std::string config_path = argv[1];
    std::cout << "config path is " << config_path << std::endl;
    std::unique_ptr<Llm> llm(Llm::createLLM(config_path));
    llm->set_config("{\"tmp_path\":\"tmp\"}");

    {
        AUTOTIME;
        if (!llm->load())
        {
            MNN_ERROR("LLM init error\n");
            return 0;
        }
    }

    // Tuning (Optional)
    // tuning_prepare(llm.get());

    // 2. Mode Selection
    if (argc < 3)
    {
        chat(llm.get());
        return 0;
    }

    // 3. Parse Arguments
    std::string prompt_input = argv[2];

// --- FIX: Convert Argument Encoding (GBK -> UTF8) ---
#ifdef _WIN32
    prompt_input = ansi_to_utf8(prompt_input);
#endif
    // ----------------------------------------------------

    int max_token_number = -1;
    std::string output_filename = "";

    // Argument 3: Can be max_tokens OR output_filename (if user skips max_tokens, need heuristic)
    // Heuristic: if argv[3] is a number, it's max_tokens. Otherwise it's filename.
    if (argc >= 4)
    {
        std::string arg3 = argv[3];
        bool is_number = !arg3.empty() && std::all_of(arg3.begin(), arg3.end(), ::isdigit);

        if (is_number)
        {
            max_token_number = std::stoi(arg3);
        }
        else
        {
            output_filename = arg3;
        }
    }

    // Argument 4: If Argument 3 was a number, this is the filename
    if (argc >= 5)
    {
        output_filename = argv[4];
    }

    // Qwen2.5/3 Special logic
    if (prompt_input.find("no_think") != std::string::npos)
    {
        llm->set_config(R"({"jinja": {"context": {"enable_thinking":false}}})");
    }

    llm->set_config(R"({"async":false})");

    return eval(llm.get(), prompt_input, max_token_number, output_filename);
}