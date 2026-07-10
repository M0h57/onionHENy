/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include "HookedFuncs.hpp"
#include "external_symbols.hpp"
#include "ipc.hpp"
#include <cstring>

extern MonoObject* Game;
extern MonoObject* rootWidget;
extern MonoObject* font;
extern OverlayLayout g_overlay_layout;
extern orion::Settings g_settings;
extern bool g_all_cpu_usage;
MonoObject* CreateLabel(const char* name, float x, float y, const char* text, MonoObject* fontObj, int horzAlign, int vertAlign, float r, float g, float b, float a);
void Widget_Append_Child(MonoObject* widget, MonoObject* child);
MonoObject* CreateUIFont(int size, int style, int weight);
int get_ip_address(char* ip_address);

void RemoveGameWidget(RemoveWidget widget) {

    // Helper lambda to remove widgets by name
    auto removeWidgets = [](const std::vector<const char*>& widgetNames) {
        MonoClass* widgetClass = mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget");
        MonoObject* rootWidget = Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget");
        for (const char* name : widgetNames) {
            MonoObject* child = Invoke<MonoObject*>(pui_img, widgetClass, rootWidget, "FindWidgetByName", mono_string_new(Root_Domain, name));
            if (child) {
                Invoke<void>(pui_img, widgetClass, child, "RemoveFromParent");
            }
        }
    };

    switch (widget) {
    case REMOVE_GPU_OVERLAY:
        removeWidgets({ "id_gpu_temp_value", "id_gpu_usage_value", "id_gpu_label" });
        break;
    case REMOVE_CPU_OVERLAY:
        removeWidgets({ "id_cpu_label", "id_cpu_temp_value", "id_cpu_usage_value" });
        break;
    case REMOVE_RAM_OVERLAY:
        removeWidgets({ "id_ram_label", "id_ram_value" });
        break;
    case REMOVE_FPS_OVERLAY:
        removeWidgets({ "id_fps_label", "id_fps_value" });
        break;
    case REMOVE_IP_OVERLAY:
		removeWidgets({ "id_ip_label", "id_ip_value" });
		break;
    case REMOVE_ALL_OVERLAYS:
        removeWidgets({ "id_gpu_temp_value", "id_gpu_usage_value", "id_gpu_label",
                        "id_cpu_label", "id_cpu_temp_value", "id_cpu_usage_value",
                        "id_ram_label", "id_ram_value",
                        "id_fps_label", "id_fps_value", 
                        "id_ip_label", "id_ip_value" });
		break;
    }
}

void CreateGameWidget(CreateWidget widget) {
    MonoObject* font = CreateUIFont(22, 0, 0);
    MonoObject* rootWidget = Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget");

    std::vector<WidgetConfig> configs;

    switch (widget) {
    case CREATE_GPU_OVERLAY:
        configs = {
            {"id_gpu_label", g_overlay_layout.overlay_gpu_x, g_overlay_layout.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", g_overlay_layout.overlay_gpu_x + 70.0f, g_overlay_layout.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", g_overlay_layout.overlay_gpu_x + 115.0f, g_overlay_layout.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_CPU_OVERLAY:
        configs = {
            {"id_cpu_label", g_overlay_layout.overlay_cpu_x, g_overlay_layout.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", g_overlay_layout.overlay_cpu_x + 70.0f, g_overlay_layout.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", g_overlay_layout.overlay_cpu_x + 115.0f, g_overlay_layout.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f}  // Orange
        };
        break;

    case CREATE_RAM_OVERLAY:
        configs = {
            {"id_ram_label", g_overlay_layout.overlay_ram_x, g_overlay_layout.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", g_overlay_layout.overlay_ram_x + 70.0f, g_overlay_layout.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f}    // Orange
        };
        break;

    case CREATE_FPS_OVERLAY:
        configs = {
            {"id_fps_label", g_overlay_layout.overlay_fps_x, g_overlay_layout.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", g_overlay_layout.overlay_fps_x + 70.0f, g_overlay_layout.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f}     // White
        };
        break;
    case CREATE_IP_OVERLAY:
		configs = {
           { "id_ip_label", g_overlay_layout.overlay_ip_x, g_overlay_layout.overlay_ip_y, "PS5 IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f},       // Green + Bold
		   { "id_ip_value", g_overlay_layout.overlay_ip_x + 70.0f, g_overlay_layout.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
	     };
	     break;
    case CREATE_ALL_OVERLAYS:
        configs = {
            // GPU Overlay
            {"id_gpu_label", g_overlay_layout.overlay_gpu_x, g_overlay_layout.overlay_gpu_y, "GPU", 1, 0.0f, 1.0f, 0.0f, 1.0f},        // Green + Bold
            {"id_gpu_temp_value", g_overlay_layout.overlay_gpu_x + 70.0f, g_overlay_layout.overlay_gpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_gpu_usage_value", g_overlay_layout.overlay_gpu_x + 115.0f, g_overlay_layout.overlay_gpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // CPU Overlay
            {"id_cpu_label", g_overlay_layout.overlay_cpu_x, g_overlay_layout.overlay_cpu_y, "CPU", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_cpu_temp_value", g_overlay_layout.overlay_cpu_x + 70.0f, g_overlay_layout.overlay_cpu_y, "--C", 0, 1.0f, 0.6f, 0.0f, 1.0f},   // Orange
            {"id_cpu_usage_value", g_overlay_layout.overlay_cpu_x + 115.0f, g_overlay_layout.overlay_cpu_y, "--%", 0, 1.0f, 0.6f, 0.0f, 1.0f},  // Orange
            // RAM Overlay
            {"id_ram_label", g_overlay_layout.overlay_ram_x, g_overlay_layout.overlay_ram_y, "RAM", 1, 0.0f, 1.0f, 1.0f, 1.0f},        // Cyan + Bold
            {"id_ram_value", g_overlay_layout.overlay_ram_x + 70.0f, g_overlay_layout.overlay_ram_y, "----- MB", 0, 1.0f, 0.6f, 0.0f, 1.0f},    // Orange
            // FPS Overlay
			{"id_fps_label", g_overlay_layout.overlay_fps_x, g_overlay_layout.overlay_fps_y, "FPS:", 1, 1.0f, 0.0f, 1.0f, 1.0f},       // Magenta + Bold
            {"id_fps_value", g_overlay_layout.overlay_fps_x + 70.0f, g_overlay_layout.overlay_fps_y, "--- FPS", 0, 1.0f, 1.0f, 1.0f, 1.0f},     // White

            { "id_ip_label", g_overlay_layout.overlay_ip_x, g_overlay_layout.overlay_ip_y, "IP:", 1, 0.0f, 1.0f, 0.0f, 1.0f },       // Green + Bold
            { "id_ip_value", g_overlay_layout.overlay_ip_x + 70.0f, g_overlay_layout.overlay_ip_y, "---.---.---.---", 0, 1.0f, 1.0f, 1.0f, 1.0f }     // White
		};
        break;
}



    // Create and append all widgets
    for (const auto& config : configs) {
        MonoObject* label = CreateLabel(config.id, config.x, config.y, config.text, font,
            config.bold, 0, config.r, config.g, config.b, config.a);
        Widget_Append_Child(rootWidget, label);
    }
}

