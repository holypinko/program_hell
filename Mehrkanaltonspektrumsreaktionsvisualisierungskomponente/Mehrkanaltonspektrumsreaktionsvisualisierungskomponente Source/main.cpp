#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL

#include <windows.h>
#include <commdlg.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <cmath>
#include <atomic>
#include <thread>
#include <mutex>
#include <sstream>
#include <iomanip>   
#include <string>    
#include <vector>     
#include <iostream>   
#include <algorithm>
#include <cmath>

// Media Foundation + WASAPI for stuff
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

template<typename T>
T clampVal(const T& v, const T& lo, const T& hi) { return (v < lo) ? lo : (hi < v) ? hi : v; }

static std::string nowStamp() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    std::time_t t = system_clock::to_time_t(tp);
    std::tm tm; localtime_s(&tm, &t);
    std::ostringstream oss; oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "] ";
    return oss.str();
}

std::string openFileDialog(HWND hwnd) {
    wchar_t filename[MAX_PATH] = L"";
    OPENFILENAMEW ofn{}; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFile = filename; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Audio Files\0*.wav;*.mp3;*.flac;*.ogg\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        int required = WideCharToMultiByte(CP_UTF8, 0, filename, -1, nullptr, 0, NULL, NULL);
        std::string path(required - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, filename, -1, &path[0], required, NULL, NULL);
        return path;
    }
    return "";
}

struct P3N3 { float px, py, pz; float nx, ny, nz; };

static std::vector<P3N3> cubeVertsWithNormals() {
    std::vector<P3N3> v;
    auto addTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
        glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        v.push_back(P3N3{ a.x, a.y, a.z, n.x, n.y, n.z });
        v.push_back(P3N3{ b.x, b.y, b.z, n.x, n.y, n.z });
        v.push_back(P3N3{ c.x, c.y, c.z, n.x, n.y, n.z });
        };

    glm::vec3 p000(-0.5f, -0.5f, -0.5f), p100(0.5f, -0.5f, -0.5f), p110(0.5f, 0.5f, -0.5f), p010(-0.5f, 0.5f, -0.5f);
    glm::vec3 p001(-0.5f, -0.5f, 0.5f), p101(0.5f, -0.5f, 0.5f), p111(0.5f, 0.5f, 0.5f), p011(-0.5f, 0.5f, 0.5f);

    addTri(p000, p100, p110); addTri(p110, p010, p000); // back
    addTri(p001, p101, p111); addTri(p111, p011, p001); // front
    addTri(p011, p010, p000); addTri(p000, p001, p011); // left
    addTri(p111, p110, p100); addTri(p100, p101, p111); // right
    addTri(p000, p100, p101); addTri(p101, p001, p000); // bottom
    addTri(p010, p110, p111); addTri(p111, p011, p010); // top
    return v;
}

static const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 iOffset;
layout(location=3) in float iSeed;
layout(location=4) in float iShape;

uniform mat4 VP;
uniform float uTime;
uniform float uAmp;

out float vPop;

float h1(float x){ return fract(sin(x*127.1)*43758.5453); }

vec3 toSphere(vec3 p){ return normalize(p)*0.5; }
vec3 toPyramid(vec3 p){ vec3 q=p; q.y=mix(p.y,0.5*sign(p.y),0.8); q.xz*=0.7; return q; }
vec3 toTetra(vec3 p){ vec3 q=p; q.y=mix(p.y,0.5*sign(p.y),0.9); q.x*=0.6; q.z*=0.6; return q; }
vec3 toOcta(vec3 p){ float m=abs(p.x)+abs(p.y)+abs(p.z); return p*(0.5/max(m,0.0001)); }
vec3 toDiamond(vec3 p){ vec3 q=p; q.xz*=0.6; q.y*=1.2; return normalize(q)*0.5; }
vec3 toWeird(vec3 p){ float k=sin(p.x*6.0)+cos(p.y*7.0)+sin(p.z*5.0); return normalize(p+0.2*vec3(k,k*0.7,-k*0.5))*0.5; }

