// Copyright 2019 The Dawn Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <array>

#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
// #include <pthread.h>

#include <thread>
#include <mutex>
#include <condition_variable>

#include "mat4.h"

#ifdef __EMSCRIPTEN__
#include <webgpu/webgpu_cpp.h>
#else
#include <dawn/webgpu_cpp.h>

#include "window.h"

#include <GLFW/glfw3.h>
// #include "third_party/glfw3/include/GLFW/glfw3.h"

#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#include <GLFW/glfw3native.h>
// #include "GLFW/glfw3native.h"
// #include "third_party/glfw3/include/GLFW/glfw3native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#endif

#undef NDEBUG

#define MULTITHREADED_RENDERING

static wgpu::Device device;
static wgpu::Queue queue;
static wgpu::Buffer readbackBuffer;
static wgpu::RenderPipeline pipeline;

static wgpu::BindGroup uniformBindGroup;
static wgpu::Buffer uniformBuffer;

static int testsCompleted = 0;

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>

void GetDevice(void (*callback)(wgpu::Device)) {
    // Left as null (until supported in Emscripten)
    static const WGPUInstance instance = nullptr;

    wgpuInstanceRequestAdapter(instance, nullptr, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
        if (message) {
            printf("wgpuInstanceRequestAdapter: %s\n", message);
        }
        if (status == WGPURequestAdapterStatus_Unavailable) {
            printf("WebGPU unavailable; exiting cleanly\n");
            // exit(0) (rather than emscripten_force_exit(0)) ensures there is no dangling keepalive.
            exit(0);
        }
        assert(status == WGPURequestAdapterStatus_Success);

        wgpuAdapterRequestDevice(adapter, nullptr, [](WGPURequestDeviceStatus status, WGPUDevice dev, const char* message, void* userdata) {
            if (message) {
                printf("wgpuAdapterRequestDevice: %s\n", message);
            }
            assert(status == WGPURequestDeviceStatus_Success);

            wgpu::Device device = wgpu::Device::Acquire(dev);
            reinterpret_cast<void (*)(wgpu::Device)>(userdata)(device);
        }, userdata);
    }, reinterpret_cast<void*>(callback));
}
#else  // __EMSCRIPTEN__
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

static std::unique_ptr<dawn::native::Instance> instance;

static wgpu::Surface surface;


static window_t* native_window;


// window.cpp

#define UNUSED_VAR(x) ((void)(x))
#define UNUSED_FUNCTION(x) ((void)(x))

#define GET_DEFAULT_IF_ZERO(value, default_value)                              \
  (value != NULL) ? value : default_value

struct window {
  GLFWwindow* handle;
  struct {
    WGPUSurface handle;
    uint32_t width, height;
    float dpscale;
  } surface;
  callbacks_t callbacks;
  int intialized;
  /* common data */
  float mouse_scroll_scale_factor;
  void* userdata;
};

static std::unique_ptr<wgpu::ChainedStruct> SurfaceDescriptor(void* display,
                                                              void* window)
{
#if defined(WIN32)
  std::unique_ptr<wgpu::SurfaceDescriptorFromWindowsHWND> desc
    = std::make_unique<wgpu::SurfaceDescriptorFromWindowsHWND>();
  desc->hwnd      = window;
  desc->hinstance = GetModuleHandle(nullptr);
  return std::move(desc);
#elif defined(__linux__) // X11
  std::unique_ptr<wgpu::SurfaceDescriptorFromXlibWindow> desc
    = std::make_unique<wgpu::SurfaceDescriptorFromXlibWindow>();
  desc->display = display;
  desc->window  = *((uint32_t*)window);
  return std::move(desc);
#endif

  return nullptr;
}

WGPUSurface CreateSurface(void* display, void* window)
// WGPUSurface CreateSurface(dawn::native::Instance* instance, void* display, void* window)
{
  std::unique_ptr<wgpu::ChainedStruct> sd = SurfaceDescriptor(display, window);
  wgpu::SurfaceDescriptor descriptor;
  descriptor.nextInChain = sd.get();
  // wgpu::Surface surface = wgpu::Instance(gpuContext.dawn_native.instance->Get())
  //                           .CreateSurface(&descriptor);
//   wgpu::Surface surface = wgpu::Instance(instance->Get())
//                             .CreateSurface(&descriptor);
//   wgpu::Surface surface = wgpu::Instance(instance->Get())
//                             .CreateSurface(&descriptor);
  surface = wgpu::Instance(instance->Get())
                            .CreateSurface(&descriptor);
  if (!surface) {
    return nullptr;
  }
  WGPUSurface surf = surface.Get();
  wgpuSurfaceReference(surf);
  return surf;
}

/* Function prototypes */
static void surface_update_framebuffer_size(window_t* window);
static void glfw_window_error_callback(int error, const char* description);
static void glfw_window_key_callback(GLFWwindow* src_window, int key,
                                     int scancode, int action, int mods);
static void glfw_window_cursor_position_callback(GLFWwindow* src_window,
                                                 double xpos, double ypos);
static void glfw_window_mouse_button_callback(GLFWwindow* src_window,
                                              int button, int action, int mods);
static void glfw_window_scroll_callback(GLFWwindow* src_window, double xoffset,
                                        double yoffset);
static void glfw_window_size_callback(GLFWwindow* src_window, int width,
                                      int height);

