/* Copyright (C) 2025 OrionHEN / LightningMods — P0 split. */


#include "HookedFuncs.hpp"
#include "ipc.hpp"
#include "external_symbols.hpp"
#include "Detour.h"
#include <orion/settings.hpp>
#include <cstring>
#include <cstdio>

extern void (*OnRender_orig)(MonoObject* instance);
extern MonoObject* rootWidget;
extern MonoObject* font;
extern OverlayLayout g_overlay_layout;
extern orion::Settings g_settings;
extern bool g_all_cpu_usage;
void RemoveGameWidget(RemoveWidget widget);
void CreateGameWidget(CreateWidget widget);
MonoObject* CreateLabel(const char* name, float x, float y, const char* text, MonoObject* fontObj, int horzAlign, int vertAlign, float r, float g, float b, float a);
void Widget_Append_Child(MonoObject* widget, MonoObject* child);
MonoObject* CreateUIFont(int size, int style, int weight);
int get_ip_address(char* ip_address);

struct OrbisKernelTimespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct Proc_Stats
{
    int32_t lo_data;								//0x00
    uint32_t td_tid;						//0x04
    OrbisKernelTimespec user_cpu_usage_time;	//0x08
    OrbisKernelTimespec system_cpu_usage_time;  //0x18
}; //0x28

extern "C" {
    int sceKernelGetSocSensorTemperature(int sensorId, int* soctime);
    int get_page_table_stats(int vm, int type, int* total, int* free);
    int sceKernelGetCpuUsage(struct Proc_Stats* out, int32_t* size);
    int sceKernelGetThreadName(uint32_t id, char* out);
	int sceKernelGetCpuTemperature(int* cputemp);
    int sceKernelClockGettime(int clockId, OrbisKernelTimespec* tp);
}

struct Memory
{
    int Used;
    int Free;
    int Total;
    float Percentage;
};

struct thread_usages
{
    OrbisKernelTimespec current_time;	//0x00
    int Thread_Count;					//0x10
    char padding0[0x4];					//0x14
    Proc_Stats Threads[3072];			//0x18
};

int Thread_Count = 0;
float Usage[8] = { 0 };
float Average_Usage;
Memory RAM;
Memory VRAM;

Proc_Stats Stat_Data[3072];
thread_usages gThread_Data[2];


extern "C" int sceLncUtilKillAppWithReason(int appId, int reason);

int KillAppWithReason_Hook(int appId, int reason)
{
    return sceLncUtilKillAppWithReason(appId, reason);
}

void Get_Page_Table_Stats(int vm, int type, int* Used, int* Free, int* Total)
{
    int _Total = 0, _Free = 0;

    if (get_page_table_stats(vm, type, &_Total, &_Free) == -1) {
        shellui_log("get_page_table_stats() Failed.\n");
        return;
    }

    if (Used)
        *Used = (_Total - _Free);

    if (Free)
        *Free = _Free;

    if (Total)
        *Total = _Total;
}

void calc_usage(unsigned int idle_tid[8], thread_usages* cur, thread_usages* prev, float usage_out[8])
{
    if (cur->Thread_Count <= 0 || prev->Thread_Count <= 0) //Make sure our banks have threads
        return;

    //Calculate the Current time difference from the last bank to the current bank.
    float Current_Time_Total = ((prev->current_time.tv_sec + (prev->current_time.tv_nsec / 1000000000.0f)) - (cur->current_time.tv_sec + (cur->current_time.tv_nsec / 1000000000.0f)));

    //Here this could use to be improved but essetially what its doing is finding the thread information for the idle threads using their thread Index stored from before.
    struct Data_s
    {
        Proc_Stats* Cur;
        Proc_Stats* Prev;
    }Data[8];

    for (int i = 0; i < cur->Thread_Count; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (idle_tid[j] == cur->Threads[i].td_tid)
                Data[j].Cur = &cur->Threads[i];
        }
    }

    for (int i = 0; i < prev->Thread_Count; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            if (idle_tid[j] == prev->Threads[i].td_tid)
                Data[j].Prev = &prev->Threads[i];
        }
    }

    //Here we loop through each core to calculate the total usage time as its split into user/sustem
    for (int i = 0; i < 8; i++)
    {
        float Prev_Usage_Time = (Data[i].Prev->system_cpu_usage_time.tv_sec + (Data[i].Prev->system_cpu_usage_time.tv_nsec / 1000000.0f));
        Prev_Usage_Time += (Data[i].Prev->user_cpu_usage_time.tv_sec + (Data[i].Prev->user_cpu_usage_time.tv_nsec / 1000000.0f));

        float Cur_Usage_Time = (Data[i].Cur->system_cpu_usage_time.tv_sec + (Data[i].Cur->system_cpu_usage_time.tv_nsec / 1000000.0f));
        Cur_Usage_Time += (Data[i].Cur->user_cpu_usage_time.tv_sec + (Data[i].Cur->user_cpu_usage_time.tv_nsec / 1000000.0f));

        //We calculate the usage using usage time difference between the two samples divided by the current time difference.
        float Idle_Usage = ((Prev_Usage_Time - Cur_Usage_Time) / Current_Time_Total);

        if (Idle_Usage > 1.0f)
            Idle_Usage = 1.0f;

        if (Idle_Usage < 0.0f)
            Idle_Usage = 0.0f;

        //Get inverse of idle percentage and express in percent.
        usage_out[i] = (1.0f - Idle_Usage) * 100.0f;
    }
}
extern bool app_launched;