void main(){
    vec3 cube=aPos;
    vec3 sphere=toSphere(aPos);
    vec3 pyramid=toPyramid(aPos);
    vec3 tetra=toTetra(aPos);
    vec3 octa=toOcta(aPos);
    vec3 diamond=toDiamond(aPos);
    vec3 weird=toWeird(aPos);

    int sid=int(clamp(iShape,0.0,6.0));
    vec3 tgt = (sid==0)?cube:(sid==1)?sphere:(sid==2)?pyramid:(sid==3)?tetra:(sid==4)?octa:(sid==5)?diamond:weird;

    float morph=clamp(uAmp*2.0,0.0,1.0);
    vec3 pos=mix(cube,tgt,morph);

    float bob=(0.6+0.4*h1(iSeed*7.0))*uAmp*2.0;
    float bobWave=sin(uTime*(1.8+h1(iSeed*19.0)*2.0))*bob;

    float gate=sin(uTime*(0.8+h1(iSeed*13.0)*3.0)+iSeed*3.0);
    vPop=smoothstep(0.15,0.95,gate);

    float ang=uTime*(0.25+0.5*h1(iSeed*5.0));
    float cs=cos(ang), sn=sin(ang);
    mat3 rotY=mat3(cs,0,sn, 0,1,0, -sn,0,cs);

    vec3 worldPos=iOffset + vec3(0.0,bobWave,0.0) + rotY*pos;
    gl_Position=VP*vec4(worldPos,1.0);
}
)";

static const char* fsSrc = R"(
#version 330 core
in float vPop;
out vec4 FragColor;
void main(){ FragColor=vec4(vec3(0.0), vPop); }
)";

static GLuint compile(GLenum t, const char* src) {
    GLuint s = glCreateShader(t); glShaderSource(s, 1, &src, nullptr); glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[2048]; glGetShaderInfoLog(s, 2048, nullptr, log); std::cerr << log << "\n"; }
    return s;
}
static GLuint makeProgram(const char* vs, const char* fs) {
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[2048]; glGetProgramInfoLog(p, 2048, nullptr, log); std::cerr << log << "\n"; }
    glDeleteShader(v); glDeleteShader(f); return p;
}

struct InstanceData { std::vector<glm::vec3> offsets; std::vector<float> seeds; std::vector<float> shapes; };

static InstanceData buildSpiral(int layers, int perLayer, float a, float b, float heightScale, float freq, float spacing) {
    InstanceData d; int total = layers * perLayer; d.offsets.resize(total); d.seeds.resize(total); d.shapes.resize(total);
    int idx = 0;
    for (int L = 0; L < layers; ++L) {
        float layerPhase = L * 0.35f;
        for (int i = 0; i < perLayer; ++i) {
            float k = (float)idx; float theta = (k * 0.012f) + layerPhase;
            float r = a + b * theta;
            float x = r * cos(theta), z = r * sin(theta);
            float y = heightScale * sin(theta * freq + L * 0.22f);
            d.offsets[idx] = glm::vec3(x * spacing, y * spacing, z * spacing);
            float seed = (float)idx * 0.123f + (float)L * 17.3f; d.seeds[idx] = seed;
            float h = fmod(seed * 0.71f, 7.0f); int shape = (int)floor(h); if (shape > 6) shape = 6; d.shapes[idx] = (float)shape;
            ++idx;
        }
    }
    return d;
}

static InstanceData buildHelix(int turns, int stepsPerTurn, float radius, float pitch, float spacing) {
    InstanceData d; int total = turns * stepsPerTurn; d.offsets.resize(total); d.seeds.resize(total); d.shapes.resize(total);
    int idx = 0;
    for (int t = 0; t < turns; ++t) {
        for (int s = 0; s < stepsPerTurn; ++s) {
            float u = (t + s / (float)stepsPerTurn) * 2.0f * 3.1415926f;
            float x = radius * cos(u), z = radius * sin(u);
            float y = pitch * u;
            d.offsets[idx] = glm::vec3(x * spacing, y * spacing, z * spacing);
            float seed = (float)idx * 0.987f + t * 33.1f; d.seeds[idx] = seed;
            float h = fmod(seed * 1.11f, 7.0f); int shape = (int)floor(h); if (shape > 6) shape = 6; d.shapes[idx] = (float)shape;
            ++idx;
        }
    }
    return d;
}

// Audio decode + playback MF Source Media + WASAPI (NERRRRD)