window_t* window_create(window_config_t* config)
{
  if (!config) {
    return NULL;
  }

  window_t* window = (window_t*)malloc(sizeof(window_t));
  memset(window, 0, sizeof(window_t));
  window->mouse_scroll_scale_factor = 1.0f;

  /* Initialize error handling */
  glfwSetErrorCallback(glfw_window_error_callback);

  /* Initialize the library */
  if (!glfwInit()) {
    /* Handle initialization failure */
    fprintf(stderr, "Failed to initialize GLFW\n");
    fflush(stderr);
    return window;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, config->resizable ? GLFW_TRUE : GLFW_FALSE);

  /* Create GLFW window */
  window->handle = glfwCreateWindow(config->width, config->height,
                                    config->title, NULL, NULL);

  /* Confirm that GLFW window was created successfully */
  if (!window->handle) {
    glfwTerminate();
    fprintf(stderr, "Failed to create window\n");
    fflush(stderr);
    return window;
  }

  surface_update_framebuffer_size(window);

  /* Set user pointer to window class */
  glfwSetWindowUserPointer(window->handle, (void*)window);

  /* -- Setup callbacks -- */
  /* clang-format off */
  /* Key input events */
//   glfwSetKeyCallback(window->handle, glfw_window_key_callback);
//   /* Cursor position */
//   glfwSetCursorPosCallback(window->handle, glfw_window_cursor_position_callback);
//   /* Mouse button input */
//   glfwSetMouseButtonCallback(window->handle, glfw_window_mouse_button_callback);
//   /* Scroll input */
//   glfwSetScrollCallback(window->handle, glfw_window_scroll_callback);
  /* Window resize events */
  glfwSetWindowSizeCallback(window->handle, glfw_window_size_callback);
  /* clang-format on */

  /* Change the state of the window to intialized */
  window->intialized = 1;

  return window;
}

void window_destroy(window_t* window)
{
  /* Cleanup window(s) */
  if (window) {
    if (window->handle) {
      glfwDestroyWindow(window->handle);
      window->handle = NULL;

      /* Terminate GLFW */
      glfwTerminate();
    }

    /* Free allocated memory */
    free(window);
    window = NULL;
  }
}

int window_should_close(window_t* window)
{
  return glfwWindowShouldClose(window->handle);
}

void window_set_title(window_t* window, const char* title)
{
  glfwSetWindowTitle(window->handle, title);
}

void window_set_userdata(window_t* window, void* userdata)
{
  window->userdata = userdata;
}

void* window_get_userdata(window_t* window)
{
  return window->userdata;
}

// void* window_get_surface(dawn::native::Instance* instance, window_t* window)
void* window_get_surface(window_t* window)
{
#if defined(WIN32)
  void* display         = NULL;
  uint32_t windowHandle = glfwGetWin32Window(window->handle);
#elif defined(__linux__) /* X11 */
  void* display         = glfwGetX11Display();
  uint32_t windowHandle = glfwGetX11Window(window->handle);
#endif
  // window->surface.handle = wgpu_create_surface(display, &windowHandle);
//   window->surface.handle = CreateSurface(instance, display, &windowHandle);
  window->surface.handle = CreateSurface(display, &windowHandle);

  return window->surface.handle;
}

void window_get_size(window_t* window, uint32_t* width, uint32_t* height)
{
  *width  = window->surface.width;
  *height = window->surface.height;
}

void window_get_aspect_ratio(window_t* window, float* aspect_ratio)
{
  *aspect_ratio = (float)window->surface.width / (float)window->surface.height;
}




static void surface_update_framebuffer_size(window_t* window)
{
  if (window) {
    float yscale = 1.0;
    glfwGetFramebufferSize(window->handle, (int*)&(window->surface.width),
                           (int*)&window->surface.height);
    glfwGetWindowContentScale(window->handle, &window->surface.dpscale,
                              &yscale);
  }
}

static void glfw_window_size_callback(GLFWwindow* src_window, int width,
                                      int height)
{
  UNUSED_VAR(width);
  UNUSED_VAR(height);

  surface_update_framebuffer_size(
    (window_t*)glfwGetWindowUserPointer(src_window));
}



static void glfw_window_error_callback(int error, const char* description)
{
  fprintf(stderr, "GLFW Error occured, Error id: %i, Description: %s\n", error,
          description);
}






////////////////////////

#define WGPU_RELEASE_RESOURCE(Type, Name)                                      \
  if (Name) {                                                                  \
    wgpu##Type##Release(Name);                                                 \
    Name = NULL;                                                               \
  }

///////////


// Return backend select priority, smaller number means higher priority.
int GetBackendPriority(wgpu::BackendType t) {
  switch (t) {
    case wgpu::BackendType::Null:
      return 9999;
    case wgpu::BackendType::D3D12:
    case wgpu::BackendType::Metal:
    case wgpu::BackendType::Vulkan:
      return 0;
    case wgpu::BackendType::WebGPU:
      return 5;
    case wgpu::BackendType::D3D11:
    case wgpu::BackendType::OpenGL:
    case wgpu::BackendType::OpenGLES:
      return 10;
  }
  return 100;
}

const char* BackendTypeName(wgpu::BackendType t)
{
  switch (t) {
    case wgpu::BackendType::Null:
      return "Null";
    case wgpu::BackendType::WebGPU:
      return "WebGPU";
    case wgpu::BackendType::D3D11:
      return "D3D11";
    case wgpu::BackendType::D3D12:
      return "D3D12";
    case wgpu::BackendType::Metal:
      return "Metal";
    case wgpu::BackendType::Vulkan:
      return "Vulkan";
    case wgpu::BackendType::OpenGL:
      return "OpenGL";
    case wgpu::BackendType::OpenGLES:
      return "OpenGL ES";
  }
  return "?";
}

const char* AdapterTypeName(wgpu::AdapterType t)
{
  switch (t) {
    case wgpu::AdapterType::DiscreteGPU:
      return "Discrete GPU";
    case wgpu::AdapterType::IntegratedGPU:
      return "Integrated GPU";
    case wgpu::AdapterType::CPU:
      return "CPU";
    case wgpu::AdapterType::Unknown:
      return "Unknown";
  }
  return "?";
}

