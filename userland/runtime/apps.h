#ifndef AGENTICOS_APPS_H
#define AGENTICOS_APPS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "syscall/abi.h"
#define EDITOR_CAPACITY 4096u
#define KEY_CONTROL 1u
struct editor_model { char path[128], text[EDITOR_CAPACITY], status[64]; size_t length, cursor; bool dirty, path_mode; };
struct monitor_model { struct system_info system; struct self_info self; struct task_info tasks[16]; size_t count; };
extern struct editor_model editor;
extern struct monitor_model monitor;
extern bool apps_slate_theme;
extern char apps_status[64];
void apps_init(void);
long apps_atomic_save(const char *path, const void *bytes, size_t length);
void editor_new(bool discard);
long editor_open(const char *path);
long editor_save(void);
void editor_key(uint32_t key, uint32_t modifiers);
void editor_choose_path(void);
void monitor_refresh(void);
long settings_select(bool slate);
#endif
