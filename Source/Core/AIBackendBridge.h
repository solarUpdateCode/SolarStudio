#pragma once
#include <juce_core/juce_core.h>
#include <functional>

namespace solar {

struct AIBackendConfig {
    juce::String localServerUrl = "http://127.0.0.1:8230";
    juce::String externalLlmProvider = "Built-In Solar Agent"; // Built-In, OpenAI (ChatGPT), Anthropic (Claude), Gemini, Ollama
    juce::String externalLlmEndpoint;
    juce::String apiKey;
    bool preferLocalDSP = true;
};

class AIBackendBridge {
public:
    AIBackendConfig config;

    bool isLocalServerReachable() const {
        juce::URL url(config.localServerUrl + "/health");
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs(600);
        if (auto stream = url.createInputStream(options)) {
            auto text = stream->readEntireStreamAsString();
            return text.containsIgnoreCase("ok");
        }
        return false;
    }

    juce::var postJson(const juce::String& endpointPath, const juce::var& payload, int timeoutMs = 4000) const {
        juce::URL url = juce::URL(config.localServerUrl + endpointPath)
                            .withPOSTData(juce::JSON::toString(payload));
        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders("Content-Type: application/json\r\n")
                           .withConnectionTimeoutMs(timeoutMs);
        if (auto stream = url.createInputStream(options)) {
            return juce::JSON::parse(stream->readEntireStreamAsString());
        }
        return {};
    }
};

} // namespace solar