// void GetDevice(void (*callback)(wgpu::Device)) {
void GetDevice() {
    instance = std::make_unique<dawn::native::Instance>();
    instance->DiscoverDefaultAdapters();

    auto adapters = instance->GetAdapters();

    // Sort adapters by adapterType, 
    std::sort(adapters.begin(), adapters.end(), [](const dawn::native::Adapter& a, const dawn::native::Adapter& b){
        wgpu::AdapterProperties pa, pb;
        a.GetProperties(&pa);
        b.GetProperties(&pb);
        
        if (pa.adapterType != pb.adapterType) {
            // Put GPU adapter (D3D, Vulkan, Metal) at front and CPU adapter at back.
            return pa.adapterType < pb.adapterType;
        }

        return GetBackendPriority(pa.backendType) < GetBackendPriority(pb.backendType);
    });
    // Simply pick the first adapter in the sorted list.
    dawn::native::Adapter backendAdapter = adapters[0];

    printf("Available adapters sorted by their Adapter type, with GPU adapters listed at front and preferred:\n\n");
    printf(" [Selected] -> ");
    for (auto&& a : adapters) {
        wgpu::AdapterProperties p;
        a.GetProperties(&p);
        printf(
            "* %s (%s)\n"
            "    deviceID=%u, vendorID=0x%x, BackendType::%s, AdapterType::%s\n",
        p.name, p.driverDescription, p.deviceID, p.vendorID,
        BackendTypeName(p.backendType), AdapterTypeName(p.adapterType));
    }
    printf("\n\n");

    device = wgpu::Device::Acquire(backendAdapter.CreateDevice());
    DawnProcTable procs = dawn::native::GetProcs();

    dawnProcSetProcs(&procs);
    // callback(device);
}
#endif  // __EMSCRIPTEN__

// const uint32_t kDrawVertexCount = 3;
static constexpr uint32_t kDrawVertexCount = 6;

static constexpr uint32_t kQuadPerRow = 16;
static constexpr uint32_t kNumInstances = kQuadPerRow * kQuadPerRow;

// static constexpr uint32_t numThreads = 1;
static constexpr uint32_t numThreads = 2;
// static constexpr uint32_t numThreads = 4;
// static constexpr size_t numObjectsPerThread = (kNumInstances + numThreads - 1) / numThreads;
static constexpr size_t numObjectsPerThread = kNumInstances / numThreads;


struct DrawObjectData {
    Mat4 mat4;
    // Vec3 color;
    // uint32_t idx;
    // vertex buffer
    // size_t offset;
    // matrix
};

// static constexpr uint32_t matrixElementCount = 4 * 4;  // 4x4 matrix
// static constexpr uint32_t matrixByteSize = sizeof(float) * matrixElementCount;
// static constexpr uint64_t uniformBufferSize = matrixByteSize * kNumInstances;
static constexpr uint64_t uniformBufferSize = sizeof(DrawObjectData) * kNumInstances;

static std::array<DrawObjectData, kNumInstances> objectData;


static const char shaderCodeTriangle[] = R"(
    @vertex
    fn main_v(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4<f32> {
        var pos = array<vec2<f32>, 3>(
            vec2<f32>(0.0, 0.5), vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5));
        return vec4<f32>(pos[idx], 0.0, 1.0);
    }
    @fragment
    fn main_f() -> @location(0) vec4<f32> {
        return vec4<f32>(0.0, 0.502, 1.0, 1.0); // 0x80/0xff ~= 0.502
    }
)";

// For multithreading, draw a animated quad for each draw command
static const char shaderCode[] = R"(
    const pos = array<vec2<f32>, 6>(
            vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5), vec2<f32>(-0.5, 0.5),
            vec2<f32>(-0.5, 0.5), vec2<f32>(0.5, -0.5), vec2<f32>(0.5, 0.5)
        );

    struct Uniforms {
        matrix : array<mat4x4<f32>, 256>,   // temp
        // matrix : mat4x4<f32>,
    }

    @binding(0) @group(0) var<uniform> uniforms : Uniforms;

    struct VertexOutput {
        @builtin(position) Position: vec4<f32>,
        @location(0) @interpolate(flat) instance_idx: u32,
    }

    struct FragmentInput {
        @location(0) @interpolate(flat) instance_idx: u32,
    }

    @vertex
    fn main_v(
        @builtin(vertex_index) vid: u32,
        @builtin(instance_index) iid: u32
    ) -> VertexOutput {
        var shader_io: VertexOutput;
        // Basic matrix transform animation
        shader_io.Position = uniforms.matrix[iid] * vec4<f32>(pos[vid], 0.0, 1.0);
        // shader_io.Position = vec4<f32>(pos[vid], 0.0, 1.0);
        shader_io.instance_idx = iid;
        return shader_io;
    }

    override kQuadPerSide: f32; 
    // override kNumInstances: f32;

    @fragment
    fn main_f(shader_io: FragmentInput) -> @location(0) vec4<f32> {
        // return vec4<f32>(0.0, 0.502, 1.0, 1.0); // 0x80/0xff ~= 0.502
        // return vec4<f32>(f32(shader_io.instance_idx) / kNumInstances, 0.502, 1.0, 1.0); // 0x80/0xff ~= 0.502
        return vec4<f32>(trunc(f32(shader_io.instance_idx) / kQuadPerSide) / kQuadPerSide, 0.502, (f32(shader_io.instance_idx) % kQuadPerSide) / kQuadPerSide, 1.0);
    }
)";

