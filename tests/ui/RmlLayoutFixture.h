#pragma once

#include <RmlUi/Core.h>
#include "UI/RmlBridge/ThemeFileInterface.h"
#include <filesystem>
#include <string>
#include <vector>

namespace UI::Tests
{
std::string ReadFile(const std::filesystem::path& path);
std::string AssetMarkup(const std::string& theme, const std::string& name);
class RmlLayoutFixture
{
    struct LogRecorder : Rml::SystemInterface
    {
        bool LogMessage(Rml::Log::Type, const Rml::String& message) override
        {
            messages.push_back(message);
            return true;
        }
        std::vector<std::string> messages;

        // RmlUi's default JoinPath strips the leading slash of POSIX stylesheet paths.
        void JoinPath(Rml::String& translated, const Rml::String& document, const Rml::String& path) override
        {
            if (!path.empty() && path.front() == '/')
            {
                translated = path;
                return;
            }
            Rml::SystemInterface::JoinPath(translated, document, path);
        }
    };
    struct NullRenderer : Rml::RenderInterface
    {
        Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>, Rml::Span<const int>) override { return 1; }
        void RenderGeometry(Rml::CompiledGeometryHandle, Rml::Vector2f, Rml::TextureHandle) override {}
        void ReleaseGeometry(Rml::CompiledGeometryHandle) override {}
        Rml::TextureHandle LoadTexture(Rml::Vector2i& size, const Rml::String&) override { size = {4096, 4096}; return 1; }
        Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>, Rml::Vector2i) override { return 1; }
        void ReleaseTexture(Rml::TextureHandle) override {}
        void EnableScissorRegion(bool) override {}
        void SetScissorRegion(Rml::Rectanglei) override {}
    };

public:
    RmlLayoutFixture();
    ~RmlLayoutFixture();
    void Configure(Rml::Vector2i viewport, int percent, float contentScale = 1.f);
    Rml::ElementDocument* Load(const std::string& theme, const std::string& name);
    Rml::ElementDocument* LoadMarkup(const std::string& markup);
    void Refresh();
    const std::vector<std::string>& Diagnostics() const { return m_System.messages; }
    Rml::Context* context = nullptr;

private:
    LogRecorder m_System;
    NullRenderer m_Renderer;
    UI::RmlBridge::ThemeFileInterface m_Files;
    int m_PreviousPercent;
    float m_PreviousContentScale;
};

} // namespace UI::Tests