struct PCMBuffer {
    std::vector<float> samples; // interleaved float32 [-1..1]
    int sampleRate = 0;
    int channels = 0;
    double durationSec = 0.0;
};

static bool mfInitOnce() {
    static std::once_flag onceFlag;
    static HRESULT hrInit = E_FAIL;
    std::call_once(onceFlag, [&]() { hrInit = MFStartup(MF_VERSION, MFSTARTUP_FULL); });
    return SUCCEEDED(hrInit);
}

static bool decodeToPCMFloat(const std::wstring& pathW, PCMBuffer& out, std::ofstream& devLog, std::ofstream& errLog) {
    if (!mfInitOnce()) { errLog << nowStamp() << "MFStartup failed\n"; return false; }

    IMFSourceReader* reader = nullptr;
    HRESULT hr = MFCreateSourceReaderFromURL(pathW.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) { errLog << nowStamp() << "MFCreateSourceReaderFromURL failed\n"; if (reader) reader->Release(); return false; }

    // Request PCM float output
    IMFMediaType* partialType = nullptr;
    hr = MFCreateMediaType(&partialType);
    if (SUCCEEDED(hr)) {
        partialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        partialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float);
        hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, partialType);
    }
    if (partialType) partialType->Release();

    IMFMediaType* currentType = nullptr;
    WAVEFORMATEX* wfx = nullptr;
    UINT32 cb = 0;
    if (SUCCEEDED(hr)) {
        hr = reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &currentType);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateWaveFormatExFromMFMediaType(currentType, &wfx, &cb);
    }
    if (currentType) currentType->Release();

    if (FAILED(hr) || !wfx) { errLog << nowStamp() << "GetCurrentMediaType/WaveFormat failed\n"; if (wfx) CoTaskMemFree(wfx); reader->Release(); return false; }

    out.sampleRate = (int)wfx->nSamplesPerSec;
    out.channels = (int)wfx->nChannels;
    CoTaskMemFree(wfx);

    out.samples.clear();
    DWORD streamIndex = MF_SOURCE_READER_FIRST_AUDIO_STREAM;

    while (true) {
        IMFSample* sample = nullptr;
        DWORD flags = 0;
        hr = reader->ReadSample(streamIndex, 0, nullptr, &flags, nullptr, &sample);
        if (FAILED(hr)) { errLog << nowStamp() << "ReadSample failed\n"; break; }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) { break; }
        if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) { continue; }
        if (!sample) { continue; }

        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (SUCCEEDED(hr) && buffer) {
            BYTE* pData = nullptr;
            DWORD cbData = 0;
            hr = buffer->Lock(&pData, nullptr, &cbData);
            if (SUCCEEDED(hr) && pData && cbData > 0) {
                size_t countFloats = cbData / sizeof(float);
                size_t oldSize = out.samples.size();
                out.samples.resize(oldSize + countFloats);
                memcpy(out.samples.data() + oldSize, pData, cbData);
            }
            if (SUCCEEDED(hr)) buffer->Unlock();
            buffer->Release();
        }
        sample->Release();
    }

    reader->Release();

    if (out.channels <= 0 || out.sampleRate <= 0 || out.samples.empty()) {
        errLog << nowStamp() << "Decoded PCM invalid\n";
        return false;
    }

    out.durationSec = double(out.samples.size()) / double(out.channels * out.sampleRate);
    devLog << nowStamp() << "Decoded PCM: SR=" << out.sampleRate << " ch=" << out.channels << " samples=" << out.samples.size() << " dur=" << out.durationSec << "s\n";
    return true;
}


struct WasapiPlayer {
    IAudioClient* client = nullptr;
    IAudioRenderClient* render = nullptr;
    IMMDevice* device = nullptr;
    HANDLE hEvent = nullptr;
    std::thread th;
    std::atomic<bool> running{ false };
    std::atomic<bool> finished{ false };
    std::atomic<float> fadeMul{ 1.0f };
    UINT32 bufferFrameCount = 0;
    int sampleRate = 0;
    int channels = 0;