void init() {
    device.SetUncapturedErrorCallback(
        [](WGPUErrorType errorType, const char* message, void*) {
            printf("%d: %s\n", errorType, message);
        }, nullptr);

    queue = device.GetQueue();

    wgpu::ShaderModule shaderModule{};
    {
        wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        // wgslDesc.source = shaderCodeTriangle;
        wgslDesc.source = shaderCode;

        wgpu::ShaderModuleDescriptor descriptor{};
        descriptor.nextInChain = &wgslDesc;
        shaderModule = device.CreateShaderModule(&descriptor);
    }

    // wgpu::BindGroupLayout bgl;
    // {
    //     wgpu::BindGroupLayoutDescriptor bglDesc{};

    //     bgl = device.CreateBindGroupLayout(&bglDesc);


    //     // wgpu::BindGroupDescriptor desc{};
    //     // desc.layout = bgl;
    //     // desc.entryCount = 0;
    //     // desc.entries = nullptr;
    //     // device.CreateBindGroup(&desc);
    // }

    {
        // wgpu::PipelineLayoutDescriptor pl{};
        // pl.bindGroupLayoutCount = 0;
        // pl.bindGroupLayouts = nullptr;

        wgpu::ColorTargetState colorTargetState{};
        colorTargetState.format = wgpu::TextureFormat::BGRA8Unorm;

        wgpu::FragmentState fragmentState{};
        fragmentState.module = shaderModule;
        fragmentState.entryPoint = "main_f";
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTargetState;
        std::vector<wgpu::ConstantEntry> constants{
            {nullptr, "kQuadPerSide", kQuadPerRow},
            // {nullptr, "kNumInstances", kNumInstances},
        };
        fragmentState.constants = constants.data();
        fragmentState.constantCount = constants.size();

        wgpu::DepthStencilState depthStencilState{};
        depthStencilState.format = wgpu::TextureFormat::Depth32Float;

        wgpu::RenderPipelineDescriptor descriptor{};
        // descriptor.layout = device.CreatePipelineLayout(&pl);
        descriptor.layout = nullptr;
        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = "main_v";
        descriptor.fragment = &fragmentState;
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.depthStencil = &depthStencilState;
        pipeline = device.CreateRenderPipeline(&descriptor);
    }

    {
        // TODO: Replace with storage buffer

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = uniformBufferSize;
        descriptor.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        // descriptor.mappedAtCreation = true;
        uniformBuffer = device.CreateBuffer(&descriptor);

        // float* ptr = static_cast<float*>(uniformBuffer.GetMappedRange());
        // assert(ptr != nullptr);
        // for (uint32_t i = 0; i < kNumInstances; i++) {
        //     float* o = ptr + i * matrixByteSize;
        //     for (uint32_t j = 0; j < matrixElementCount; j++) {
        //         o[j] = 0.0;
        //     }
        //     o[0] = 1;
        //     o[5] = 1;
        //     o[10] = 1;
        //     o[15] = 1;
        // }
        // uniformBuffer.Unmap();

        const float quadSize = 2.0 / (float) kQuadPerRow;
        const float quadOffsetBase = -1.0 + 0.5 * quadSize;

        for (uint32_t x = 0; x < kQuadPerRow; x++) {
            for (uint32_t y = 0; y < kQuadPerRow; y++) {
                DrawObjectData& d = objectData[x + y * kQuadPerRow];

                d.mat4 = Mat4::Translation(
                    Vec3(
                        (float)x * quadSize + quadOffsetBase,
                        (float)y * quadSize + quadOffsetBase,
                        0.0f)
                ) * Mat4::Scale(quadSize);
                // d.color = Vec3((float)x / kQuadPerRow, (float)y / kQuadPerRow, 0.5);
            }
        }

        queue.WriteBuffer(uniformBuffer, 0, objectData.data(), uniformBufferSize);
    }

    wgpu::BindGroupEntry bindEntries[] = {
        { nullptr, 0, uniformBuffer },
    };

    {
        wgpu::BindGroupDescriptor desc{};
        // desc.layout = bgl;
        desc.layout = pipeline.GetBindGroupLayout(0);
        desc.entryCount = 1;
        desc.entries = bindEntries;
        uniformBindGroup = device.CreateBindGroup(&desc);
    }
}

static bool program_running = true;
static std::mutex deviceMutex;

#if defined(MULTITHREADED_RENDERING)

// ready to build command buffer
static std::mutex frameMutex;
static std::condition_variable frame_cv;
static std::mutex renderMutex;
static std::condition_variable render_cv;
static bool frameStartRendering = false;
static uint32_t renderthreadsFinished = 0;

struct ThreadRenderData {
    // wgpu::CommandBuffer commands;
    // wgpu::CommandBuffer& commands;
    wgpu::RenderPassDescriptor renderpass;
    // // Per object information (position, rotation, etc.)
    // std::vector<DrawObjectData> objectData;

    // std::vector<DrawObjectData&> objectDataRefs;
    std::vector<size_t> objectIds;

    // bool commandsBufferDone = false;
    uint32_t threadIdx;

    std::mutex m;
    std::condition_variable condition;

    bool rendering = false;
};

static std::array<ThreadRenderData, numThreads> threadData;
static std::array<wgpu::CommandBuffer, numThreads> commandBuffers;
static std::vector<std::thread> renderThreads;

void threadRenderFunc(ThreadRenderData& data) {
    while (program_running) {        
        {
            std::unique_lock lock(data.m);
            // printf("Thread %u waiting mutex\n", data.threadIdx);
            
            data.condition.wait(lock, [&]{ return data.rendering == true || !program_running; });
            if (!program_running) {
                break;
            }
            // printf("Thread finish waiting frame\n");
        }

        wgpu::CommandEncoder encoder;
        {
            std::scoped_lock lock(deviceMutex);
            encoder = device.CreateCommandEncoder();
        }

        // printf("Thread %u recording command\n", data.threadIdx);

        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&data.renderpass);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, uniformBindGroup);
            // pass.Draw(kDrawVertexCount);
            for (size_t id : data.objectIds) {
                pass.Draw(kDrawVertexCount, 1, 0, id);
            }
            // printf("%lu\n", data.objectIds.back());
            pass.End();
        }
        commandBuffers[data.threadIdx] = encoder.Finish();

        {
            std::scoped_lock lock(data.m);
            data.rendering = false;
            // printf("Thread %u notify\n", data.threadIdx);
            // renderthreadsFinished++;
            data.condition.notify_one();
        }
    }
}