class AtomicString {
    mutable std::mutex mtx;
    std::string value;

public:
    void store(const std::string& str) {
        std::lock_guard<std::mutex> lock(mtx);
        value = str;
    }

    std::string load() const {
        std::lock_guard<std::mutex> lock(mtx);
        return value;
    }
};

void* search_bytes(const void* haystack, size_t haystack_len,
    const void* needle, size_t needle_len){

    if (needle_len == 0 || needle_len > haystack_len) {
        return NULL;

    }

    const unsigned char* h = (const unsigned char*)haystack;
    const unsigned char* n = (const unsigned char*)needle;


    for (size_t i = 0; i <= haystack_len - needle_len; i++) {
        if (memcmp(&h[i], n, needle_len) == 0) {
            return (void*)&h[i];

        }
    }

    return NULL;

}
void ShellHexDump(const void* data, size_t size) {
    const unsigned char* byteData = static_cast<const unsigned char*>(data);
    char line[256];
    
    for (size_t i = 0; i < size; i += 16) {
        int pos = 0;
        
        // Offset
        pos += snprintf(line + pos, sizeof(line) - pos, "%08zx  ", i);
        
        // Hex bytes
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                pos += snprintf(line + pos, sizeof(line) - pos, "%02x ", 
                                byteData[i + j]);
            } else {
                pos += snprintf(line + pos, sizeof(line) - pos, "   ");
            }
        }
        
        pos += snprintf(line + pos, sizeof(line) - pos, " ");
        
        // ASCII representation
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size) {
                unsigned char c = byteData[i + j];
                pos += snprintf(line + pos, sizeof(line) - pos, "%c", 
                                isprint(c) ? c : '.');
            }
        }
        
        shellui_log(line);
    }
}
AtomicString fps_string;
ssize_t(*read_orig)(int fd, void *buf, size_t count) = nullptr;
ssize_t read_hook(int fd, void* buf, size_t count) {
    ssize_t ret = read_orig(fd, buf, count);
   // shellui_log("read_hook called: fd=%d, count=%zu, ret=%zd", fd, count, ret);
    if (count == 65536) {
        void* found = search_bytes(buf, 100, "FPS", 3);
        if (found) {
            const char* fps_ptr = (const char*)found;

            // Skip "FPS" and any separators (: = space etc)
            fps_ptr += 3; // Skip "FPS"
            while (*fps_ptr && !isdigit(*fps_ptr)) {
                fps_ptr++;
            }

            // Extract the number
            std::string fps_value;
            while (*fps_ptr && (isdigit(*fps_ptr) || *fps_ptr == '.')) {
                fps_value += *fps_ptr;
                fps_ptr++;
            }

            if (!fps_value.empty()) {
                fps_string.store(fps_value);
              //  shellui_log("Captured FPS: %s", fps_value.c_str());
            }
            return -1;
        }
    }
    return ret;
}