    bool init(const PCMBuffer& pcm, std::ofstream& devLog, std::ofstream& errLog) {
        HRESULT hr;
        IMMDeviceEnumerator* enumr = nullptr;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumr));
        if (FAILED(hr) || !enumr) { errLog << nowStamp() << "MMDeviceEnumerator create failed\n"; return false; }
        hr = enumr->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        enumr->Release();
        if (FAILED(hr) || !device) { errLog << nowStamp() << "GetDefaultAudioEndpoint failed\n"; return false; }

        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
        if (FAILED(hr) || !client) { errLog << nowStamp() << "IAudioClient activate failed\n"; return false; }

        WAVEFORMATEXTENSIBLE wfx = {};
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.nChannels = pcm.channels;
        wfx.Format.nSamplesPerSec = pcm.sampleRate;
        wfx.Format.wBitsPerSample = 32;
        wfx.Format.nBlockAlign = (wfx.Format.nChannels * wfx.Format.wBitsPerSample) / 8;
        wfx.Format.nAvgBytesPerSec = wfx.Format.nSamplesPerSec * wfx.Format.nBlockAlign;
        wfx.Format.cbSize = 22;
        wfx.Samples.wValidBitsPerSample = 32;
        wfx.dwChannelMask = (pcm.channels == 1) ? SPEAKER_FRONT_CENTER : SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        REFERENCE_TIME hnsBufferDuration = 10000000; // 1s
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, hnsBufferDuration, 0, (WAVEFORMATEX*)&wfx, nullptr);
        if (FAILED(hr)) { errLog << nowStamp() << "IAudioClient Initialize failed\n"; return false; }

        hr = client->GetBufferSize(&bufferFrameCount);
        if (FAILED(hr)) { errLog << nowStamp() << "GetBufferSize failed\n"; return false; }

        hr = client->GetService(IID_PPV_ARGS(&render));
        if (FAILED(hr) || !render) { errLog << nowStamp() << "GetService IAudioRenderClient failed\n"; return false; }

        hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
        if (!hEvent) { errLog << nowStamp() << "CreateEvent failed\n"; return false; }
        hr = client->SetEventHandle(hEvent);
        if (FAILED(hr)) { errLog << nowStamp() << "SetEventHandle failed\n"; return false; }

        sampleRate = pcm.sampleRate;
        channels = pcm.channels;
        return true;
    }

    void start(const PCMBuffer& pcm, std::ofstream& devLog) {
        running = true; finished = false;
        th = std::thread([this, &pcm, &devLog]() {
            HRESULT hr = client->Start();
            if (FAILED(hr)) { running = false; finished = true; return; }

            size_t cursor = 0;
            const size_t totalFrames = pcm.samples.size() / pcm.channels;

            while (running) {
                WaitForSingleObject(hEvent, 5);
                UINT32 padding = 0;
                hr = client->GetCurrentPadding(&padding);
                if (FAILED(hr)) break;
                UINT32 framesAvailable = bufferFrameCount - padding;
                if (framesAvailable == 0) continue;

                UINT32 framesToWrite = framesAvailable;
                if (cursor + framesToWrite > totalFrames) framesToWrite = (UINT32)(totalFrames - cursor);

                BYTE* pData = nullptr;
                hr = render->GetBuffer(framesToWrite, &pData);
                if (FAILED(hr) || !pData) break;

                float* outF = (float*)pData;
                const float* inF = pcm.samples.data() + cursor * pcm.channels;
                float mul = fadeMul.load();

                for (UINT32 f = 0; f < framesToWrite; ++f) {
                    for (int c = 0; c < channels; ++c) {
                        outF[f * channels + c] = inF[f * channels + c] * mul;
                    }
                }

                hr = render->ReleaseBuffer(framesToWrite, 0);
                if (FAILED(hr)) break;

                cursor += framesToWrite;
                if (cursor >= totalFrames) break;
            }

            client->Stop();
            finished = true;
            running = false;
            });
    }

    void stop() {
        running = false;
        if (th.joinable()) th.join();
    }

    void release() {
        if (render) { render->Release(); render = nullptr; }
        if (client) { client->Release(); client = nullptr; }
        if (device) { device->Release(); device = nullptr; }
        if (hEvent) { CloseHandle(hEvent); hEvent = nullptr; }
    }
};