void setupThreads() {
    size_t objectId = 0;

    for (uint32_t i = 0; i < numThreads; i++) {
        threadData[i].threadIdx = i;
        for (size_t j = 0; j < numObjectsPerThread; j++) {
            threadData[i].objectIds.push_back(objectId++);
        }
    }

    for (uint32_t i = 0; i < numThreads; i++) {
        renderThreads.emplace_back(threadRenderFunc, std::ref(threadData[i]));
    }
}

void multiThreadedRender(wgpu::TextureView view, wgpu::TextureView depthStencilView, wgpu::RenderPassDescriptor renderpass) {
    // printf("    render frame starts\n");
    wgpu::RenderPassColorAttachment attachment{};
    attachment.view = view;
    attachment.loadOp = wgpu::LoadOp::Load;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = {0, 0, 0, 1};

    wgpu::RenderPassDescriptor threadRenderpass{};
    threadRenderpass.colorAttachmentCount = 1;
    threadRenderpass.colorAttachments = &attachment;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = depthStencilView;
    depthStencilAttachment.depthClearValue = 0;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Load;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;

    threadRenderpass.depthStencilAttachment = &depthStencilAttachment;

    for (ThreadRenderData& d : threadData) {
        d.renderpass = threadRenderpass;
        d.rendering = true;
        d.condition.notify_one();
    }

    for (uint32_t i = 0; i < numThreads; i++) {
        std::unique_lock<std::mutex> lock(threadData[i].m);
        // printf("    render waits threads %u\n", i);
        threadData[i].condition.wait(lock, [=]{ return threadData[i].rendering == false;});
    }

    // printf("    render submits commands\n\n");
    // for (ThreadRenderData& d : threadData) {
    //     queue.Submit(1, &d.commands);
    //     // device.GetQueue().Submit(1, &d.commands);
    // }
    {
        std::scoped_lock lock(deviceMutex);
        // printf("    render submits commands\n\n");

        // clear pass
        {
            wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
            pass.End();
            wgpu::CommandBuffer clearCommand = encoder.Finish();
            queue.Submit(1, &clearCommand);
        }

        queue.Submit(commandBuffers.size(), commandBuffers.data());
    }
}

#else

// The depth stencil attachment isn't really needed to draw the triangle
// and doesn't really affect the render result.
// But having one should give us a slightly better test coverage for the compile of the depth stencil descriptor.
void render(wgpu::TextureView view, wgpu::TextureView depthStencilView, wgpu::RenderPassDescriptor renderpass) {
    
    // // Try submitting multiple command buffer
    // std::array<wgpu::CommandBuffer, 2> commands;
    // {
    //     wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    //     {
    //         wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    //         pass.SetPipeline(pipeline);
    //         pass.SetBindGroup(0, uniformBindGroup);
    //         // for (size_t i = 0; i < kNumInstances / 2; i++) {
    //         //     pass.Draw(kDrawVertexCount, 1, 0, i);
    //         // }
    //         // for (size_t i = kNumInstances / 2; i < kNumInstances; i++) {
    //         //     pass.Draw(kDrawVertexCount, 1, 0, i);
    //         // }
    //         // pass.Draw(kDrawVertexCount, 1, 0, 127);
    //         pass.Draw(kDrawVertexCount, 1, 0, 255);
    //         // pass.Draw(kDrawVertexCount, 1, 0, 0);
    //         pass.End();
    //     }
    //     commands[0] = encoder.Finish();
    // }
    // {
    //     wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    //     {
    //         wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
    //         pass.SetPipeline(pipeline);
    //         pass.SetBindGroup(0, uniformBindGroup);
    //         // for (size_t i = kNumInstances / 2; i < kNumInstances; i++) {
    //         //     pass.Draw(kDrawVertexCount, 1, 0, i);
    //         // }
    //         // for (size_t i = 0; i < kNumInstances / 2; i++) {
    //         //     pass.Draw(kDrawVertexCount, 1, 0, i);
    //         // }
    //         // pass.Draw(kDrawVertexCount, 1, 0, 255);
    //         // pass.Draw(kDrawVertexCount, 1, 0, 127);
    //         pass.Draw(kDrawVertexCount, 1, 0, 3);
    //         pass.End();
    //     }
    //     commands[1] = encoder.Finish();
    // }
    // queue.Submit(commands.size(), commands.data());
    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        {
            wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);
            pass.SetPipeline(pipeline);
            pass.SetBindGroup(0, uniformBindGroup);
            // pass.Draw(kDrawVertexCount);
            pass.Draw(kDrawVertexCount, kNumInstances, 0, 0);
            pass.End();
        }
        commands = encoder.Finish();
    }
    queue.Submit(1, &commands);
}

#endif  // MULTITHREADED_RENDERING

void issueContentsCheck(const char* functionName,
        wgpu::Buffer readbackBuffer, uint32_t expectData) {
    struct UserData {
        const char* functionName;
        wgpu::Buffer readbackBuffer;
        uint32_t expectData;
    };

    UserData* userdata = new UserData;
    userdata->functionName = functionName;
    userdata->readbackBuffer = readbackBuffer;
    userdata->expectData = expectData;

    readbackBuffer.MapAsync(
        wgpu::MapMode::Read, 0, 4,
        [](WGPUBufferMapAsyncStatus status, void* vp_userdata) {
            assert(status == WGPUBufferMapAsyncStatus_Success);
            std::unique_ptr<UserData> userdata(reinterpret_cast<UserData*>(vp_userdata));

            const void* ptr = userdata->readbackBuffer.GetConstMappedRange();

            printf("%s: readback -> %p%s\n", userdata->functionName,
                    ptr, ptr ? "" : " <------- FAILED");
            assert(ptr != nullptr);
            uint32_t readback = static_cast<const uint32_t*>(ptr)[0];
            printf("  got %08x, expected %08x%s\n",
                readback, userdata->expectData,
                readback == userdata->expectData ? "" : " <------- FAILED");
            userdata->readbackBuffer.Unmap();

            testsCompleted++;
        }, userdata);
}