int get_ip_address(char* ip_address);
void OnRender_Hook(MonoObject* instance)
{
    static bool Do_Once = false;
    static unsigned int Idle_Thread_ID[8];
    static int Current_Bank = 0;

    // Separate labels for text and values
    static MonoObject* gpu_temp_value = nullptr;
    static MonoObject* gpu_usage_value = nullptr;

    static MonoObject* cpu_temp_value = nullptr;
    static MonoObject* cpu_usage_value = nullptr;

    static MonoObject* ram_value = nullptr;
    static MonoObject* fps_value = nullptr;


    char GPU_TEMP[32];
    char GPU_USAGE[32];
    char CPU_TEMP[32];
    char CPU_USAGE[120];
    char RAM_STR[32];

    static int wait = 0;
    int SOC_temp = 0;
    int CPU_temp = 0;

    if (!Do_Once)
    {
#if 1
        fps_string.store("LOADING");
#else
        fps_string.store("NOT SUPPORTED IN THIS BUILD");
#endif
	//	shellui_log("string %s", fps_string.load().c_str());
        int Thread_Count = 3072;
        if (!sceKernelGetCpuUsage((Proc_Stats*)&Stat_Data, (int*)&Thread_Count) && Thread_Count > 0)
        {
            char Thread_Name[0x40];
            int Core_Count = 0;
            for (int i = 0; i < Thread_Count; i++)
            {
                if (!sceKernelGetThreadName(Stat_Data[i].td_tid, Thread_Name) && sscanf(Thread_Name, "SceIdleCpu%d", &Core_Count) == 1 && Core_Count <= 7)
                {
                    Idle_Thread_ID[Core_Count] = Stat_Data[i].td_tid;
                }
            }
        }

        rootWidget = Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget");
        font = CreateUIFont(22, 0, 0);           // Regular font for values

        // GPU row - Green label (BOLD), Orange values - Better spacing
        if (g_settings.overlay_cpu) {
            CreateGameWidget(CREATE_CPU_OVERLAY);
        }
        if (g_settings.overlay_ram) {
            CreateGameWidget(CREATE_RAM_OVERLAY);
        }
        if (g_settings.overlay_gpu) {
            CreateGameWidget(CREATE_GPU_OVERLAY);
        }
        if (g_settings.overlay_fps) {
            CreateGameWidget(CREATE_FPS_OVERLAY);
        }
		if (g_settings.overlay_ip) {
			CreateGameWidget(CREATE_IP_OVERLAY);
		}

        Do_Once = true;
    }


    if (wait <= 0) {


        // Get CPU usage
        while (g_settings.overlay_cpu || g_all_cpu_usage) {
            gThread_Data[Current_Bank].Thread_Count = 3072;
            if (!sceKernelGetCpuUsage((Proc_Stats*)&gThread_Data[Current_Bank].Threads, &gThread_Data[Current_Bank].Thread_Count))
            {
                Thread_Count = gThread_Data[Current_Bank].Thread_Count;
                sceKernelClockGettime(4, &gThread_Data[Current_Bank].current_time);
                Current_Bank = !Current_Bank;

                if (gThread_Data[Current_Bank].Thread_Count <= 0)
                    continue;

                calc_usage(Idle_Thread_ID, &gThread_Data[!Current_Bank], &gThread_Data[Current_Bank], Usage);

                if (g_all_cpu_usage) {
                    snprintf(CPU_USAGE, sizeof(CPU_USAGE), "%2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%%",Usage[0], Usage[1], Usage[2], Usage[3], Usage[4], Usage[5], Usage[6], Usage[7]);
                    break;
                }

                // Calculate average CPU usage
                float avg_cpu = 0;
                for (int i = 0; i < 8; i++) {
                    avg_cpu += Usage[i];
                }
                avg_cpu /= 8.0f;

                snprintf(CPU_USAGE, sizeof(CPU_USAGE), "%.0f%%", avg_cpu);
                break;
            }
        }

        // Get RAM info
        if (g_settings.overlay_ram)
        {
            Get_Page_Table_Stats(1, 1, &RAM.Used, &RAM.Free, &RAM.Total);
            snprintf(RAM_STR, sizeof(RAM_STR), "%u MB", RAM.Used);
        }

        // Get GPU usage (estimate based on VRAM usage)
        if (g_settings.overlay_gpu) 
        {
            // Get temperatures
            sceKernelGetSocSensorTemperature(0, &SOC_temp);
            snprintf(GPU_TEMP, sizeof(GPU_TEMP), "%dC", SOC_temp);
            Get_Page_Table_Stats(1, 2, &VRAM.Used, &VRAM.Free, &VRAM.Total);
            VRAM.Percentage = (((float)VRAM.Used / (float)VRAM.Total) * 100.0f);
            snprintf(GPU_USAGE, sizeof(GPU_USAGE), "%.0f%%", VRAM.Percentage);
        }
        if(g_settings.overlay_ip)
        {
			char ip_address[64];
            get_ip_address(&ip_address[0]);
            MonoObject* ip_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_ip_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), ip_value, "Text", mono_string_new(Root_Domain, ip_address));
		}

        if (g_settings.overlay_gpu) {
            // Update GPU values
            gpu_temp_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_gpu_temp_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), gpu_temp_value, "Text", mono_string_new(Root_Domain, GPU_TEMP));

            gpu_usage_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_gpu_usage_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), gpu_usage_value, "Text", mono_string_new(Root_Domain, GPU_USAGE));
        }
        if (g_settings.overlay_cpu || g_all_cpu_usage) {
            sceKernelGetCpuTemperature(&CPU_temp);
            // Format temperature strings
            snprintf(CPU_TEMP, sizeof(CPU_TEMP), "%dC", CPU_temp);
            // Update CPU values
            cpu_temp_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_cpu_temp_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), cpu_temp_value, "Text", mono_string_new(Root_Domain, CPU_TEMP));

            cpu_usage_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_cpu_usage_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), cpu_usage_value, "Text", mono_string_new(Root_Domain, CPU_USAGE));
        }
        if(g_settings.overlay_ram) 
        {
            // Update RAM value
            ram_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_ram_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), ram_value, "Text", mono_string_new(Root_Domain, RAM_STR));
		}
        if (g_settings.overlay_fps) {
            // Update FPS value
            std::string current_fps = fps_string.load();
            fps_value = Invoke<MonoObject*>(pui_img, mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Widget"), Get_Property<MonoObject*>(pui_img, "Sce.PlayStation.PUI.UI2", "Scene", Game, "RootWidget"), "FindWidgetByName", mono_string_new(Root_Domain, "id_fps_value"));
            Set_Property(mono_class_from_name(pui_img, "Sce.PlayStation.PUI.UI2", "Label"), fps_value, "Text", mono_string_new(Root_Domain, current_fps.c_str()));
        }
        wait = 60; // Update every 60 frames
    }
    else {
        wait--;
    }

    OnRender_orig(instance);
}



