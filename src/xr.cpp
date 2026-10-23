// SPDX-License-Identifier: Apache-2.0
#include "xr.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <thread>
namespace ob {
namespace {
void check(XrResult result, const char *call) {
  if (XR_FAILED(result))
    throw std::runtime_error(std::string(call) + ": OpenXR error " +
                             std::to_string(result));
}
#define XR(call) check(call, #call)
std::vector<XrExtensionProperties> extensions() {
  uint32_t n = 0;
  XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &n, nullptr));
  std::vector<XrExtensionProperties> p(n, {XR_TYPE_EXTENSION_PROPERTIES});
  XR(xrEnumerateInstanceExtensionProperties(nullptr, n, &n, p.data()));
  return p;
}
bool has(const std::vector<XrExtensionProperties> &list, const char *name) {
  return std::any_of(list.begin(), list.end(),
                     [&](auto &p) { return !strcmp(p.extensionName, name); });
}
XrInstance create(const std::vector<const char *> &ext) {
  XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
  strcpy(info.applicationInfo.applicationName, "OpenBrush Native");
  strcpy(info.applicationInfo.engineName, "No external engine");
  info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
  info.enabledExtensionCount = ext.size();
  info.enabledExtensionNames = ext.data();
  XrInstance result{};
  XR(xrCreateInstance(&info, &result));
  return result;
}
} // namespace
void XrPainter::probe() {
  auto list = extensions();
  auto instance = create({});
  XrInstanceProperties p{XR_TYPE_INSTANCE_PROPERTIES};
  auto result = xrGetInstanceProperties(instance, &p);
  xrDestroyInstance(instance);
  check(result, "xrGetInstanceProperties");
  std::cout << "Runtime: " << p.runtimeName << '\n';
  for (auto name :
       {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME, XR_MNDX_EGL_ENABLE_EXTENSION_NAME,
        XR_EXTX_OVERLAY_EXTENSION_NAME})
    std::cout << name << ": " << (has(list, name) ? "yes" : "no") << '\n';
}
XrPainter::XrPainter(Document &d, History &h, std::function<void()> save,
                     bool overlay)
    : document(d), history(h), changed(std::move(save)) {
  try {
    setup(overlay);
  } catch (...) {
    cleanup();
    throw;
  }
}
XrPainter::~XrPainter() { cleanup(); }
void XrPainter::cleanup() {
  for (auto &e : eyes) {
    glDeleteFramebuffers(1, &e.fbo);
    glDeleteRenderbuffers(1, &e.depth);
    if (e.swapchain)
      xrDestroySwapchain(e.swapchain);
  }
  eyes.clear();
  for (auto &space : handSpaces)
    if (space) {
      xrDestroySpace(space);
      space = XR_NULL_HANDLE;
    }
  if (reference) {
    xrDestroySpace(reference);
    reference = XR_NULL_HANDLE;
  }
  if (session) {
    xrDestroySession(session);
    session = XR_NULL_HANDLE;
  }
  if (actionSet) {
    xrDestroyActionSet(actionSet);
    actionSet = XR_NULL_HANDLE;
  }
  if (instance) {
    xrDestroyInstance(instance);
    instance = XR_NULL_HANDLE;
  }
}
void XrPainter::setup(bool overlay) {
  auto available = extensions();
  std::vector<const char *> ext{XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
                                XR_MNDX_EGL_ENABLE_EXTENSION_NAME};
  for (auto name : ext)
    if (!has(available, name))
      throw std::runtime_error(std::string("Runtime must support ") + name);
  if (overlay) {
    if (!has(available, XR_EXTX_OVERLAY_EXTENSION_NAME))
      throw std::runtime_error("Overlay extension unavailable; --exclusive "
                               "explicitly allows primary XR session mode");
    ext.push_back(XR_EXTX_OVERLAY_EXTENSION_NAME);
  }
  instance = create(ext);
  XrSystemGetInfo get{XR_TYPE_SYSTEM_GET_INFO};
  get.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
  XR(xrGetSystem(instance, &get, &system));
  PFN_xrGetOpenGLGraphicsRequirementsKHR reqFn{};
  XR(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                           reinterpret_cast<PFN_xrVoidFunction *>(&reqFn)));
  XrGraphicsRequirementsOpenGLKHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
  XR(reqFn(instance, system, &req));
  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  auto version = XR_MAKE_VERSION(major, minor, 0);
  if (version < req.minApiVersionSupported ||
      version > req.maxApiVersionSupported)
    throw std::runtime_error("Unsupported OpenGL context version");
  XrSessionCreateInfoOverlayEXTX layer{
      XR_TYPE_SESSION_CREATE_INFO_OVERLAY_EXTX};
  layer.sessionLayersPlacement = 25;
  XrGraphicsBindingEGLMNDX binding{XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};
  binding.next = overlay ? &layer : nullptr;
  binding.display = graphics.display;
  binding.context = graphics.context;
  binding.config = graphics.config;
  binding.getProcAddress =
      reinterpret_cast<PFN_xrEglGetProcAddressMNDX>(eglGetProcAddress);
  XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};
  info.next = &binding;
  info.systemId = system;
  XR(xrCreateSession(instance, &info, &session));
  XrReferenceSpaceCreateInfo ref{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  ref.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
  ref.poseInReferenceSpace.orientation.w = 1;
  XR(xrCreateReferenceSpace(session, &ref, &reference));
  XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
  strcpy(set.actionSetName, "paint");
  strcpy(set.localizedActionSetName, "Painting");
  XR(xrCreateActionSet(instance, &set, &actionSet));
  XR(xrStringToPath(instance, "/user/hand/left", &hands[0]));
  XR(xrStringToPath(instance, "/user/hand/right", &hands[1]));
  auto action = [&](const char *name, XrActionType type, XrAction &handle) {
    XrActionCreateInfo a{XR_TYPE_ACTION_CREATE_INFO};
    strcpy(a.actionName, name);
    strcpy(a.localizedActionName, name);
    a.actionType = type;
    a.countSubactionPaths = 2;
    a.subactionPaths = hands;
    XR(xrCreateAction(actionSet, &a, &handle));
  };
  action("grip_pose", XR_ACTION_TYPE_POSE_INPUT, grip);
  action("paint_pressure", XR_ACTION_TYPE_FLOAT_INPUT, trigger);
  action("undo", XR_ACTION_TYPE_BOOLEAN_INPUT, undo);
  action("redo", XR_ACTION_TYPE_BOOLEAN_INPUT, redo);
  for (auto profile : {"/interaction_profiles/oculus/touch_controller",
                       "/interaction_profiles/valve/index_controller",
                       "/interaction_profiles/htc/vive_controller",
                       "/interaction_profiles/khr/simple_controller"}) {
    std::vector<XrActionSuggestedBinding> bindings;
    bool simple = std::string(profile).find("simple") != std::string::npos;
    auto bind = [&](XrAction handle, const std::string &path) {
      XrPath p;
      XR(xrStringToPath(instance, path.c_str(), &p));
      bindings.push_back({handle, p});
    };
    for (auto hand : {"left", "right"}) {
      std::string base = "/user/hand/" + std::string(hand) + "/input/";
      bind(grip, base + "grip/pose");
      bind(trigger, base + (simple ? "select/click" : "trigger/value"));
    }
    if (std::string(profile).find("touch") != std::string::npos) {
      bind(undo, "/user/hand/right/input/a/click");
      bind(redo, "/user/hand/right/input/b/click");
    }
    XrInteractionProfileSuggestedBinding suggest{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    XR(xrStringToPath(instance, profile, &suggest.interactionProfile));
    suggest.countSuggestedBindings = bindings.size();
    suggest.suggestedBindings = bindings.data();
    auto result = xrSuggestInteractionProfileBindings(instance, &suggest);
    if (XR_FAILED(result))
      std::cerr << "Controller profile not available: " << profile << '\n';
  }
  XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach.countActionSets = 1;
  attach.actionSets = &actionSet;
  XR(xrAttachSessionActionSets(session, &attach));
  for (int i = 0; i < 2; ++i) {
    XrActionSpaceCreateInfo space{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    space.action = grip;
    space.subactionPath = hands[i];
    space.poseInActionSpace.orientation.w = 1;
    XR(xrCreateActionSpace(session, &space, &handSpaces[i]));
  }
  uint32_t n = 0;
  XR(xrEnumerateEnvironmentBlendModes(instance, system,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      0, &n, nullptr));
  std::vector<XrEnvironmentBlendMode> modes(n);
  XR(xrEnumerateEnvironmentBlendModes(instance, system,
                                      XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                      n, &n, modes.data()));
  if (modes.empty())
    throw std::runtime_error("No environment blend modes");
  blend = std::find(modes.begin(), modes.end(),
                    XR_ENVIRONMENT_BLEND_MODE_OPAQUE) != modes.end()
              ? XR_ENVIRONMENT_BLEND_MODE_OPAQUE
              : modes[0];
  XR(xrEnumerateViewConfigurationViews(
      instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n,
      nullptr));
  if (n != 2)
    throw std::runtime_error("Stereo view configuration required");
  std::vector<XrViewConfigurationView> views(n,
                                             {XR_TYPE_VIEW_CONFIGURATION_VIEW});
  XR(xrEnumerateViewConfigurationViews(
      instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n,
      views.data()));
  uint32_t count;
  XR(xrEnumerateSwapchainFormats(session, 0, &count, nullptr));
  std::vector<int64_t> formats(count);
  XR(xrEnumerateSwapchainFormats(session, count, &count, formats.data()));
  int64_t format = std::find(formats.begin(), formats.end(), GL_SRGB8_ALPHA8) !=
                           formats.end()
                       ? GL_SRGB8_ALPHA8
                       : GL_RGBA8;
  if (std::find(formats.begin(), formats.end(), format) == formats.end())
    throw std::runtime_error("RGBA swapchains unavailable");
  eyes.resize(n);
  for (size_t i = 0; i < n; ++i) {
    auto &e = eyes[i];
    e.width = views[i].recommendedImageRectWidth;
    e.height = views[i].recommendedImageRectHeight;
    XrSwapchainCreateInfo sc{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sc.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    sc.format = format;
    sc.sampleCount = 1;
    sc.width = e.width;
    sc.height = e.height;
    sc.faceCount = 1;
    sc.arraySize = 1;
    sc.mipCount = 1;
    XR(xrCreateSwapchain(session, &sc, &e.swapchain));
    XR(xrEnumerateSwapchainImages(e.swapchain, 0, &count, nullptr));
    e.images.resize(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
    XR(xrEnumerateSwapchainImages(
        e.swapchain, count, &count,
        reinterpret_cast<XrSwapchainImageBaseHeader *>(e.images.data())));
    glGenFramebuffers(1, &e.fbo);
    glGenRenderbuffers(1, &e.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, e.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, e.width,
                          e.height);
  }
}
void XrPainter::finishStroke() {
  if (drawing && !active.points.empty()) {
    history.append(std::move(active));
    changed();
  }
  active = Stroke{};
  drawing = false;
}
void XrPainter::input(XrTime time) {
  if (state != XR_SESSION_STATE_FOCUSED) {
    finishStroke();
    erasing = false;
    return;
  }
  XrActiveActionSet activeSet{actionSet, XR_NULL_PATH};
  XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};
  sync.countActiveActionSets = 1;
  sync.activeActionSets = &activeSet;
  XR(xrSyncActions(session, &sync));
  for (auto pair : {std::pair<XrAction, bool>{undo, false}, {redo, true}}) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = pair.first;
    get.subactionPath = hands[1];
    XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};
    XR(xrGetActionStateBoolean(session, &get, &value));
    if (value.isActive && value.changedSinceLastSync && value.currentState) {
      finishStroke();
      if (pair.second ? history.redo() : history.undo())
        changed();
    }
  }
  for (int hand = 0; hand < 2; ++hand) {
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};
    get.action = trigger;
    get.subactionPath = hands[hand];
    XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};
    XR(xrGetActionStateFloat(session, &get, &value));
    XrSpaceLocation pose{XR_TYPE_SPACE_LOCATION};
    XR(xrLocateSpace(handSpaces[hand], reference, time, &pose));
    const auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                       XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    bool down = value.isActive && value.currentState > .08f &&
                (pose.locationFlags & valid) == valid;
    if (hand == 1) {
      if (!down) {
        finishStroke();
        continue;
      }
      if (!drawing) {
        active = Stroke{};
        active.color = {.15f, .65f, 1, 1};
        drawing = true;
      }
      Point p{
          {pose.pose.position.x, pose.pose.position.y, pose.pose.position.z},
          {pose.pose.orientation.x, pose.pose.orientation.y,
           pose.pose.orientation.z, pose.pose.orientation.w},
          value.currentState,
          uint32_t(time / 1000000)};
      if (active.points.empty() ||
          length(active.points.back().position - p.position) > .001f)
        active.points.push_back(p);
    } else if (down) {
      finishStroke();
      Vec3 p{pose.pose.position.x, pose.pose.position.y, pose.pose.position.z};
      if (history.eraseNear(p, .025f))
        changed();
    }
  }
}
void XrPainter::events() {
  XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
  XrResult result;
  while ((result = xrPollEvent(instance, &event)) == XR_SUCCESS) {
    if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
      state = reinterpret_cast<XrEventDataSessionStateChanged &>(event).state;
      if (state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
        begin.primaryViewConfigurationType =
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XR(xrBeginSession(session, &begin));
        running = true;
      } else if (state == XR_SESSION_STATE_STOPPING) {
        finishStroke();
        XR(xrEndSession(session));
        running = false;
      } else if (state == XR_SESSION_STATE_EXITING ||
                 state == XR_SESSION_STATE_LOSS_PENDING)
        quit = true;
    } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING)
      quit = true;
    event = {XR_TYPE_EVENT_DATA_BUFFER};
  }
  if (result != XR_EVENT_UNAVAILABLE)
    check(result, "xrPollEvent");
}
void XrPainter::run(unsigned frameLimit) {
  unsigned frames = 0, rendered = 0;
  while (!quit) {
    events();
    if (!running) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frame{XR_TYPE_FRAME_STATE};
    XR(xrWaitFrame(session, &wait, &frame));
    XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
    XR(xrBeginFrame(session, &begin));
    input(frame.predictedDisplayTime);
    std::vector<XrCompositionLayerProjectionView> layers(
        eyes.size(), {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space = reference;
    layer.viewCount = layers.size();
    layer.views = layers.data();
    bool submit = false;
    if (frame.shouldRender) {
      XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};
      locate.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
      locate.displayTime = frame.predictedDisplayTime;
      locate.space = reference;
      XrViewState viewState{XR_TYPE_VIEW_STATE};
      std::vector<XrView> views(eyes.size(), {XR_TYPE_VIEW});
      uint32_t n;
      XR(xrLocateViews(session, &locate, &viewState, views.size(), &n,
                       views.data()));
      auto flags = XR_VIEW_STATE_POSITION_VALID_BIT |
                   XR_VIEW_STATE_ORIENTATION_VALID_BIT;
      if (n == eyes.size() && (viewState.viewStateFlags & flags) == flags) {
        submit = true;
        for (size_t i = 0; i < eyes.size(); ++i) {
          auto &e = eyes[i];
          uint32_t index;
          XrSwapchainImageAcquireInfo acq{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
          XR(xrAcquireSwapchainImage(e.swapchain, &acq, &index));
          XrSwapchainImageWaitInfo iw{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
          iw.timeout = XR_INFINITE_DURATION;
          XR(xrWaitSwapchainImage(e.swapchain, &iw));
          glBindFramebuffer(GL_FRAMEBUFFER, e.fbo);
          glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                 GL_TEXTURE_2D, e.images[index].image, 0);
          glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                    GL_RENDERBUFFER, e.depth);
          if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
              GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("XR framebuffer incomplete");
          auto &p = views[i].pose;
          auto &f = views[i].fov;
          auto vp = multiply(
              projection(f.angleLeft, f.angleRight, f.angleDown, f.angleUp),
              viewMatrix({p.position.x, p.position.y, p.position.z},
                         {p.orientation.x, p.orientation.y, p.orientation.z,
                          p.orientation.w}));
          renderer.draw(document, drawing ? &active : nullptr, vp, e.width,
                        e.height);
          glFlush();
          XrSwapchainImageReleaseInfo release{
              XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
          XR(xrReleaseSwapchainImage(e.swapchain, &release));
          layers[i].pose = p;
          layers[i].fov = f;
          layers[i].subImage.swapchain = e.swapchain;
          layers[i].subImage.imageRect.extent = {e.width, e.height};
        }
      }
    }
    const XrCompositionLayerBaseHeader *base =
        reinterpret_cast<const XrCompositionLayerBaseHeader *>(&layer);
    XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
    end.displayTime = frame.predictedDisplayTime;
    end.environmentBlendMode = blend;
    end.layerCount = submit ? 1 : 0;
    end.layers = submit ? &base : nullptr;
    XR(xrEndFrame(session, &end));
    if (submit)
      ++rendered;
    ++frames;
    if (frameLimit && frames >= frameLimit)
      break;
  }
  finishStroke();
  std::cout << "XR frames=" << frames << " stereo_submissions=" << rendered
            << "\n";
}
} // namespace ob
