// #include "llm/llm.hpp"
// #define MNN_OPEN_TIME_TRACE
// #include <MNN/AutoTime.hpp>
// #include <MNN/expr/ExecutorScope.hpp>
// #include <fstream>
// #include <sstream>
// #include <stdlib.h>
// #include <initializer_list>
// #include <algorithm> // for std::max, std::min
// #include <vector>
// #include <string>
// #include <iomanip> // for std::fixed, std::setprecision

// // #define LLM_SUPPORT_AUDIO
// #ifdef LLM_SUPPORT_AUDIO
// #include "audio/audio.hpp"
// #endif
// using namespace MNN::Transformer;

// // --- JSON Data Structures & Helpers ---

// struct InferenceItem
// {
//     int id;
//     std::string prompt;
//     std::string response;
// };

// struct BenchmarkMetrics
// {
//     int batch_size;
//     int prompt_tokens;
//     int decode_tokens;
//     float prefill_time_s;
//     float decode_time_s;
//     float total_time_s;
//     float prefill_speed;
//     float decode_speed;
// };

// // Helper: Escape string for JSON
// static std::string json_escape(const std::string &s)
// {
//     std::ostringstream o;
//     for (char c : s)
//     {
//         if (c == '"')
//             o << "\\\"";
//         else if (c == '\\')
//             o << "\\\\";
//         else if (c == '\b')
//             o << "\\b";
//         else if (c == '\f')
//             o << "\\f";
//         else if (c == '\n')
//             o << "\\n";
//         else if (c == '\r')
//             o << "\\r";
//         else if (c == '\t')
//             o << "\\t";
//         else if ((unsigned char)c <= 0x1f)
//         {
//             o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
//         }
//         else
//         {
//             o << c;
//         }
//     }
//     return o.str();
// }

// // Helper: Write results to JSON file
// static void write_json_result(const std::string &filename,
//                               const std::vector<InferenceItem> &items,
//                               const BenchmarkMetrics &metrics)
// {
//     std::ofstream fs(filename);
//     if (!fs.is_open())
//     {
//         MNN_ERROR("Failed to open %s for writing\n", filename.c_str());
//         return;
//     }

//     fs << "{\n";
//     fs << "  \"performance\": {\n";
//     fs << "    \"batch_size\": " << metrics.batch_size << ",\n";
//     fs << "    \"prompt_tokens\": " << metrics.prompt_tokens << ",\n";
//     fs << "    \"decode_tokens\": " << metrics.decode_tokens << ",\n";
//     fs << "    \"prefill_time_s\": " << std::fixed << std::setprecision(4) << metrics.prefill_time_s << ",\n";
//     fs << "    \"decode_time_s\": " << metrics.decode_time_s << ",\n";
//     fs << "    \"total_time_s\": " << metrics.total_time_s << ",\n";
//     fs << "    \"prefill_speed_tok_s\": " << std::setprecision(2) << metrics.prefill_speed << ",\n";
//     fs << "    \"decode_speed_tok_s\": " << metrics.decode_speed << "\n";
//     fs << "  },\n";
//     fs << "  \"results\": [\n";

//     for (size_t i = 0; i < items.size(); ++i)
//     {
//         fs << "    {\n";
//         fs << "      \"id\": " << items[i].id << ",\n";
//         fs << "      \"prompt\": \"" << json_escape(items[i].prompt) << "\",\n";
//         fs << "      \"response\": \"" << json_escape(items[i].response) << "\"\n";
//         fs << "    }" << (i < items.size() - 1 ? "," : "") << "\n";
//     }

//     fs << "  ]\n";
//     fs << "}\n";
//     fs.close();
//     MNN_PRINT("Benchmark results written to %s\n", filename.c_str());
// }
// // --------------------------------------

// static void tuning_prepare(Llm *llm)
// {
//     MNN_PRINT("Prepare for tuning opt Begin\n");
//     llm->tuning(OP_ENCODER_NUMBER, {1, 5, 10, 20, 30, 50, 100});
//     MNN_PRINT("Prepare for tuning opt End\n");
// }

// std::vector<std::vector<std::string>> parse_csv(const std::vector<std::string> &lines)
// {
//     std::vector<std::vector<std::string>> csv_data;
//     std::string content = "";
//     for (auto line : lines)
//         content = content + line + "\n";
//     std::istringstream stream(content);

//     std::string line;
//     std::vector<std::string> row;
//     std::string cell;
//     bool insideQuotes = false;
//     bool startCollecting = false;

