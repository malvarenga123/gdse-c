#include "gdse.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#define GD_MKDIR(path) _mkdir(path)
#else
#include <unistd.h>
#define GD_MKDIR(path) mkdir(path, 0777)
#endif

void gd_set_error(gd_error *err, const char *fmt, ...)
{
    va_list args;
    if (err == NULL) return;
    va_start(args, fmt);
    vsprintf(err->message, fmt, args);
    va_end(args);
}

void *gd_alloc(size_t size, gd_error *err)
{
    void *result = malloc(size == 0 ? 1 : size);
    if (result == NULL) gd_set_error(err, "out of memory");
    return result;
}

char *gd_strdup(const char *text, gd_error *err)
{
    size_t len = strlen(text);
    char *copy = (char *)gd_alloc(len + 1, err);
    if (copy != NULL) memcpy(copy, text, len + 1);
    return copy;
}

int gd_read_u16(FILE *file, gd_u16 *value, gd_error *err)
{
    gd_u8 b[2];
    if (fread(b, 1, 2, file) != 2) {
        gd_set_error(err, "unexpected end of file");
        return 0;
    }
    *value = (gd_u16)((gd_u16)b[0] | ((gd_u16)b[1] << 8));
    return 1;
}

int gd_read_u32(FILE *file, gd_u32 *value, gd_error *err)
{
    gd_u8 b[4];
    if (fread(b, 1, 4, file) != 4) {
        gd_set_error(err, "unexpected end of file");
        return 0;
    }
    *value = (gd_u32)b[0] | ((gd_u32)b[1] << 8) |
             ((gd_u32)b[2] << 16) | ((gd_u32)b[3] << 24);
    return 1;
}

int gd_seek(FILE *file, gd_u32 offset, gd_error *err)
{
    if (offset > 0x7fffffffUL || fseek(file, (long)offset, SEEK_SET) != 0) {
        gd_set_error(err, "could not seek in input");
        return 0;
    }
    return 1;
}

int gd_path_exists(const char *path)
{
    struct stat info;
    return stat(path, &info) == 0;
}

int gd_is_directory(const char *path)
{
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
}

int gd_mkdirs(const char *path, gd_error *err)
{
    char *copy;
    char *p;
    if (gd_is_directory(path)) return 1;
    copy = gd_strdup(path, err);
    if (copy == NULL) return 0;
    for (p = copy + 1; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
            if (*copy != '\0' && !gd_is_directory(copy) &&
                GD_MKDIR(copy) != 0 && errno != EEXIST) {
                gd_set_error(err, "could not create %s: %s", copy,
                             strerror(errno));
                free(copy);
                return 0;
            }
            *p = saved;
        }
    }
    if (!gd_is_directory(copy) && GD_MKDIR(copy) != 0 && errno != EEXIST) {
        gd_set_error(err, "could not create %s: %s", copy, strerror(errno));
        free(copy);
        return 0;
    }
    free(copy);
    return 1;
}

char *gd_path_join(const char *left, const char *right, gd_error *err)
{
    size_t a = strlen(left);
    size_t b = strlen(right);
    int slash = a != 0 && left[a - 1] != '/' && left[a - 1] != '\\';
    char *out = (char *)gd_alloc(a + b + (size_t)slash + 1, err);
    if (out == NULL) return NULL;
    memcpy(out, left, a);
    if (slash) out[a++] = '/';
    memcpy(out + a, right, b + 1);
    return out;
}

int gd_safe_record_path(const char *path)
{
    const char *p = path;
    if (*p == '/' || *p == '\\' || (p[0] != '\0' && p[1] == ':')) return 0;
    while (*p != '\0') {
        const char *start = p;
        while (*p != '\0' && *p != '/' && *p != '\\') ++p;
        if ((p - start == 2 && start[0] == '.' && start[1] == '.') ||
            (p - start == 0)) return 0;
        if (*p != '\0') ++p;
    }
    return 1;
}
