#include "../include/proc.h"

#include <string.h>

struct proc* find_proc_by_name(const char* proc_name)
{

    uint64_t next = 0;
    kernel_copyout(KERNEL_ADDRESS_ALLPROC, &next, sizeof(uint64_t));
    struct proc* proc = (struct proc*) malloc(sizeof(struct proc));
    struct vmspace vmspace;

    do
    {
        kernel_copyout(next, (void*) proc, sizeof(struct proc));

        kernel_copyout((intptr_t) proc->p_vmspace, (void*) &vmspace, sizeof(vmspace));

        if (!strcmp(proc->p_comm, proc_name))
            return proc;

        kernel_copyout(next, &next, sizeof(uint64_t));

    } while (next);

    free(proc);
    return NULL;
}
int sceKernelDebugOutText(int a1, const char* fmt);
void list_all_proc_and_pid()
{

    uint64_t next = 0;
    kernel_copyout(KERNEL_ADDRESS_ALLPROC, &next, sizeof(uint64_t));
    struct proc* proc = (struct proc*) malloc(sizeof(struct proc));
    struct vmspace vmspace;

    do
    {
        kernel_copyout(next, (void*) proc, sizeof(struct proc));

        kernel_copyout((intptr_t) proc->p_vmspace, (void*) &vmspace, sizeof(vmspace));
        char buffer[0x100];
        sprintf(buffer, "%s - %d\n", proc->p_comm, proc->pid);

        sceKernelDebugOutText(0, buffer);

        kernel_copyout(next, &next, sizeof(uint64_t));

    } while (next);

    free(proc);
}

struct proc* get_proc_by_pid(pid_t pid)
{
    uintptr_t next = 0;

    kernel_copyout(KERNEL_ADDRESS_ALLPROC, &next, sizeof(uintptr_t));
    struct proc* proc =  (struct proc*) malloc(sizeof(struct proc));
    do
    {
        kernel_copyout(next, proc, sizeof(struct proc));

        if (proc->pid == pid)
            return proc;

        kernel_copyout(next, &next, sizeof(uint64_t));

    } while (next);

    free(proc);
    return NULL;
}


//
// List process modules by using the sys_dynlib_get_info_ex syscall
//
void list_proc_modules(struct proc* proc)
{
    size_t num_handles = 0;
    syscall(SYS_dl_get_list, proc->pid, NULL, 0, &num_handles);
    
    if (num_handles)
    {
        uintptr_t* handles = (uintptr_t*) calloc(num_handles, sizeof(uintptr_t));
        syscall(SYS_dl_get_list, proc->pid, handles, num_handles, &num_handles);

        for (int i = 0; i < num_handles; ++i)
        {
            module_info_t mod_info;
            bzero(&mod_info, sizeof(mod_info));

            syscall(SYS_dl_get_info_2, proc->pid, 1, handles[i], &mod_info);

            printf("%s - ", mod_info.filename);
            printf("%#02lx\n", mod_info.init);
        }
        
        free(handles);
    }
}

static const char *path_basename_c(const char *path)
{
    const char *slash;

    if (path == NULL || path[0] == '\0') {
        return "";
    }
    slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static int module_name_matches(const module_info_t *mod, const char *module_name)
{
    if (mod == NULL || module_name == NULL) {
        return 0;
    }
    return strcmp(mod->filename, module_name) == 0 ||
           strcmp(mod->libname, module_name) == 0 ||
           strcmp(path_basename_c(mod->sandboxed_path), module_name) == 0;
}

/*
 * Fill *out with the first loaded module matching name (filename, libname,
 * or sandboxed path basename). Returns 0 on success.
 */
int get_module_info(pid_t pid, const char *module_name, module_info_t *out)
{
    size_t num_handles = 0;
    uintptr_t *handles = NULL;
    size_t i;

    if (module_name == NULL || module_name[0] == '\0' || out == NULL) {
        return -1;
    }

    memset(out, 0, sizeof(*out));
    syscall(SYS_dl_get_list, pid, NULL, 0, &num_handles);
    if (num_handles == 0) {
        return -1;
    }

    handles = (uintptr_t *)calloc(num_handles, sizeof(uintptr_t));
    if (handles == NULL) {
        return -1;
    }

    syscall(SYS_dl_get_list, pid, handles, num_handles, &num_handles);
    for (i = 0; i < num_handles; ++i) {
        module_info_t tmp;
        memset(&tmp, 0, sizeof(tmp));
        syscall(SYS_dl_get_info_2, pid, 1, handles[i], &tmp);
        if (module_name_matches(&tmp, module_name)) {
            *out = tmp;
            free(handles);
            return 0;
        }
    }

    free(handles);
    return -1;
}

/*
 * Heap-allocated variant kept for existing callers (injector, etc.).
 * Prefer get_module_info() for new code.
 */
module_info_t* get_module_handle(pid_t pid, const char* module_name)
{
    module_info_t *mod_info = (module_info_t *)malloc(sizeof(module_info_t));

    if (mod_info == NULL) {
        return NULL;
    }
    if (get_module_info(pid, module_name, mod_info) == 0) {
        return mod_info;
    }
    free(mod_info);
    return NULL;
}