//     while (stream.peek() != EOF)
//     {
//         char c = stream.get();
//         if (c == '"')
//         {
//             if (insideQuotes && stream.peek() == '"')
//             {
//                 cell += '"';
//                 stream.get();
//             }
//             else
//             {
//                 insideQuotes = !insideQuotes;
//             }
//             startCollecting = true;
//         }
//         else if (c == ',' && !insideQuotes)
//         {
//             row.push_back(cell);
//             cell.clear();
//             startCollecting = false;
//         }
//         else if ((c == '\n' || stream.peek() == EOF) && !insideQuotes)
//         {
//             row.push_back(cell);
//             csv_data.push_back(row);
//             cell.clear();
//             row.clear();
//             startCollecting = false;
//         }
//         else
//         {
//             cell += c;
//             startCollecting = true;
//         }
//     }
//     return csv_data;
// }

// // Updated benchmark signature to collect data
// static int benchmark(Llm *llm, const std::vector<std::string> &prompts, int max_token_number, int batch_size,
//                      std::vector<InferenceItem> &out_items, BenchmarkMetrics &out_metrics)
// {
//     int prompt_len = 0;
//     int decode_len = 0;
//     int64_t prefill_time = 0;
//     int64_t decode_time = 0;
//     int64_t sample_time = 0;

//     auto context = llm->getContext();
//     if (max_token_number > 0)
//     {
//         llm->set_config("{\"max_new_tokens\":1}");
//     }

//     if (batch_size > 1)
//     {
//         llm->set_config("{\"batch_size\":" + std::to_string(batch_size) + "}");
//         MNN_PRINT("Batching enabled: Batch Size = %d\n", batch_size);
//     }

// #ifdef LLM_SUPPORT_AUDIO
//     std::vector<float> waveform;
//     llm->setWavformCallback([&](const float *ptr, size_t size, bool last_chunk)
//                             {
//         waveform.reserve(waveform.size() + size);
//         waveform.insert(waveform.end(), ptr, ptr + size);
//         if (last_chunk) {
//             auto waveform_var = MNN::Express::_Const(waveform.data(), {(int)waveform.size()}, MNN::Express::NCHW, halide_type_of<float>());
//             MNN::AUDIO::save("output.wav", waveform_var, 24000);
//             waveform.clear();
//         }
//         return true; });
// #endif

//     std::vector<std::string> valid_prompts;
//     for (const auto &p : prompts)
//     {
//         if (p.substr(0, 1) != "#")
//             valid_prompts.push_back(p);
//     }

//     if (batch_size <= 1)
//     {
//         // --- Serial Mode ---
//         for (int i = 0; i < valid_prompts.size(); ++i)
//         {
//             std::string current_prompt = valid_prompts[i];
// #ifdef MIMO_NO_THINKING
//             llm->set_config("{\"assistant_prompt_template\":\"<|im_start|>assistant\\n<think>\\n</think>\%s<|im_end|>\\n\"}");
//             current_prompt = current_prompt + "<think>\n</think>";
// #endif
//             // Pass &std::cout to preserve streaming type-writer effect on console
//             if (max_token_number >= 0)
//             {
//                 llm->response(current_prompt, &std::cout, nullptr, 0);
//                 while (!llm->stoped() && context->gen_seq_len < max_token_number)
//                 {
//                     llm->generate(1);
//                 }
//             }
//             else
//             {
//                 llm->response(current_prompt);
//             }

//             // Capture the full generated string from context for JSON
//             out_items.push_back({i, current_prompt, context->generate_str});

//             prompt_len += context->prompt_len;
//             decode_len += context->gen_seq_len;
//             prefill_time += context->prefill_us;
//             decode_time += context->decode_us;
//             sample_time += context->sample_us;
//         }
//     }
//     else
//     {
//         // --- Batched Mode ---
//         for (int i = 0; i < valid_prompts.size(); i += batch_size)
//         {
//             int current_bs = std::min(batch_size, (int)valid_prompts.size() - i);

//             // 1. Prepare
//             std::vector<std::vector<int>> batch_tokens;
//             int max_seq_len = 0;
//             for (int b = 0; b < current_bs; b++)
//             {
//                 std::string p = valid_prompts[i + b];
// #ifdef MIMO_NO_THINKING
//                 if (b == 0)
//                     llm->set_config("{\"assistant_prompt_template\":\"<|im_start|>assistant\\n<think>\\n</think>\%s<|im_end|>\\n\"}");
//                 p = p + "<think>\n</think>";
// #endif
//                 // In batch mode, exact individual response text is hard to separate, so we skip it.
//                 out_items.push_back({i + b, p, "[Batched Mode] Text output suppressed for performance"});

