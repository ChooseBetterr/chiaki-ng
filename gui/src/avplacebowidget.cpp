// SPDX-License-Identifier: LicenseRef-AGPL-3.0-only-OpenSSL

#include <avplacebowidget.h>
#include <avplaceboframeuploader.h>
#include <streamsession.h>

#include <QThread>
#include <QTimer>
#include <QGuiApplication>
#include <QWindow>
#include <stdio.h>

#define PL_LIBAV_IMPLEMENTATION 0
#include <libplacebo/utils/libav.h>

#include <xcb/xcb.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>
#include <qpa/qplatformnativeinterface.h>

AVPlaceboWidget::AVPlaceboWidget(
    StreamSession *session, pl_log placebo_log, pl_vk_inst placebo_vk_inst,
    ResolutionMode resolution_mode, PlaceboPreset preset)
    : placebo_log(placebo_log), placebo_vk_inst(placebo_vk_inst), session(session), resolution_mode(resolution_mode)
{
    setSurfaceType(QWindow::VulkanSurface);

    if (preset == PlaceboPreset::Default)
    {
        CHIAKI_LOGI(session->GetChiakiLog(), "Using placebo default preset");
        render_params = pl_render_default_params;
    }
    else if (preset == PlaceboPreset::Fast)
    {
        CHIAKI_LOGI(session->GetChiakiLog(), "Using placebo fast preset");
        render_params = pl_render_fast_params;
    }
    else
    {
        CHIAKI_LOGI(session->GetChiakiLog(), "Using placebo high quality preset");
        render_params = pl_render_high_quality_params;
    }
}

AVPlaceboWidget::~AVPlaceboWidget()
{
    for (int i = 0; i < 4; i++) {
        if (placebo_tex[i])
            pl_tex_destroy(placebo_vulkan->gpu, &placebo_tex[i]);
    }

    pl_renderer_destroy(&placebo_renderer);
    pl_swapchain_destroy(&placebo_swapchain);

    PFN_vkDestroySurfaceKHR destroySurface = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
            placebo_vk_inst->get_proc_addr(placebo_vk_inst->instance, "vkDestroySurfaceKHR"));
    destroySurface(placebo_vk_inst->instance, surface, nullptr);
}

void AVPlaceboWidget::showEvent(QShowEvent *event) {
    QWindow::showEvent(event);
    this->requestActivate();

    VkResult err;

    if (QGuiApplication::platformName().startsWith("wayland")) {
        PFN_vkCreateWaylandSurfaceKHR createSurface = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(
                placebo_vk_inst->get_proc_addr(placebo_vk_inst->instance, "vkCreateWaylandSurfaceKHR"));

        VkWaylandSurfaceCreateInfoKHR surfaceInfo;
        memset(&surfaceInfo, 0, sizeof(surfaceInfo));
        surfaceInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        surfaceInfo.display = static_cast<struct wl_display*>(pni->nativeResourceForWindow("display", this));
        surfaceInfo.surface = static_cast<struct wl_surface*>(pni->nativeResourceForWindow("surface", this));
        err = createSurface(placebo_vk_inst->instance, &surfaceInfo, nullptr, &surface);
    } else if (QGuiApplication::platformName().startsWith("xcb")) {
        PFN_vkCreateXcbSurfaceKHR createSurface = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(
                placebo_vk_inst->get_proc_addr(placebo_vk_inst->instance, "vkCreateXcbSurfaceKHR"));

        VkXcbSurfaceCreateInfoKHR surfaceInfo;
        memset(&surfaceInfo, 0, sizeof(surfaceInfo));
        surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        QPlatformNativeInterface* pni = QGuiApplication::platformNativeInterface();
        surfaceInfo.connection = static_cast<xcb_connection_t*>(pni->nativeResourceForWindow("connection", this));
        surfaceInfo.window = static_cast<xcb_window_t>(winId());
        err = createSurface(placebo_vk_inst->instance, &surfaceInfo, nullptr, &surface);
    } else {
        Q_UNREACHABLE();
    }

    if (err != VK_SUCCESS) {
        qFatal("Failed to create VkSurfaceKHR");
    }

    struct pl_vulkan_swapchain_params swapchain_params = {
        .surface = surface,
        .present_mode = VK_PRESENT_MODE_FIFO_KHR,
    };
    placebo_swapchain = pl_vulkan_create_swapchain(placebo_vulkan, &swapchain_params);

    int initialWidth = this->width() * this->devicePixelRatio();
    int initialHeight = this->height() * this->devicePixelRatio();
    pl_swapchain_resize(placebo_swapchain, &initialWidth, &initialHeight);

    placebo_renderer = pl_renderer_create(
        placebo_log,
        placebo_vulkan->gpu
    );

    frame_uploader = new AVPlaceboFrameUploader(session, this);
    frame_uploader_thread = new QThread(this);
    frame_uploader_thread->setObjectName("Frame Uploader");
    frame_uploader->moveToThread(frame_uploader_thread);
    frame_uploader_thread->start();
}

