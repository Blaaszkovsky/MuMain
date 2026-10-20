#include "stdafx.h"

#ifdef _EDITOR
#include "BmdRuntimeProbe.h"
#include "Core/Utilities/Log/MuLogger.h"
#include "Data/DataHandler/LoadData.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Renderer/MuRenderer.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "UI/Common/LookAt.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

extern float BoneScale;

namespace MuEditor::BmdRuntimeProbe
{
namespace
{
constexpr char CaseVariable[] = "MU_BMD_PROBE_CASE";
constexpr double ActionSeconds = 4.0;
constexpr float KeysPerSecond = 6.0f;
constexpr float PreviewFov = 35.0f;
constexpr float Pi = 3.14159265f;
// The editor already uses this one guaranteed spare slot for model previews.
constexpr int PreviewSlot = MAX_MODELS;

struct Sample
{
    std::string_view name;
    const wchar_t* folder;
    const wchar_t* stem;
    const wchar_t* textures;
};

constexpr std::array Samples = {
    Sample{"object", L"Data\\BmdProbe\\Object113\\", L"Object01", L"BmdProbe\\Object113\\"},
    Sample{"monster", L"Data\\BmdProbe\\Monster\\", L"monster300", L"BmdProbe\\Monster\\"},
    Sample{"item", L"Data\\BmdProbe\\Item\\", L"DarkangelSword", L"BmdProbe\\Item\\"},
    Sample{"legacy-item", L"Data\\Item\\", L"Sword01", L"Item\\"},
    Sample{"legacy-monster", L"Data\\Monster\\", L"Monster01", L"Monster\\"},
    Sample{"legacy-object", L"Data\\Object1\\", L"Tree01", L"Object1\\"},
};

const Sample* SelectedSample()
{
    const char* selected = std::getenv(CaseVariable);
    if (selected == nullptr)
        return nullptr;
    for (const auto& sample : Samples)
        if (sample.name == selected)
            return &sample;
    MU_LOG_ERROR(mu::log::Get("data"), "BMD probe unknown case: {}", selected);
    return nullptr;
}

bool Load(const Sample& sample)
{
    if (!gLoadData.AccessModel(PreviewSlot, sample.folder, sample.stem))
        return false;
    const std::wstring folder = sample.textures;
    std::vector<TextureProblem> problems;
    gLoadData.OpenTexture(PreviewSlot, std::span<const std::wstring>(&folder, 1), problems);
    const BMD& model = Models[PreviewSlot];
    MU_LOG_INFO(mu::log::Get("data"), "BMD probe Open2 {} loaded version={} meshes={} bones={} actions={} texture_problems={}",
                sample.name, static_cast<int>(model.Version), model.NumMeshs, model.NumBones, model.NumActions, problems.size());
    mu::log::Get("data")->flush();
    return problems.empty() && model.NumMeshs > 0 && model.NumActions > 0 && model.NumBones <= MAX_BONES;
}

void Animate(BMD& model, double seconds)
{
    const int action = static_cast<int>(seconds / ActionSeconds) % model.NumActions;
    static int previousAction = -1;
    if (action != previousAction)
    {
        MU_LOG_INFO(mu::log::Get("data"), "BMD probe Animation action={} keys={} lock_positions={}", action,
                    model.Actions[action].NumAnimationKeys, model.Actions[action].LockPositions);
        mu::log::Get("data")->flush();
        previousAction = action;
    }
    model.CurrentAction = static_cast<unsigned short>(action);
    const float frame = std::fmod(static_cast<float>(seconds) * KeysPerSecond,
                                 static_cast<float>(std::max(1, model.Actions[action].NumAnimationKeys - 1)));
    vec3_t angle = {};
    model.Animation(BoneTransform, frame, frame, model.CurrentAction, angle, angle, false, false);
}

void CalculateBounds(BMD& model, vec3_t minimum, vec3_t maximum)
{
    vec3_t unusedMinimum = {}, unusedMaximum = {};
    OBB_t box;
    model.Transform(BoneTransform, unusedMinimum, unusedMaximum, &box, true);
    for (int axis = 0; axis < 3; ++axis)
    {
        minimum[axis] = std::numeric_limits<float>::max();
        maximum[axis] = std::numeric_limits<float>::lowest();
    }
    for (int mesh = 0; mesh < model.NumMeshs; ++mesh)
        for (int vertex = 0; vertex < model.Meshs[mesh].NumVertices; ++vertex)
        {
            vec3_t position;
            model.SkinVertex(mesh, vertex, BoneTransform, true, 0.f, position);
            for (int axis = 0; axis < 3; ++axis)
            {
                minimum[axis] = std::min(minimum[axis], position[axis]);
                maximum[axis] = std::max(maximum[axis], position[axis]);
            }
        }
}

void SetCamera(BMD& model)
{
    vec3_t minimum, maximum;
    CalculateBounds(model, minimum, maximum);
    const LookAt::Vector center = {(minimum[0] + maximum[0]) * 0.5f,
                                  (minimum[1] + maximum[1]) * 0.5f,
                                  (minimum[2] + maximum[2]) * 0.5f};
    float diagonalSquared = 0.f;
    for (int axis = 0; axis < 3; ++axis)
        diagonalSquared += (maximum[axis] - minimum[axis]) * (maximum[axis] - minimum[axis]);
    const float radius = std::max(1.f, std::sqrt(diagonalSquared) * 0.5f);
    const float distance = radius / std::tan(PreviewFov * 0.5f * Pi / 180.f) * 1.3f;
    const LookAt::Vector eye = {center[0] + distance, center[1] - distance, center[2] + distance * 0.6f};
    const LookAt::Vector up = {0.f, 0.f, 1.f};
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(PreviewFov, static_cast<float>(WindowWidth) / WindowHeight,
                   std::max(1.f, distance * 0.02f), distance * 4.f + radius);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    const auto view = LookAt::ViewMatrix(eye, center, up);
    mu::GetRenderer().LoadMatrix(view.data());
}

void Draw(BMD& model, double seconds)
{
    BeginOpenglPhysical(0, 0, WindowWidth, WindowHeight);
    glDisable(GL_CULL_FACE);
    glDisable(GL_FOG);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    model.BodyScale = 1.f;
    model.BodyHeight = 0.f;
    Vector(0.f, 0.f, 0.f, model.BodyOrigin);
    Vector(1.f, 1.f, 1.f, model.BodyLight);
    model.LightEnable = false;
    model.ContrastEnable = false;
    BoneScale = 1.f;
    Animate(model, seconds);
    SetCamera(model);
    model.RenderBody(RENDER_TEXTURE, 1.f, -1, 1.f, 0.f, 0.f);
    static bool firstDraw = true;
    if (firstDraw)
    {
        MU_LOG_INFO(mu::log::Get("data"), "BMD probe RenderBody submitted meshes={}", model.NumMeshs);
        mu::log::Get("data")->flush();
        firstDraw = false;
    }
    EndOpengl();
}
} // namespace

bool IsEnabled()
{
    static const bool enabled = SelectedSample() != nullptr;
    return enabled;
}

bool Render()
{
    static const Sample* sample = SelectedSample();
    if (sample == nullptr)
        return false;
    static const bool loaded = Load(*sample);
    if (!loaded)
        return true;
    static const auto start = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    mu::GetRenderer().SetClearColor(0.04f, 0.05f, 0.07f, 1.f);
    Draw(Models[PreviewSlot], seconds);
    return true;
}
} // namespace MuEditor::BmdRuntimeProbe
#endif
