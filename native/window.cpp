#include "window.h"

#if defined(__linux__)
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#endif

// #include <GLFW/glfw3.h>
// #include "GLFW/glfw3.h"
#include "third_party/glfw3/include/GLFW/glfw3.h"

#if defined(WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#endif
// #include "GLFW/glfw3native.h"
// #include "GLFW/glfw3native.h"
#include "third_party/glfw3/include/GLFW/glfw3native.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "macro.h"
// #include <webgpu_cpp.h>
#include <dawn/webgpu_cpp.h>
// #include "dawn/webgpu_cpp.h"

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

// struct window {
//   GLFWwindow* handle;
//   struct {
//     WGPUSurface handle;
//     uint32_t width, height;
//     float dpscale;
//   } surface;
//   callbacks_t callbacks;
//   int intialized;
//   /* common data */
//   float mouse_scroll_scale_factor;
//   void* userdata;
// };

// static std::unique_ptr<wgpu::ChainedStruct> SurfaceDescriptor(void* display,
//                                                               void* window)
// {
// #if defined(WIN32)
//   std::unique_ptr<wgpu::SurfaceDescriptorFromWindowsHWND> desc
//     = std::make_unique<wgpu::SurfaceDescriptorFromWindowsHWND>();
//   desc->hwnd      = window;
//   desc->hinstance = GetModuleHandle(nullptr);
//   return std::move(desc);
// #elif defined(__linux__) // X11
//   std::unique_ptr<wgpu::SurfaceDescriptorFromXlibWindow> desc
//     = std::make_unique<wgpu::SurfaceDescriptorFromXlibWindow>();
//   desc->display = display;
//   desc->window  = *((uint32_t*)window);
//   return std::move(desc);
// #endif

//   return nullptr;
// }

// WGPUSurface CreateSurface(dawn::native::Instance* instance, void* display, void* window)
// {
//   std::unique_ptr<wgpu::ChainedStruct> sd = SurfaceDescriptor(display, window);
//   wgpu::SurfaceDescriptor descriptor;
//   descriptor.nextInChain = sd.get();
//   // wgpu::Surface surface = wgpu::Instance(gpuContext.dawn_native.instance->Get())
//   //                           .CreateSurface(&descriptor);
//   wgpu::Surface surface = wgpu::Instance(instance->Get())
//                             .CreateSurface(&descriptor);
//   if (!surface) {
//     return nullptr;
//   }
//   WGPUSurface surf = surface.Get();
//   wgpuSurfaceReference(surf);
//   return surf;
// }

// /* Function prototypes */
// static void surface_update_framebuffer_size(window_t* window);
// static void glfw_window_error_callback(int error, const char* description);
// static void glfw_window_key_callback(GLFWwindow* src_window, int key,
//                                      int scancode, int action, int mods);
// static void glfw_window_cursor_position_callback(GLFWwindow* src_window,
//                                                  double xpos, double ypos);
// static void glfw_window_mouse_button_callback(GLFWwindow* src_window,
//                                               int button, int action, int mods);
// static void glfw_window_scroll_callback(GLFWwindow* src_window, double xoffset,
//                                         double yoffset);
// static void glfw_window_size_callback(GLFWwindow* src_window, int width,
//                                       int height);

// window_t* window_create(window_config_t* config)
// {
//   if (!config) {
//     return NULL;
//   }

//   window_t* window = (window_t*)malloc(sizeof(window_t));
//   memset(window, 0, sizeof(window_t));
//   window->mouse_scroll_scale_factor = 1.0f;

//   /* Initialize error handling */
//   glfwSetErrorCallback(glfw_window_error_callback);

//   /* Initialize the library */
//   if (!glfwInit()) {
//     /* Handle initialization failure */
//     fprintf(stderr, "Failed to initialize GLFW\n");
//     fflush(stderr);
//     return window;
//   }

//   glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//   glfwWindowHint(GLFW_RESIZABLE, config->resizable ? GLFW_TRUE : GLFW_FALSE);

//   /* Create GLFW window */
//   window->handle = glfwCreateWindow(config->width, config->height,
//                                     config->title, NULL, NULL);

//   /* Confirm that GLFW window was created successfully */
//   if (!window->handle) {
//     glfwTerminate();
//     fprintf(stderr, "Failed to create window\n");
//     fflush(stderr);
//     return window;
//   }

//   surface_update_framebuffer_size(window);

//   /* Set user pointer to window class */
//   glfwSetWindowUserPointer(window->handle, (void*)window);

//   /* -- Setup callbacks -- */
//   /* clang-format off */
//   /* Key input events */
//   glfwSetKeyCallback(window->handle, glfw_window_key_callback);
//   /* Cursor position */
//   glfwSetCursorPosCallback(window->handle, glfw_window_cursor_position_callback);
//   /* Mouse button input */
//   glfwSetMouseButtonCallback(window->handle, glfw_window_mouse_button_callback);
//   /* Scroll input */
//   glfwSetScrollCallback(window->handle, glfw_window_scroll_callback);
//   /* Window resize events */
//   glfwSetWindowSizeCallback(window->handle, glfw_window_size_callback);
//   /* clang-format on */

//   /* Change the state of the window to intialized */
//   window->intialized = 1;

//   return window;
// }

// void window_destroy(window_t* window)
// {
//   /* Cleanup window(s) */
//   if (window) {
//     if (window->handle) {
//       glfwDestroyWindow(window->handle);
//       window->handle = NULL;

//       /* Terminate GLFW */
//       glfwTerminate();
//     }

//     /* Free allocated memory */
//     free(window);
//     window = NULL;
//   }
// }

// int window_should_close(window_t* window)
// {
//   return glfwWindowShouldClose(window->handle);
// }

// void window_set_title(window_t* window, const char* title)
// {
//   glfwSetWindowTitle(window->handle, title);
// }

// void window_set_userdata(window_t* window, void* userdata)
// {
//   window->userdata = userdata;
// }

// void* window_get_userdata(window_t* window)
// {
//   return window->userdata;
// }

// void* window_get_surface(dawn::native::Instance* instance, window_t* window)
// {
// #if defined(WIN32)
//   void* display         = NULL;
//   uint32_t windowHandle = glfwGetWin32Window(window->handle);
// #elif defined(__linux__) /* X11 */
//   void* display         = glfwGetX11Display();
//   uint32_t windowHandle = glfwGetX11Window(window->handle);
// #endif
//   // window->surface.handle = wgpu_create_surface(display, &windowHandle);
//   window->surface.handle = CreateSurface(instance, display, &windowHandle);

//   return window->surface.handle;
// }

// void window_get_size(window_t* window, uint32_t* width, uint32_t* height)
// {
//   *width  = window->surface.width;
//   *height = window->surface.height;
// }

// void window_get_aspect_ratio(window_t* window, float* aspect_ratio)
// {
//   *aspect_ratio = (float)window->surface.width / (float)window->surface.height;
// }