void AVPlaceboWidget::Stop() {
    if (frame_uploader_thread) {
        frame_uploader_thread->quit();
        frame_uploader_thread->wait();
        delete frame_uploader_thread;
        frame_uploader_thread = nullptr;
    }
    delete frame_uploader;
    frame_uploader = nullptr;
}

bool AVPlaceboWidget::RenderFrame(AVFrame *frame) {
    struct pl_swapchain_frame sw_frame = {0};
    struct pl_frame placebo_frame = {0};
    struct pl_frame target_frame = {0};

    struct pl_avframe_params avparams = {
        .frame = frame,
        .tex = placebo_tex,
        .map_dovi = false,
    };

    if (!pl_map_avframe_ex(placebo_vulkan->gpu, &placebo_frame, &avparams)) {
        CHIAKI_LOGE(session->GetChiakiLog(), "Failed to map AVFrame to Placebo frame!");
        return false;
    }
    // set colorspace hint
    struct pl_color_space hint = placebo_frame.color;
    pl_swapchain_colorspace_hint(placebo_swapchain, &hint);

    pl_rect2df crop;

    bool ret = false;
    if (!pl_swapchain_start_frame(placebo_swapchain, &sw_frame)) {
        CHIAKI_LOGE(session->GetChiakiLog(), "Failed to start Placebo frame!");
        goto cleanup;
    }

    pl_frame_from_swapchain(&target_frame, &sw_frame);

    crop = placebo_frame.crop;
    switch (resolution_mode) {
        case ResolutionMode::Normal:
            pl_rect2df_aspect_copy(&target_frame.crop, &crop, 0.0);
            break;
        case ResolutionMode::Stretch:
            // Nothing to do, target.crop already covers the full image
            break;
        case ResolutionMode::Zoom:
            pl_rect2df_aspect_copy(&target_frame.crop, &crop, 1.0);
            break;
    }

    if (!pl_render_image(placebo_renderer, &placebo_frame, &target_frame,
                         &render_params)) {
        CHIAKI_LOGE(session->GetChiakiLog(), "Failed to render Placebo frame!");
        goto cleanup;
    }

    if (!pl_swapchain_submit_frame(placebo_swapchain)) {
        CHIAKI_LOGE(session->GetChiakiLog(), "Failed to submit Placebo frame!");
        goto cleanup;
    }
    pl_swapchain_swap_buffers(placebo_swapchain);
    ret = true;

cleanup:
    pl_unmap_avframe(placebo_vulkan->gpu, &placebo_frame);
    return ret;
}

void AVPlaceboWidget::resizeEvent(QResizeEvent *event)
{
    QWindow::resizeEvent(event);

    int width = event->size().width() * this->devicePixelRatio();
    int height = event->size().height() * this->devicePixelRatio();
    pl_swapchain_resize(placebo_swapchain, &width, &height);
}

void AVPlaceboWidget::ToggleZoom()
{
	if( resolution_mode == Zoom )
		resolution_mode = Normal;
	else
		resolution_mode = Zoom;
}

void AVPlaceboWidget::ToggleStretch()
{
	if( resolution_mode == Stretch )
		resolution_mode = Normal;
	else
		resolution_mode = Stretch;
}

void AVPlaceboWidget::PlaceboLog(void *user, pl_log_level level, const char *msg)
{
    ChiakiLog *log = (ChiakiLog*)user;
    if (!log) {
        return;
    }

    ChiakiLogLevel chiaki_level;
    switch (level)
    {
        case PL_LOG_DEBUG:
            chiaki_level = CHIAKI_LOG_VERBOSE;
            break;
        case PL_LOG_INFO:
            chiaki_level = CHIAKI_LOG_INFO;
            break;
        case PL_LOG_WARN:
            chiaki_level = CHIAKI_LOG_WARNING;
            break;
        case PL_LOG_ERR:
        case PL_LOG_FATAL:
            chiaki_level = CHIAKI_LOG_ERROR;
            break;
    }

    chiaki_log(log, chiaki_level, "[libplacebo] %s", msg);
}