//                 std::string formatted_p = llm->apply_chat_template(p);
//                 auto tokens = llm->tokenizer_encode(formatted_p);
//                 if (tokens.size() > max_seq_len)
//                     max_seq_len = tokens.size();
//                 batch_tokens.push_back(tokens);
//             }

//             // 2. Padding
//             std::vector<int> input_ids;
//             input_ids.reserve(current_bs * max_seq_len);
//             for (const auto &tokens : batch_tokens)
//             {
//                 int pad_len = max_seq_len - tokens.size();
//                 for (int k = 0; k < pad_len; k++)
//                     input_ids.push_back(0);
//                 input_ids.insert(input_ids.end(), tokens.begin(), tokens.end());
//             }

//             // 3. Inference
//             if (max_token_number >= 0)
//             {
//                 llm->response(input_ids, nullptr, nullptr, 0);
//                 while (!llm->stoped() && context->gen_seq_len < max_token_number)
//                 {
//                     llm->generate(1);
//                 }
//             }
//             else
//             {
//                 llm->response(input_ids, nullptr);
//             }

//             // 4. Metrics
//             prompt_len += context->prompt_len;
//             decode_len += context->gen_seq_len * current_bs; // Total tokens
//             prefill_time += context->prefill_us;
//             decode_time += context->decode_us;
//             sample_time += context->sample_us;

//             MNN_PRINT("Batch %d processed.\n", (i / batch_size) + 1);
//         }
//     }

//     llm->generateWavform();

//     // Stats
//     float vision_s = context->vision_us / 1e6;
//     float audio_s = context->audio_us / 1e6;
//     float prefill_s = prefill_time / 1e6;
//     float decode_s = decode_time / 1e6;
//     float sample_s = sample_time / 1e6;

//     // Fill Metrics Struct
//     out_metrics.batch_size = batch_size;
//     out_metrics.prompt_tokens = prompt_len;
//     out_metrics.decode_tokens = decode_len;
//     out_metrics.prefill_time_s = prefill_s;
//     out_metrics.decode_time_s = decode_s;
//     out_metrics.total_time_s = prefill_s + decode_s + sample_s;
//     out_metrics.prefill_speed = (prefill_s > 0) ? (prompt_len / prefill_s) : 0.0f;
//     out_metrics.decode_speed = (decode_s > 0) ? (decode_len / decode_s) : 0.0f;

//     // Console Print
//     MNN_PRINT("\n#################################\n");
//     MNN_PRINT("Batch Size        = %d\n", batch_size);
//     MNN_PRINT("prompt tokens num = %d\n", prompt_len);
//     MNN_PRINT("decode tokens num = %d\n", decode_len);
//     MNN_PRINT(" vision time = %.2f s\n", vision_s);
//     MNN_PRINT(" pixels_mp = %.2f MP\n", context->pixels_mp);
//     MNN_PRINT("  audio process time = %.2f s\n", audio_s);
//     MNN_PRINT("  audio input time = %.2f s\n", context->audio_input_s);
//     MNN_PRINT("prefill time = %.2f s\n", prefill_s);
//     MNN_PRINT(" decode time = %.2f s\n", decode_s);
//     if (prefill_s > 0)
//         MNN_PRINT("prefill speed = %.2f tok/s\n", out_metrics.prefill_speed);
//     if (decode_s > 0)
//         MNN_PRINT(" decode speed = %.2f tok/s\n", out_metrics.decode_speed);
//     MNN_PRINT(" vision speed = %.3f MP/s\n", (vision_s > 0) ? context->pixels_mp / vision_s : 0);
//     MNN_PRINT(" audio RTF = %.3f \n", (context->audio_input_s > 0) ? audio_s / context->audio_input_s : 0);
//     MNN_PRINT("##################################\n");
//     return 0;
// }