void doCopyTestMappedAtCreation(bool useRange) {
    static constexpr uint32_t kValue = 0x05060708;
    size_t size = useRange ? 12 : 4;
    wgpu::Buffer src;
    {
        wgpu::BufferDescriptor descriptor{};
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::CopySrc;
        descriptor.mappedAtCreation = true;
        src = device.CreateBuffer(&descriptor);
    }
    size_t offset = useRange ? 8 : 0;
    uint32_t* ptr = static_cast<uint32_t*>(useRange ?
            src.GetMappedRange(offset, 4) :
            src.GetMappedRange());
    printf("%s: getMappedRange -> %p%s\n", __FUNCTION__,
            ptr, ptr ? "" : " <------- FAILED");
    assert(ptr != nullptr);
    *ptr = kValue;
    src.Unmap();

    wgpu::Buffer dst;
    {
        wgpu::BufferDescriptor descriptor{};
        descriptor.size = 4;
        descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        dst = device.CreateBuffer(&descriptor);
    }

    wgpu::CommandBuffer commands;
    {
        wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(src, offset, dst, 0, 4);
        commands = encoder.Finish();
    }
    queue.Submit(1, &commands);

    issueContentsCheck(__FUNCTION__, dst, kValue);
}

void doCopyTestMapAsync(bool useRange) {
    static constexpr uint32_t kValue = 0x01020304;
    size_t size = useRange ? 12 : 4;
    wgpu::Buffer src;
    {
        wgpu::BufferDescriptor descriptor{};
        descriptor.size = size;
        descriptor.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
        src = device.CreateBuffer(&descriptor);
    }
    size_t offset = useRange ? 8 : 0;

    struct UserData {
        const char* functionName;
        bool useRange;
        size_t offset;
        wgpu::Buffer src;
    };

    UserData* userdata = new UserData;
    userdata->functionName = __FUNCTION__;
    userdata->useRange = useRange;
    userdata->offset = offset;
    userdata->src = src;

    src.MapAsync(wgpu::MapMode::Write, offset, 4,
        [](WGPUBufferMapAsyncStatus status, void* vp_userdata) {
            assert(status == WGPUBufferMapAsyncStatus_Success);
            std::unique_ptr<UserData> userdata(reinterpret_cast<UserData*>(vp_userdata));

            uint32_t* ptr = static_cast<uint32_t*>(userdata->useRange ?
                    userdata->src.GetMappedRange(userdata->offset, 4) :
                    userdata->src.GetMappedRange());
            printf("%s: getMappedRange -> %p%s\n", userdata->functionName,
                    ptr, ptr ? "" : " <------- FAILED");
            assert(ptr != nullptr);
            *ptr = kValue;
            userdata->src.Unmap();

            wgpu::Buffer dst;
            {
                wgpu::BufferDescriptor descriptor{};
                descriptor.size = 4;
                descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
                dst = device.CreateBuffer(&descriptor);
            }

            wgpu::CommandBuffer commands;
            {
                wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
                encoder.CopyBufferToBuffer(userdata->src, userdata->offset, dst, 0, 4);
                commands = encoder.Finish();
            }
            queue.Submit(1, &commands);

            issueContentsCheck(userdata->functionName, dst, kValue);
        }, userdata);
}

// void doRenderTest() {
//     wgpu::Texture readbackTexture;
//     {
//         wgpu::TextureDescriptor descriptor{};
//         descriptor.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;
//         descriptor.size = {1, 1, 1};
//         descriptor.format = wgpu::TextureFormat::BGRA8Unorm;
//         readbackTexture = device.CreateTexture(&descriptor);
//     }
//     wgpu::Texture depthTexture;
//     {
//         wgpu::TextureDescriptor descriptor{};
//         descriptor.usage = wgpu::TextureUsage::RenderAttachment;
//         descriptor.size = {1, 1, 1};
//         descriptor.format = wgpu::TextureFormat::Depth32Float;
//         depthTexture = device.CreateTexture(&descriptor);
//     }
//     render(readbackTexture.CreateView(), depthTexture.CreateView());

//     {
//         wgpu::BufferDescriptor descriptor{};
//         descriptor.size = 4;
//         descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;

//         readbackBuffer = device.CreateBuffer(&descriptor);
//     }

//     wgpu::CommandBuffer commands;
//     {
//         wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
//         wgpu::ImageCopyTexture src{};
//         src.texture = readbackTexture;
//         src.origin = {0, 0, 0};
//         wgpu::ImageCopyBuffer dst{};
//         dst.buffer = readbackBuffer;
//         dst.layout.bytesPerRow = 256;
//         wgpu::Extent3D extent = {1, 1, 1};
//         encoder.CopyTextureToBuffer(&src, &dst, &extent);
//         commands = encoder.Finish();
//     }
//     queue.Submit(1, &commands);

//     // Check the color value encoded in the shader makes it out correctly.
//     static const uint32_t expectData = 0xff0080ff;
//     issueContentsCheck(__FUNCTION__, readbackBuffer, expectData);
// }

wgpu::SwapChain swapChain;
wgpu::TextureView canvasDepthStencilView;
// const uint32_t kWidth = 300;
// const uint32_t kHeight = 150;
const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

