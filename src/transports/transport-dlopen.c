#include <aura/aura.h>
#include <aura/private.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>


static void load_plugin(const char *path)
{
    slog(4, SLOG_DEBUG, "Loading plugin from: %s", path);
    void *ret = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!ret) {
        slog(0, SLOG_WARN, "Failed to load dynamic transport plugin: %s", path);
    }
    /* That's it. Constructors will do their job */
}

static void load_from_directory(const char *path)
{
    slog(4, SLOG_DEBUG, "Looking for plugins in %s", path);
    DIR *d = opendir(path);
    if (!d) {
        slog(0, SLOG_WARN, "Failed to open plugin directory, ignoring: %s", path);
        return;
    }
    struct dirent *el;
    while (el = readdir(d)) {
        if (strstr(el->d_name, "libauratransport-") && strstr(el->d_name, ".so")) {
            char *tmp;
            asprintf(&tmp, "%s/%s", path, el->d_name);
            load_plugin(tmp);
            free(tmp);
        }
    }
}

void __attribute__((constructor(100))) register_transport_plugins(void)
{
    const char *plpath = getenv("AURA_PLUGIN_PATH");
    if (plpath) {
        load_from_directory(plpath);
    }
    load_from_directory(CMAKE_INSTALL_PREFIX "/lib/aura/");
}