// static int ceval(Llm *llm, const std::vector<std::string> &lines, std::string filename)
// {
//     // C-Eval logic kept identical
//     auto csv_data = parse_csv(lines);
//     std::vector<std::string> answers;
//     for (int i = 1; i < csv_data.size(); i++)
//     {
//         const auto &elements = csv_data[i];
//         std::string prompt = elements[1];
//         prompt += "\n\nA. " + elements[2];
//         prompt += "\nB. " + elements[3];
//         prompt += "\nC. " + elements[4];
//         prompt += "\nD. " + elements[5];
//         prompt += "\n\n";
//         MNN_PRINT("%s", prompt.c_str());
//         MNN_PRINT("## 进度: %d / %lu\n", i, lines.size() - 1);
//         std::ostringstream lineOs;
//         llm->response(prompt.c_str(), &lineOs);
//         auto line = lineOs.str();
//         MNN_PRINT("%s", line.c_str());
//         answers.push_back(line);
//     }
//     {
//         auto position = filename.rfind("/");
//         if (position != std::string::npos)
//         {
//             filename = filename.substr(position + 1, -1);
//         }
//         position = filename.find("_val");
//         if (position != std::string::npos)
//         {
//             filename.replace(position, 4, "_res");
//         }
//         std::cout << "store to " << filename << std::endl;
//     }
//     std::ofstream ofp(filename);
//     ofp << "id,answer" << std::endl;
//     for (int i = 0; i < answers.size(); i++)
//     {
//         ofp << i << ",\"" << answers[i] << "\"" << std::endl;
//     }
//     ofp.close();
//     return 0;
// }

// static int eval(Llm *llm, std::string prompt_file, int max_token_number, int batch_size)
// {
//     std::cout << "prompt file is " << prompt_file << std::endl;
//     std::ifstream prompt_fs(prompt_file);
//     std::vector<std::string> prompts;
//     std::string prompt;

// #ifdef LLM_DEMO_ONELINE
//     std::ostringstream tempOs;
//     tempOs << prompt_fs.rdbuf();
//     prompt = tempOs.str();
//     prompts = {prompt};
// #else
//     while (std::getline(prompt_fs, prompt))
//     {
//         if (prompt.empty())
//             continue;
//         if (prompt.back() == '\r')
//             prompt.pop_back();
//         prompts.push_back(prompt);
//     }
// #endif
//     prompt_fs.close();
//     if (prompts.empty())
//         return 1;

//     // C-Eval Branch
//     if (prompts[0] == "id,question,A,B,C,D,answer")
//     {
//         return ceval(llm, prompts, prompt_file);
//     }

//     // Benchmark Branch (with JSON)
//     std::vector<InferenceItem> items;
//     BenchmarkMetrics metrics;
//     int ret = benchmark(llm, prompts, max_token_number, batch_size, items, metrics);

//     // Generate Filename: input.txt -> input.json
//     std::string json_filename = prompt_file;
//     size_t last_dot = json_filename.find_last_of(".");
//     if (last_dot != std::string::npos)
//     {
//         json_filename = json_filename.substr(0, last_dot);
//     }
//     json_filename += ".json";

//     write_json_result(json_filename, items, metrics);
//     return ret;
// }

// void chat(Llm *llm)
// {
//     ChatMessages messages;
//     messages.emplace_back("system", "You are a helpful assistant.");
//     auto context = llm->getContext();
//     while (true)
//     {
//         std::cout << "\nUser: ";
//         std::string user_str;
//         std::getline(std::cin, user_str);
//         if (user_str == "/exit")
//             return;
//         if (user_str == "/reset")
//         {
//             llm->reset();
//             std::cout << "\nA: reset done." << std::endl;
//             continue;
//         }
//         messages.emplace_back("user", user_str);
//         std::cout << "\nA: " << std::flush;
//         llm->response(messages);
//         auto assistant_str = context->generate_str;
//         messages.emplace_back("assistant", assistant_str);
//     }
// }

// int main(int argc, const char *argv[])
// {
//     if (argc < 2)
//     {
//         std::cout << "Usage: " << argv[0] << " config.json <prompt.txt> [max_tokens] [batch_size]" << std::endl;
//         return 0;
//     }
//     MNN::BackendConfig backendConfig;
//     auto executor = MNN::Express::Executor::newExecutor(MNN_FORWARD_CPU, backendConfig, 1);
//     MNN::Express::ExecutorScope s(executor);

//     std::string config_path = argv[1];
//     std::cout << "config path is " << config_path << std::endl;
//     std::unique_ptr<Llm> llm(Llm::createLLM(config_path));
//     llm->set_config("{\"tmp_path\":\"tmp\"}");
//     {
//         AUTOTIME;
//         if (!llm->load())
//         {
//             MNN_ERROR("LLM init error\n");
//             return 0;
//         }
//     }
//     if (true)
//     {
//         AUTOTIME;
//         tuning_prepare(llm.get());
//     }
//     if (argc < 3)
//     {
//         chat(llm.get());
//         return 0;
//     }
//     int max_token_number = -1;
//     if (argc >= 4)
//     {
//         std::istringstream os(argv[3]);
//         os >> max_token_number;
//     }