void frame() {

    // // buffer data update
    // queue.WriteBuffer(uniformBuffer, 0, objectData.data(), uniformBufferSize);


    // swap chain get texture view

    // wgpu::TextureView backbuffer;
    // {
    //     std::scoped_lock lock(deviceMutex);
    //     backbuffer = swapChain.GetCurrentTextureView();
    // }
    
    wgpu::TextureView backbuffer = swapChain.GetCurrentTextureView();

    wgpu::RenderPassColorAttachment attachment{};
    attachment.view = backbuffer;
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = {0, 0, 0, 1};

    wgpu::RenderPassDescriptor renderpass{};
    renderpass.colorAttachmentCount = 1;
    renderpass.colorAttachments = &attachment;

    wgpu::RenderPassDepthStencilAttachment depthStencilAttachment = {};
    depthStencilAttachment.view = canvasDepthStencilView;
    depthStencilAttachment.depthClearValue = 0;
    depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
    depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;

    renderpass.depthStencilAttachment = &depthStencilAttachment;

#if defined(MULTITHREADED_RENDERING)
    multiThreadedRender(backbuffer, canvasDepthStencilView, renderpass);
#else
    render(backbuffer, canvasDepthStencilView, renderpass);
#endif
    // TODO: Read back from the canvas with drawImage() (or something) and
    // check the result.



#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();

    // exit(0) (rather than emscripten_force_exit(0)) ensures there is no dangling keepalive.
    exit(0);
#else
    // submit_frame
    // wgpu_swap_chain_present();
    swapChain.Present();

    // {
    //     std::scoped_lock lock(deviceMutex);
    //     swapChain.Present();
    // }
#endif

}


struct ThreadArg {
    wgpu::Buffer buffer;
    uint32_t value;

    // std::thread thread;
};



// std::thread style
// void threadFunc(ThreadArg arg) {
void threadFunc(const ThreadArg& arg) {

    // std::lock_guard<std::mutex> lock(deviceMutex);
    std::scoped_lock lock(deviceMutex);

    uint32_t* ptr = static_cast<uint32_t*>(arg.buffer.GetMappedRange());

    *ptr = arg.value;
    arg.buffer.Unmap();

    printf("Printing from Thread value: 0x%.8x\n", arg.value);
}

void doMultithreadingBufferTest() {
    pthread_t threadId;
    printf("Before Thread\n");

    wgpu::BufferDescriptor descriptor{};
    descriptor.size = 4;
    descriptor.usage = wgpu::BufferUsage::MapRead;
    descriptor.mappedAtCreation = true;

    ThreadArg threadArgs[] = {
        ThreadArg{
            device.CreateBuffer(&descriptor),
            0x01020304,
        },
        ThreadArg{
            device.CreateBuffer(&descriptor),
            0x05060708,
        },
    };

    std::vector<std::thread> threads{};
    for (auto& arg : threadArgs) {
        threads.emplace_back(threadFunc, std::cref(arg));
    }
    for (auto& thread : threads) {
        thread.join();
    }

    // for (auto& arg : threadArgs) {
    //     threadFunc(arg);
    // }

    printf("After Thread\n");

    for (auto& arg : threadArgs) {
        issueContentsCheck(__FUNCTION__, arg.buffer, arg.value);
    }
}


#ifndef __EMSCRIPTEN__

static const char* const WINDOW_TITLE = "WebGPU Example";
static const uint32_t WINDOW_WIDTH    = 1280;
static const uint32_t WINDOW_HEIGHT   = 720;

// static void setup_window(wgpu_example_context_t* context,
//                          window_config_t* windows_config)
static void setup_window()
{
  char window_title[] = "WebGPU Native Window";
    // snprintf(window_title,
    //         strlen("WebGPU Native Window - ") + strlen(context->example_title) + 1,
    //         "WebGPU Example - %s", context->example_title);
    window_config_t config = {
        .title     = GET_DEFAULT_IF_ZERO((const char*)window_title, WINDOW_TITLE),
        .width     = kWidth,
        .height    = kHeight,
        // .width     = 800,
        // .height    = 800,
        // .width     = GET_DEFAULT_IF_ZERO(800, WINDOW_WIDTH),
        // .height    = GET_DEFAULT_IF_ZERO(800, WINDOW_HEIGHT),
        // .resizable = true,
        .resizable = false,
    };
    native_window = window_create(&config);
//   window_get_size(context->window, &context->window_size.width,
//                   &context->window_size.height);
//   window_get_aspect_ratio(context->window, &context->window_size.aspect_ratio);

//   memset(&context->callbacks, 0, sizeof(callbacks_t));
//   context->callbacks.mouse_button_callback = mouse_button_callback;
//   context->callbacks.key_callback          = key_callback;
//   context->callbacks.scroll_callback       = scroll_callback;
//   context->callbacks.resize_callback       = resize_callback;

//   input_set_callbacks(context->window, context->callbacks);
}

void wgpu_setup_swap_chain()
{
//   /* Create the swap chain */
//   WGPUSwapChainDescriptor swap_chain_descriptor = {
//     .usage       = WGPUTextureUsage_RenderAttachment,
//     .format      = WGPUTextureFormat_BGRA8Unorm,
//     .width       = kWidth,
//     .height      = kHeight,
//     .presentMode = WGPUPresentMode_Fifo,
//   };
//   if (wgpu_context->swap_chain.instance) {
//     wgpuSwapChainRelease(wgpu_context->swap_chain.instance);
//   }
//   wgpu_context->swap_chain.instance = wgpuDeviceCreateSwapChain(
//     wgpu_context->device, wgpu_context->surface.instance,
//     &swap_chain_descriptor);
//   ASSERT(wgpu_context->swap_chain.instance);

//   /* Find a suitable depth format */
//   wgpu_context->swap_chain.format = swap_chain_descriptor.format;

    wgpu::SwapChainDescriptor scDesc{};
        scDesc.usage = wgpu::TextureUsage::RenderAttachment;
        scDesc.format = wgpu::TextureFormat::BGRA8Unorm;
        scDesc.width = kWidth;
        scDesc.height = kHeight;
        scDesc.presentMode = wgpu::PresentMode::Fifo;
        swapChain = device.CreateSwapChain(surface, &scDesc);

    {
        wgpu::TextureDescriptor descriptor{};
        descriptor.usage = wgpu::TextureUsage::RenderAttachment;
        descriptor.size = {kWidth, kHeight, 1};
        descriptor.format = wgpu::TextureFormat::Depth32Float;
        canvasDepthStencilView = device.CreateTexture(&descriptor).CreateView();
    }
}