int main() {
    HRESULT hrCoInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrCoInit)) {
        std::cerr << "CoInitializeEx failed: 0x" << std::hex << hrCoInit << std::endl;
        return -1;
    }

    std::ofstream devLog("dev.log", std::ios::app); std::ofstream errLog("error.log", std::ios::app);
    devLog << nowStamp() << "Program start\n";

    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; errLog << nowStamp() << "GLFW init failed\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    GLFWwindow* win = glfwCreateWindow(
        1920, 1080,
        "Mehrkanaltonspektrumsreaktionsvisualisierungskomponente",
        nullptr, nullptr
    );

    if (!win) { errLog << nowStamp() << "Failed to create window\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(0);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD init failed\n"; errLog << nowStamp() << "GLAD init failed\n"; return -1; }
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    HWND hwnd = glfwGetWin32Window(win);
    devLog << nowStamp() << "Window created\n";

    devLog << nowStamp() << "Opening file dialog\n";
    std::string audioPath = openFileDialog(hwnd);

    PCMBuffer pcm;
    WasapiPlayer player;
    bool audioReady = false;

    if (audioPath.empty()) {
        errLog << nowStamp() << "No audio selected\n";
        devLog << nowStamp() << "Continuing silent\n";
    }
    else {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, audioPath.c_str(), -1, nullptr, 0);
        std::wstring wpath(wlen, L'\0'); MultiByteToWideChar(CP_UTF8, 0, audioPath.c_str(), -1, &wpath[0], wlen);
        devLog << nowStamp() << "Selected audio: " << audioPath << "\n";

        if (decodeToPCMFloat(wpath, pcm, devLog, errLog)) {
            if (player.init(pcm, devLog, errLog)) {
                player.fadeMul.store(1.0f);
                player.start(pcm, devLog);
                audioReady = true;
                devLog << nowStamp() << "WASAPI playback started\n";
            }
            else {
                errLog << nowStamp() << "WASAPI init failed\n";
            }
        }
        else {
            errLog << nowStamp() << "Decode failed\n";
        }
    }

    std::vector<P3N3> cube = cubeVertsWithNormals();

    struct InstanceData { std::vector<glm::vec3> offsets; std::vector<float> seeds; std::vector<float> shapes; };
    auto buildSpiralLocal = [&](int layers, int perLayer, float a, float b, float heightScale, float freq, float spacing) {
        InstanceData d; int total = layers * perLayer; d.offsets.resize(total); d.seeds.resize(total); d.shapes.resize(total);
        int idx = 0;
        for (int L = 0; L < layers; ++L) {
            float layerPhase = L * 0.35f;
            for (int i = 0; i < perLayer; ++i) {
                float k = (float)idx; float theta = (k * 0.012f) + layerPhase;
                float r = a + b * theta;
                float x = r * cos(theta), z = r * sin(theta);
                float y = heightScale * sin(theta * freq + L * 0.22f);
                d.offsets[idx] = glm::vec3(x * spacing, y * spacing, z * spacing);
                float seed = (float)idx * 0.123f + (float)L * 17.3f; d.seeds[idx] = seed;
                float h = fmod(seed * 0.71f, 7.0f); int shape = (int)floor(h); if (shape > 6) shape = 6; d.shapes[idx] = (float)shape;
                ++idx;
            }
        }
        return d;
        };
    auto buildHelixLocal = [&](int turns, int stepsPerTurn, float radius, float pitch, float spacing) {
        InstanceData d; int total = turns * stepsPerTurn; d.offsets.resize(total); d.seeds.resize(total); d.shapes.resize(total);
        int idx = 0;
        for (int t = 0; t < turns; ++t) {
            for (int s = 0; s < stepsPerTurn; ++s) {
                float u = (t + s / (float)stepsPerTurn) * 2.0f * 3.1415926f;
                float x = radius * cos(u), z = radius * sin(u);
                float y = pitch * u;
                d.offsets[idx] = glm::vec3(x * spacing, y * spacing, z * spacing);
                float seed = (float)idx * 0.987f + t * 33.1f; d.seeds[idx] = seed;
                float h = fmod(seed * 1.11f, 7.0f); int shape = (int)floor(h); if (shape > 6) shape = 6; d.shapes[idx] = (float)shape;
                ++idx;
            }
        }
        return d;
        };

    int spiralLayers = 140, spiralPerLayer = 320;
    float a = 2.0f, b = 0.28f, heightScale = 1.9f, freq = 1.35f, spacing = 2.6f;
    InstanceData spiral = buildSpiralLocal(spiralLayers, spiralPerLayer, a, b, heightScale, freq, spacing);

    int helixTurns = 80, helixSteps = 260; float helixRadius = 5.0f, helixPitch = 0.03f, helixSpacing = 2.6f;
    InstanceData helix = buildHelixLocal(helixTurns, helixSteps, helixRadius, helixPitch, helixSpacing);

    InstanceData inst;
    inst.offsets.reserve(spiral.offsets.size() + helix.offsets.size());
    inst.seeds.reserve(spiral.seeds.size() + helix.seeds.size());
    inst.shapes.reserve(spiral.shapes.size() + helix.shapes.size());
    inst.offsets.insert(inst.offsets.end(), spiral.offsets.begin(), spiral.offsets.end());
    inst.offsets.insert(inst.offsets.end(), helix.offsets.begin(), helix.offsets.end());
    inst.seeds.insert(inst.seeds.end(), spiral.seeds.begin(), spiral.seeds.end());
    inst.seeds.insert(inst.seeds.end(), helix.seeds.begin(), helix.seeds.end());
    inst.shapes.insert(inst.shapes.end(), spiral.shapes.begin(), spiral.shapes.end());
    inst.shapes.insert(inst.shapes.end(), helix.shapes.begin(), helix.shapes.end());

    int instanceCount = (int)inst.offsets.size();
    devLog << nowStamp() << "Instances total=" << instanceCount << "\n";

    std::vector<float> pos; pos.reserve(cube.size() * 3);
    std::vector<float> norm; norm.reserve(cube.size() * 3);
    for (const auto& pn : cube) {
        pos.push_back(pn.px); pos.push_back(pn.py); pos.push_back(pn.pz);
        norm.push_back(pn.nx); norm.push_back(pn.ny); norm.push_back(pn.nz);
    }

    GLuint vao = 0, vboPos = 0, vboNorm = 0, vboOffset = 0, vboSeed = 0, vboShape = 0;
    glGenVertexArrays(1, &vao); glBindVertexArray(vao);

    glGenBuffers(1, &vboPos); glBindBuffer(GL_ARRAY_BUFFER, vboPos);
    glBufferData(GL_ARRAY_BUFFER, pos.size() * sizeof(float), pos.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    glGenBuffers(1, &vboNorm); glBindBuffer(GL_ARRAY_BUFFER, vboNorm);
    glBufferData(GL_ARRAY_BUFFER, norm.size() * sizeof(float), norm.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(1);

    glGenBuffers(1, &vboOffset); glBindBuffer(GL_ARRAY_BUFFER, vboOffset);
    glBufferData(GL_ARRAY_BUFFER, inst.offsets.size() * sizeof(glm::vec3), inst.offsets.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);

    glGenBuffers(1, &vboSeed); glBindBuffer(GL_ARRAY_BUFFER, vboSeed);
    glBufferData(GL_ARRAY_BUFFER, inst.seeds.size() * sizeof(float), inst.seeds.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0); glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1);

    glGenBuffers(1, &vboShape); glBindBuffer(GL_ARRAY_BUFFER, vboShape);
    glBufferData(GL_ARRAY_BUFFER, inst.shapes.size() * sizeof(float), inst.shapes.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(float), (void*)0); glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    GLuint prog = makeProgram(vsSrc, fsSrc);
    GLint locVP = glGetUniformLocation(prog, "VP");
    GLint locTime = glGetUniformLocation(prog, "uTime");
    GLint locAmp = glGetUniformLocation(prog, "uAmp");

    float fadeFactor = 1.0f;
    bool fadeActive = false;
    const float fadeDuration = 3.0f;
    float lastTime = (float)glfwGetTime();

    devLog << nowStamp() << "Entering render loop\n";
    while (!glfwWindowShouldClose(win)) {
        int w = 0, h = 0; glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float t = (float)glfwGetTime();
        float dt = std::max(0.0f, t - lastTime);
        lastTime = t;

        glm::vec3 target(0.0f);
        if (!inst.offsets.empty()) {
            for (const auto& o : inst.offsets) target += o;
            target /= (float)inst.offsets.size();
        }

        float radiusBase = std::max(200.0f, spiralLayers * spacing * 0.8f);
        float cinematicPhase = fmod(t / 20.0f, 3.0f); // cycle every 60 ish secconds
        glm::vec3 camPos;

        if (cinematicPhase < 1.0f) {
            float radius = radiusBase + 80.0f * sin(t * 0.16f);
            camPos = glm::vec3(
                sin(t * 0.22f) * radius,
                70.0f * sin(t * 0.15f),
                cos(t * 0.22f) * radius
            );
        }
        else if (cinematicPhase < 2.0f) {
            float zoom = 250.0f + 150.0f * sin(t * 0.3f);
            camPos = glm::vec3(
                sin(t * 0.5f) * zoom,
                35.0f * sin(t * 0.25f),
                cos(t * 0.5f) * zoom
            );
        }
        else {
            float sweep = radiusBase * 0.5f;
            camPos = glm::vec3(
                sweep * cos(t * 0.1f),
                450.0f * sin(t * 0.2f),
                sweep * sin(t * 0.1f)
            );
        }

        glm::mat4 view = glm::lookAt(camPos, target, glm::vec3(0, 1, 0));
        float aspect = (w > 0 && h > 0) ? (float)w / (float)h : 16.0f / 9.0f;
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.5f, 20000.0f);
        glm::mat4 VP = proj * view;

        glUniformMatrix4fv(locVP, 1, GL_FALSE, glm::value_ptr(VP));

        bool trackFinished = audioReady && player.finished.load();
        if (trackFinished && !fadeActive) {
            fadeActive = true;
            devLog << nowStamp() << "Track finished, starting fade out\n";
        }

        float amp = 0.0f;
        if (audioReady) {
            float timeForEnv = std::min(t, (float)pcm.durationSec);
            int win = std::max(1, pcm.sampleRate / 40);
            int start = std::max(0, (int)(timeForEnv * pcm.sampleRate) - win / 2);
            int end = std::min((int)(pcm.samples.size() / pcm.channels), start + win);
            double sum = 0.0;
            for (int i = start; i < end; ++i) {
                double sL = pcm.samples[i * pcm.channels + 0];
                double sR = (pcm.channels > 1) ? pcm.samples[i * pcm.channels + 1] : sL;
                double s = 0.5 * (std::abs(sL) + std::abs(sR));
                sum += s;
            }
            double avg = (end > start) ? sum / (end - start) : 0.0;
            amp = (float)clampVal(avg, 0.0, 1.0);
            static float s = 0.0f; s = s * 0.85f + amp * 0.15f; amp = s;
        }
        else {
            amp = 0.15f * (0.5f + 0.5f * sin(t * 0.8f));
        }

        if (fadeActive) {
            if (fadeDuration > 0.0f) {
                fadeFactor = std::max(0.0f, fadeFactor - (dt / fadeDuration));
            }
            else {
                fadeFactor = 0.0f;
            }
            player.fadeMul.store(fadeFactor);
        }
        amp *= fadeFactor;

        glUseProgram(prog);
        glUniformMatrix4fv(locVP, 1, GL_FALSE, glm::value_ptr(VP));
        glUniform1f(locTime, t);
        glUniform1f(locAmp, amp);

        glBindVertexArray(vao);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArraysInstanced(GL_TRIANGLES, 0, (GLsizei)cube.size(), instanceCount);

        glfwSwapBuffers(win);
        glfwPollEvents();
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(win, true);

        if (fadeActive && fadeFactor <= 0.0f) {
            player.stop();
        }
    }

    devLog << nowStamp() << "RENDER COMPLETE\n";

    player.stop();
    player.release();

    GLuint buffers[] = { 0, 0, 0, 0, 0 };
    buffers[0] = vboPos; buffers[1] = vboNorm; buffers[2] = vboOffset; buffers[3] = vboSeed; buffers[4] = vboShape;
    for (GLuint b : buffers) { if (b) glDeleteBuffers(1, &b); }
    if (vao) glDeleteVertexArrays(1, &vao);
    if (prog) glDeleteProgram(prog);
    glfwDestroyWindow(win);
    glfwTerminate();
    devLog << nowStamp() << "Program stopped\n";

    MFShutdown();
    CoUninitialize();
    return 0;
}