//     // Logic for parsing batch_size (Arg 4) while maintaining compatibility
//     int batch_size = 1;
//     bool is_old_thinking_arg = false;

//     if (argc >= 5)
//     {
//         std::string arg4 = argv[4];
//         if (std::all_of(arg4.begin(), arg4.end(), ::isdigit))
//         {
//             batch_size = std::stoi(arg4);
//         }
//         else
//         {
//             is_old_thinking_arg = true;
//         }
//     }

//     if (is_old_thinking_arg || argc >= 6)
//     {
//         MNN_PRINT("Set not thinking, only valid for Qwen3\n");
//         llm->set_config(R"({"jinja": {"context": {"enable_thinking":false}}})");
//     }

//     std::string prompt_file = argv[2];
//     llm->set_config(R"({"async":false})");
//     return eval(llm.get(), prompt_file, max_token_number, batch_size);
// }

#include "llm/llm.hpp"
#define MNN_OPEN_TIME_TRACE
#include <MNN/AutoTime.hpp>
#include <MNN/expr/ExecutorScope.hpp>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <initializer_list>
#include <algorithm> // for std::max, std::min
#include <vector>
#include <string>
#include <iomanip> // for std::fixed, std::setprecision
#include <iostream>

// #define LLM_SUPPORT_AUDIO
#ifdef LLM_SUPPORT_AUDIO
#include "audio/audio.hpp"
#endif
using namespace MNN::Transformer;

// --- JSON Data Structures & Helpers ---

struct InferenceItem
{
    int id;
    std::string prompt;
    // std::string response;
};

struct BenchmarkMetrics
{
    int batch_size;
    int prompt_tokens;
    int decode_tokens;
    float prefill_time_s;
    float decode_time_s;
    float total_time_s;
    float prefill_speed;
    float decode_speed;
};

// Helper: Escape string for JSON
static std::string json_escape(const std::string &s)
{
    std::ostringstream o;
    for (char c : s)
    {
        if (c == '"')
            o << "\\\"";
        else if (c == '\\')
            o << "\\\\";
        else if (c == '\b')
            o << "\\b";
        else if (c == '\f')
            o << "\\f";
        else if (c == '\n')
            o << "\\n";
        else if (c == '\r')
            o << "\\r";
        else if (c == '\t')
            o << "\\t";
        else if ((unsigned char)c <= 0x1f)
        {
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
        }
        else
        {
            o << c;
        }
    }
    return o.str();
}

// Helper: Write results to JSON file
static void write_json_result(const std::string &filename,
                              const std::vector<InferenceItem> &items,
                              const BenchmarkMetrics &metrics)
{
    std::ofstream fs(filename);
    if (!fs.is_open())
    {
        MNN_ERROR("Failed to open %s for writing\n", filename.c_str());
        return;
    }

    fs << "{\n";
    fs << "  \"performance\": {\n";
    fs << "    \"batch_size\": " << metrics.batch_size << ",\n";
    fs << "    \"prompt_tokens\": " << metrics.prompt_tokens << ",\n";
    fs << "    \"decode_tokens\": " << metrics.decode_tokens << ",\n";
    fs << "    \"prefill_time_s\": " << std::fixed << std::setprecision(4) << metrics.prefill_time_s << ",\n";
    fs << "    \"decode_time_s\": " << metrics.decode_time_s << ",\n";
    fs << "    \"total_time_s\": " << metrics.total_time_s << ",\n";
    fs << "    \"prefill_speed_tok_s\": " << std::setprecision(2) << metrics.prefill_speed << ",\n";
    fs << "    \"decode_speed_tok_s\": " << metrics.decode_speed << "\n";
    fs << "  },\n";
    fs << "  \"results\": [\n";

    for (size_t i = 0; i < items.size(); ++i)
    {
        fs << "    {\n";
        fs << "      \"id\": " << items[i].id << ",\n";
        // 不再写入 response
        fs << "      \"prompt\": \"" << json_escape(items[i].prompt) << "\"\n";
        fs << "    }" << (i < items.size() - 1 ? "," : "") << "\n";
    }

    fs << "  ]\n";
    fs << "}\n";
    fs.close();
    MNN_PRINT("Benchmark results written to %s\n", filename.c_str());
}
// --------------------------------------