// void wgpu_swap_chain_present()
// {
//     swapChain.Present();
// //   wgpuSwapChainPresent(instance->Get());

// //   WGPU_RELEASE_RESOURCE(TextureView, swapChain)
// }


// // static void render_loop(wgpu_example_context_t* context,
// //                         renderfunc_t* render_func,
// //                         onviewchangedfunc_t* view_changed_func,
// //                         onkeypressedfunc_t* example_on_key_pressed_func)
// static void render_loop()
// {
// //   record_t record;
// //   memset(&record, 0, sizeof(record_t));
// //   window_set_userdata(context->window, &record);

// //   float time_start, time_end, time_diff, fps_timer;
// //   record.last_timestamp = platform_get_time();
//   while (!window_should_close(native_window)) {
//     // time_start                      = platform_get_time();
//     // context->frame.timestamp_millis = time_start * 1000.0f;
//     // if (record.view_updated) {
//     //   record.mouse_scrolled = 0;
//     //   record.wheel_delta    = 0;
//     //   record.view_updated   = false;
//     // }
//     // input_poll_events();
//     // update_window_size(context, &record);
//     // render_func(context);
//     // render_func();
//     glfwPollEvents();

//     frame();
// //     ++record.frame_counter;
// //     ++context->frame.index;
// //     time_end             = platform_get_time();
// //     time_diff            = (time_end - time_start) * 1000.0f;
// //     record.frame_timer   = time_diff / 1000.0f;
// //     context->frame_timer = record.frame_timer;
// //     context->run_time += context->frame_timer;
// //     update_camera(context, &record);
// //     update_input_state(context, &record);
// //     if (example_on_key_pressed_func) {
// //       notify_key_input_state(&record, example_on_key_pressed_func);
// //     }
// //     // Convert to clamped timer value
// //     if (!context->paused) {
// //       context->timer += context->timer_speed * record.frame_timer;
// //       if (context->timer >= 1.0) {
// //         context->timer -= 1.0f;
// //       }
// //     }
// //     if (record.view_updated && view_changed_func) {
// //       view_changed_func(context);
// //     }
// //     fps_timer = (time_end - record.last_timestamp) * 1000.0f;
// //     if (fps_timer > 1000.0f) {
// //       record.last_fps   = (float)record.frame_counter * (1000.0f / fps_timer);
// //       context->last_fps = (int)(record.last_fps + 0.5f);
// //       record.frame_counter  = 0;
// //       record.last_timestamp = time_end;
// //     }
// //     context->frame_counter = record.frame_counter;
//   }
// }

#endif


void run() {
    init();


    static constexpr int kNumTests = 1;
    // doMultithreadingBufferTest();

    // static constexpr int kNumTests = 5;
    // doCopyTestMappedAtCreation(false);
    // doCopyTestMappedAtCreation(true);
    // doCopyTestMapAsync(false);
    // doCopyTestMapAsync(true);
    // doRenderTest();

#ifdef __EMSCRIPTEN__
    {
        wgpu::SurfaceDescriptorFromCanvasHTMLSelector canvasDesc{};
        canvasDesc.selector = "#canvas";

        wgpu::SurfaceDescriptor surfDesc{};
        surfDesc.nextInChain = &canvasDesc;
        wgpu::Instance instance{};  // null instance
        wgpu::Surface surface = instance.CreateSurface(&surfDesc);

        wgpu::SwapChainDescriptor scDesc{};
        scDesc.usage = wgpu::TextureUsage::RenderAttachment;
        scDesc.format = wgpu::TextureFormat::BGRA8Unorm;
        scDesc.width = kWidth;
        scDesc.height = kHeight;
        scDesc.presentMode = wgpu::PresentMode::Fifo;
        swapChain = device.CreateSwapChain(surface, &scDesc);

        {
            wgpu::TextureDescriptor descriptor{};
            descriptor.usage = wgpu::TextureUsage::RenderAttachment;
            descriptor.size = {kWidth, kHeight, 1};
            descriptor.format = wgpu::TextureFormat::Depth32Float;
            canvasDepthStencilView = device.CreateTexture(&descriptor).CreateView();
        }
    }
    emscripten_set_main_loop(frame, 0, false);
#else

    setup_window();

    // wgpu_context->surface.instance = window_get_surface(native_window);
    window_get_surface(native_window);
    // window_get_size(native_window, &wgpu_context->surface.width,
    //                 &wgpu_context->surface.height);
    wgpu_setup_swap_chain();

#if defined(MULTITHREADED_RENDERING)
    setupThreads();
#endif
    // render_loop();

    while (!window_should_close(native_window)) {

        glfwPollEvents();

        frame();

        // device.Tick();

        // printf("xxx\n");
    }

    // device.Tick();

    // while (testsCompleted < kNumTests) {
    // // while (true) {
    //     device.Tick();
    // }
#endif

    program_running = false;

#if defined(MULTITHREADED_RENDERING)
    for (ThreadRenderData& d : threadData) {
        d.condition.notify_one();
    }
    for (std::thread& t : renderThreads) {
        t.join();
    }
#endif

}

int main() {
    // GetDevice([](wgpu::Device dev) {
    //     device = dev;
    //     run();
    // });
    

#ifdef __EMSCRIPTEN__
    GetDevice([](wgpu::Device dev) {
        device = dev;
        run();
    });

    // The test result will be reported when the main_loop completes.
    // emscripten_exit_with_live_runtime isn't needed because the WebGPU
    // callbacks should all automatically keep the runtime alive until
    // emscripten_set_main_loop, and that should keep it alive until
    // emscripten_cancel_main_loop.
    //
    // This code is returned when the runtime exits unless something else sets it, like exit(0).
    return 99;
#else
    GetDevice();
    run();
    return 0;
#endif
}