static void tuning_prepare(Llm *llm)
{
    MNN_PRINT("Prepare for tuning opt Begin\n");
    llm->tuning(OP_ENCODER_NUMBER, {1, 5, 10, 20, 30, 50, 100});
    MNN_PRINT("Prepare for tuning opt End\n");
}

std::vector<std::vector<std::string>> parse_csv(const std::vector<std::string> &lines)
{
    std::vector<std::vector<std::string>> csv_data;
    std::string content = "";
    for (auto line : lines)
        content = content + line + "\n";
    std::istringstream stream(content);

    std::string line;
    std::vector<std::string> row;
    std::string cell;
    bool insideQuotes = false;
    bool startCollecting = false;

    while (stream.peek() != EOF)
    {
        char c = stream.get();
        if (c == '"')
        {
            if (insideQuotes && stream.peek() == '"')
            {
                cell += '"';
                stream.get();
            }
            else
            {
                insideQuotes = !insideQuotes;
            }
            startCollecting = true;
        }
        else if (c == ',' && !insideQuotes)
        {
            row.push_back(cell);
            cell.clear();
            startCollecting = false;
        }
        else if ((c == '\n' || stream.peek() == EOF) && !insideQuotes)
        {
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

// Updated benchmark signature to collect data
static int benchmark(Llm *llm, const std::vector<std::string> &prompts, int max_token_number, int batch_size,
                     std::vector<InferenceItem> &out_items, BenchmarkMetrics &out_metrics)
{
    int prompt_len = 0;
    int decode_len = 0;
    int64_t prefill_time = 0;
    int64_t decode_time = 0;
    int64_t sample_time = 0;

    auto context = llm->getContext();
    if (max_token_number > 0)
    {
        llm->set_config("{\"max_new_tokens\":1}");
    }

    if (batch_size > 1)
    {
        llm->set_config("{\"batch_size\":" + std::to_string(batch_size) + "}");
        MNN_PRINT("Batching enabled: Batch Size = %d\n", batch_size);
    }

#ifdef LLM_SUPPORT_AUDIO
    std::vector<float> waveform;
    llm->setWavformCallback([&](const float *ptr, size_t size, bool last_chunk)
                            {
        waveform.reserve(waveform.size() + size);
        waveform.insert(waveform.end(), ptr, ptr + size);
        if (last_chunk) {
            auto waveform_var = MNN::Express::_Const(waveform.data(), {(int)waveform.size()}, MNN::Express::NCHW, halide_type_of<float>());
            MNN::AUDIO::save("output.wav", waveform_var, 24000);
            waveform.clear();
        }
        return true; });
#endif

    std::vector<std::string> valid_prompts;
    for (const auto &p : prompts)
    {
        if (p.substr(0, 1) != "#")
            valid_prompts.push_back(p);
    }

    if (batch_size <= 1)
    {
        // --- Serial Mode ---
        for (int i = 0; i < valid_prompts.size(); ++i)
        {
            std::string current_prompt = valid_prompts[i];
#ifdef MIMO_NO_THINKING
            llm->set_config("{\"assistant_prompt_template\":\"<|im_start|>assistant\\n<think>\\n</think>\%s<|im_end|>\\n\"}");
            current_prompt = current_prompt + "<think>\n</think>";
#endif
            // 显式打印 Prompt
            std::cout << "\n[Request " << i << "] Prompt: " << current_prompt << std::endl;

            // Pass &std::cout to preserve streaming type-writer effect on console
            if (max_token_number >= 0)
            {
                llm->response(current_prompt, &std::cout, nullptr, 0);
                while (!llm->stoped() && context->gen_seq_len < max_token_number)
                {
                    llm->generate(1);
                }
            }
            else
            {
                llm->response(current_prompt, &std::cout);
            }

            // 仅记录 Prompt，不再记录 Response
            out_items.push_back({i, current_prompt});

            prompt_len += context->prompt_len;
            decode_len += context->gen_seq_len;
            prefill_time += context->prefill_us;
            decode_time += context->decode_us;
            sample_time += context->sample_us;
        }
    }
    else
    {
        // --- Batched Mode ---
        for (int i = 0; i < valid_prompts.size(); i += batch_size)
        {
            int current_bs = std::min(batch_size, (int)valid_prompts.size() - i);

            // 1. Prepare
            std::vector<std::vector<int>> batch_tokens;
            int max_seq_len = 0;

            for (int b = 0; b < current_bs; b++)
            {
                std::string p = valid_prompts[i + b];
#ifdef MIMO_NO_THINKING
                if (b == 0)
                    llm->set_config("{\"assistant_prompt_template\":\"<|im_start|>assistant\\n<think>\\n</think>\%s<|im_end|>\\n\"}");
                p = p + "<think>\n</think>";
#endif

                // 【修改点】：打印 Batch 中每个请求的 Prompt
                std::cout << "Prompt[" << (i + b) << "]: " << p << std::endl;

                // 【修改点】：不再记录 suppressed 文本，也不记录 response，仅记录 prompt 以便生成 JSON
                out_items.push_back({i + b, p});

                std::string formatted_p = llm->apply_chat_template(p);
                auto tokens = llm->tokenizer_encode(formatted_p);
                if (tokens.size() > max_seq_len)
                    max_seq_len = tokens.size();
                batch_tokens.push_back(tokens);
            }

            // 2. Padding
            std::vector<int> input_ids;
            input_ids.reserve(current_bs * max_seq_len);
            for (const auto &tokens : batch_tokens)
            {
                int pad_len = max_seq_len - tokens.size();
                for (int k = 0; k < pad_len; k++)
                    input_ids.push_back(0);
                input_ids.insert(input_ids.end(), tokens.begin(), tokens.end());
            }

            // 3. Inference
            // 【修改点】：使用 &std::cout 替代 nullptr，强制显示所有生成内容
            std::cout << ">>> Batch Generation Output (All Content):" << std::endl;
            if (max_token_number >= 0)
            {
                llm->response(input_ids, &std::cout, nullptr, 0);
                while (!llm->stoped() && context->gen_seq_len < max_token_number)
                {
                    llm->generate(1);
                }
            }
            else
            {
                llm->response(input_ids, &std::cout);
            }
            std::cout << "\n<<< Batch Generation End" << std::endl;

            // 4. Metrics
            prompt_len += context->prompt_len;
            decode_len += context->gen_seq_len * current_bs; // Total tokens
            prefill_time += context->prefill_us;
            decode_time += context->decode_us;
            sample_time += context->sample_us;

            MNN_PRINT("Batch %d processed.\n", (i / batch_size) + 1);
        }
    }

    llm->generateWavform();

    // Stats
    float vision_s = context->vision_us / 1e6;
    float audio_s = context->audio_us / 1e6;
    float prefill_s = prefill_time / 1e6;
    float decode_s = decode_time / 1e6;
    float sample_s = sample_time / 1e6;

    // Fill Metrics Struct
    out_metrics.batch_size = batch_size;
    out_metrics.prompt_tokens = prompt_len;
    out_metrics.decode_tokens = decode_len;
    out_metrics.prefill_time_s = prefill_s;
    out_metrics.decode_time_s = decode_s;
    out_metrics.total_time_s = prefill_s + decode_s + sample_s;
    out_metrics.prefill_speed = (prefill_s > 0) ? (prompt_len / prefill_s) : 0.0f;
    out_metrics.decode_speed = (decode_s > 0) ? (decode_len / decode_s) : 0.0f;

    // Console Print
    MNN_PRINT("\n#################################\n");
    MNN_PRINT("Batch Size        = %d\n", batch_size);
    MNN_PRINT("prompt tokens num = %d\n", prompt_len);
    MNN_PRINT("decode tokens num = %d\n", decode_len);
    MNN_PRINT(" vision time = %.2f s\n", vision_s);
    MNN_PRINT(" pixels_mp = %.2f MP\n", context->pixels_mp);
    MNN_PRINT("  audio process time = %.2f s\n", audio_s);
    MNN_PRINT("  audio input time = %.2f s\n", context->audio_input_s);
    MNN_PRINT("prefill time = %.2f s\n", prefill_s);
    MNN_PRINT(" decode time = %.2f s\n", decode_s);
    if (prefill_s > 0)
        MNN_PRINT("prefill speed = %.2f tok/s\n", out_metrics.prefill_speed);
    if (decode_s > 0)
        MNN_PRINT(" decode speed = %.2f tok/s\n", out_metrics.decode_speed);
    MNN_PRINT(" vision speed = %.3f MP/s\n", (vision_s > 0) ? context->pixels_mp / vision_s : 0);
    MNN_PRINT(" audio RTF = %.3f \n", (context->audio_input_s > 0) ? audio_s / context->audio_input_s : 0);
    MNN_PRINT("##################################\n");
    return 0;
}

static int ceval(Llm *llm, const std::vector<std::string> &lines, std::string filename)
{
    // C-Eval logic kept identical
    auto csv_data = parse_csv(lines);
    std::vector<std::string> answers;
    for (int i = 1; i < csv_data.size(); i++)
    {
        const auto &elements = csv_data[i];
        std::string prompt = elements[1];
        prompt += "\n\nA. " + elements[2];
        prompt += "\nB. " + elements[3];
        prompt += "\nC. " + elements[4];
        prompt += "\nD. " + elements[5];
        prompt += "\n\n";
        MNN_PRINT("%s", prompt.c_str());
        MNN_PRINT("## 进度: %d / %lu\n", i, lines.size() - 1);
        std::ostringstream lineOs;
        llm->response(prompt.c_str(), &lineOs);
        auto line = lineOs.str();
        MNN_PRINT("%s", line.c_str());
        answers.push_back(line);
    }
    {
        auto position = filename.rfind("/");
        if (position != std::string::npos)
        {
            filename = filename.substr(position + 1, -1);
        }
        position = filename.find("_val");
        if (position != std::string::npos)
        {
            filename.replace(position, 4, "_res");
        }
        std::cout << "store to " << filename << std::endl;
    }
    std::ofstream ofp(filename);
    ofp << "id,answer" << std::endl;
    for (int i = 0; i < answers.size(); i++)
    {
        ofp << i << ",\"" << answers[i] << "\"" << std::endl;
    }
    ofp.close();
    return 0;
}

static int eval(Llm *llm, std::string prompt_file, int max_token_number, int batch_size)
{
    std::cout << "prompt file is " << prompt_file << std::endl;
    std::ifstream prompt_fs(prompt_file);
    std::vector<std::string> prompts;
    std::string prompt;

#ifdef LLM_DEMO_ONELINE
    std::ostringstream tempOs;
    tempOs << prompt_fs.rdbuf();
    prompt = tempOs.str();
    prompts = {prompt};
#else
    while (std::getline(prompt_fs, prompt))
    {
        if (prompt.empty())
            continue;
        if (prompt.back() == '\r')
            prompt.pop_back();
        prompts.push_back(prompt);
    }
#endif
    prompt_fs.close();
    if (prompts.empty())
        return 1;

    // C-Eval Branch
    if (prompts[0] == "id,question,A,B,C,D,answer")
    {
        return ceval(llm, prompts, prompt_file);
    }

    // Benchmark Branch (with JSON)
    std::vector<InferenceItem> items;
    BenchmarkMetrics metrics;
    int ret = benchmark(llm, prompts, max_token_number, batch_size, items, metrics);

    // Generate Filename: input.txt -> input.json
    std::string json_filename = prompt_file;
    size_t last_dot = json_filename.find_last_of(".");
    if (last_dot != std::string::npos)
    {
        json_filename = json_filename.substr(0, last_dot);
    }
    json_filename += ".json";

    write_json_result(json_filename, items, metrics);
    return ret;
}

void chat(Llm *llm)
{
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
        std::cout << "Usage: " << argv[0] << " config.json <prompt.txt> [max_tokens] [batch_size]" << std::endl;
        return 0;
    }
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
    if (true)
    {
        AUTOTIME;
        tuning_prepare(llm.get());
    }
    if (argc < 3)
    {
        chat(llm.get());
        return 0;
    }
    int max_token_number = -1;
    if (argc >= 4)
    {
        std::istringstream os(argv[3]);
        os >> max_token_number;
    }

    // Logic for parsing batch_size (Arg 4) while maintaining compatibility
    int batch_size = 1;
    bool is_old_thinking_arg = false;

    if (argc >= 5)
    {
        std::string arg4 = argv[4];
        if (std::all_of(arg4.begin(), arg4.end(), ::isdigit))
        {
            batch_size = std::stoi(arg4);
        }
        else
        {
            is_old_thinking_arg = true;
        }
    }

    if (is_old_thinking_arg || argc >= 6)
    {
        MNN_PRINT("Set not thinking, only valid for Qwen3\n");
        llm->set_config(R"({"jinja": {"context": {"enable_thinking":false}}})");
    }

    std::string prompt_file = argv[2];
    llm->set_config(R"({"async":false})");
    return eval(llm.get(), prompt_file, max_token_number, batch_size);
}