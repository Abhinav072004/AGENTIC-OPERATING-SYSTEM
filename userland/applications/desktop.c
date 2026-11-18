#include "syscall.h"
#include "text.h"
#include "commands.h"
#include "apps.h"
#include "window.h"
#include "gui/abi.h"
#include "drivers/input.h"
#include "fs/vfs.h"
int user_main(uint64_t pid, uint64_t mode);
static struct window_manager wm;
static bool menu, failed, quitting, interactive;
static int mouse_x, mouse_y;
static uint32_t buttons;
static char terminal[18][64], terminal_input[TEXT_LINE_MAX];
static size_t terminal_row, terminal_column, input_length;
static char files_path[128] = "/home", file_text[1024];
static struct fs_entry entries[16]; static size_t entry_count; static int selected = -1;
static char expression[64], calculation[64] = "Enter an integer expression";
static void trace(const char *message) { (void)user_write(1, message, text_length(message)); }
static void rect(int x, int y, int width, int height, uint32_t color) { struct gui_draw draw = {.operation = GUI_RECT, .x = x, .y = y, .width = width, .height = height, .color = color}; if (user_call(SYS_GUI_DRAW, (uintptr_t)&draw, 0, 0) != 0) { failed = true; } }
static void label(int x, int y, const char *text, uint32_t color) { struct gui_draw draw = {.operation = GUI_TEXT, .x = x, .y = y, .color = color}; text_copy(draw.text, text, sizeof(draw.text)); if (user_call(SYS_GUI_DRAW, (uintptr_t)&draw, 0, 0) != 0) { failed = true; } }
static const char *title(unsigned int app) { return app == APP_TERMINAL ? "Terminal" : app == APP_FILES ? "Files" : app == APP_CALCULATOR ? "Calculator" : app == APP_EDITOR ? "Editor" : app == APP_MONITOR ? "Monitor" : "Settings"; }
static void emit(const char *text) {
    for (size_t i = 0; text[i] != 0; ++i) {
        if (text[i] == '\r') { continue; }
        if (text[i] == '\n' || terminal_column == 63) {
            if (terminal_row == 17) { for (size_t row = 0; row < 17; ++row) { text_copy(terminal[row], terminal[row + 1], sizeof(terminal[row])); } terminal[17][0] = 0; }
            else { ++terminal_row; }
            terminal_column = 0; if (text[i] == '\n') { continue; }
        }
        terminal[terminal_row][terminal_column++] = text[i]; terminal[terminal_row][terminal_column] = 0;
    }
}
static void refresh_files(void) { entry_count = 0; selected = -1; file_text[0] = 0; while (entry_count < 16 && user_call(SYS_LIST, (uintptr_t)files_path, entry_count, (uintptr_t)&entries[entry_count]) == 1) { ++entry_count; } }
static void launch(unsigned int app) { for (unsigned int i = 0; i < WM_MAX; ++i) { if (wm.windows[i].live && wm.windows[i].app == app) { wm_focus(&wm,i); menu = false; trace("GUI focus"); return; } } if (wm_open(&wm, app) < 0) { emit("Window limit reached\n"); } menu = false; trace("GUI launch"); }
static void button(int x, int y, int width, const char *text, bool active) { rect(x,y,width,24,active ? (apps_slate_theme ? 0x327569 : 0x285da3) : 0x2b3b52); label(x+8,y+8,text,0xe8f0fc); }
static void render_window(unsigned int slot) {
    const struct window *window = &wm.windows[slot]; int x = window->x, y = window->y, width = window->width;
    rect(x+4,y+5,width,window->height,0x080e19); rect(x,y,width,window->height,0x24354b); rect(x+1,y+25,width-2,window->height-26,0x172438);
    rect(x+1,y+1,width-2,23,wm.active == (int)slot ? (apps_slate_theme ? 0x327569 : 0x285da3) : 0x34465e); label(x+10,y+9,window->app == APP_EDITOR && editor.dirty ? "Editor *" : title(window->app),0xffffff); label(x+width-18,y+9,"X",0xffffff);
    if (window->app == APP_TERMINAL) {
        rect(x+8,y+32,width-16,window->height-40,0x0b1422);
        size_t first = terminal_row > 14 ? terminal_row-14 : 0;
        for (size_t i = first; i <= terminal_row; ++i) { label(x+14,y+40+(int)(i-first)*12,terminal[i],0xc7dbef); }
        label(x+14,y+window->height-35,commands_cwd(),0x79b9ff); label(x+14,y+window->height-20,terminal_input,0xffffff);
    } else if (window->app == APP_FILES) {
        label(x+12,y+36,files_path,0x9ecaff); button(x+12,y+52,64,"Home",false); button(x+82,y+52,48,"Up",false); button(x+136,y+52,64,"New",false); button(x+206,y+52,76,"Delete",false); button(x+288,y+52,64,"Edit",false);
        for (size_t i = 0; i < entry_count; ++i) { if ((int)i == selected) { rect(x+10,y+86+(int)i*11,170,11,0x285da3); } label(x+14,y+88+(int)i*11,entries[i].name,entries[i].info.directory != 0 ? 0x8fc7ff : 0xdce6f5); }
        rect(x+184,y+86,width-196,window->height-100,0x101c2d);
        size_t offset = 0; for (int row = 0; row < 14 && file_text[offset] != 0; ++row) { char line[32]; size_t count = 0; while (count < 30 && file_text[offset] != 0 && file_text[offset] != '\n') { line[count++] = file_text[offset++]; } if (file_text[offset] == '\n') { ++offset; } line[count] = 0; label(x+190,y+94+row*11,line,0xb7cce4); }
    } else if (window->app == APP_CALCULATOR) {
        label(x+14,y+40,expression[0] == 0 ? "0" : expression,0xffffff); label(x+14,y+58,calculation,0x8fc7ff);
        static const char keys[] = "789/456*123-0C=+";
        for (int row = 0; row < 4; ++row) { for (int column = 0; column < 4; ++column) { char key[2] = {keys[row*4+column],0}; button(x+16+column*62,y+84+row*40,54,key,false); } }
        label(x+14,y+258,"Enter evaluates. C clears.",0xa9bed6);
    } else if (window->app == APP_EDITOR) {
        button(x+12,y+34,64,"Open",false); button(x+82,y+34,64,"Save",false); button(x+152,y+34,64,"New",false); button(x+222,y+34,80,"Discard",false);
        rect(x+10,y+66,width-20,20,editor.path_mode ? 0x285da3 : 0x0c1829); label(x+16,y+73,editor.path,0xdbeaff);
        rect(x+10,y+92,width-20,window->height-126,0x0b1422);
        size_t cursor_row = 0, cursor_column = 0; for (size_t i = 0; i < editor.cursor; ++i) { if (editor.text[i] == '\n' || cursor_column == 63) { ++cursor_row; cursor_column = 0; if (editor.text[i] == '\n') { continue; } } ++cursor_column; }
        size_t first_row = cursor_row >= 16 ? cursor_row-15 : 0, offset = 0;
        for (size_t row = 0; row < first_row+16; ++row) { char line[64]; size_t count = 0; while (count < 63 && editor.text[offset] != 0 && editor.text[offset] != '\n') { line[count++] = editor.text[offset++]; } if (editor.text[offset] == '\n') { ++offset; } line[count] = 0; if (row >= first_row) { label(x+16,y+100+(int)(row-first_row)*12,line,0xdce8f5); } if (editor.text[offset] == 0) { break; } }
        if (!editor.path_mode) { rect(x+16+(int)cursor_column*6,y+108+(int)(cursor_row-first_row)*12,5,1,0x8fc7ff); }

        label(x+12,y+window->height-22,editor.status,editor.dirty ? 0xffd184 : 0x8fc7ff);
    } else if (window->app == APP_MONITOR) {
        char value[22]; label(x+12,y+38,"Memory use",0x9bc5f1); uint64_t total = monitor.system.managed_frames, free = monitor.system.free_frames; int used = total == 0 ? 0 : (int)((total-free)*((uint64_t)width-32u)/total);
        rect(x+14,y+54,width-28,12,0x0b1422); rect(x+14,y+54,used,12,0x377fbd); (void)text_number(value,(int64_t)(free/256u)); label(x+14,y+76,value,0xffffff); label(x+120,y+76,"MiB free",0xa8c1dc);
        label(x+14,y+100,"PID   USER TICKS   NAME",0x93bde8);
        for (size_t i = 0; i < monitor.count && i < 10; ++i) { (void)text_number(value,(int64_t)monitor.tasks[i].pid); label(x+14,y+120+(int)i*12,value,0xffffff); (void)text_number(value,(int64_t)monitor.tasks[i].runtime_ticks); label(x+66,y+120+(int)i*12,value,0xc7d9ef); label(x+150,y+120+(int)i*12,monitor.tasks[i].name,0xc7d9ef); }
        button(x+14,y+window->height-36,100,"Refresh",false);
    } else if (window->app == APP_SETTINGS) {
        label(x+14,y+40,"Appearance",0xbad7f4); button(x+14,y+62,130,"Ocean",!apps_slate_theme); button(x+152,y+62,130,"Slate",apps_slate_theme);
        char value[22]; (void)text_number(value,monitor.self.uid); label(x+14,y+104,"Session UID",0xa5bfdb); label(x+120,y+104,value,0xffffff);
        label(x+14,y+128,"Files are stored in RAM.",0xd7e5f6); label(x+14,y+144,"Reboot clears files and settings.",0xa9c0db);
        label(x+14,y+174,"Editor shortcut: Ctrl+S saves",0xa9c0db); label(x+14,y+198,"AgenticOS",0x9dc9f4); label(x+92,y+198,monitor.system.version,0xffffff); label(x+14,y+222,apps_status,0x8fc7ff);
    }
}
static void render(void) {
    rect(0,0,wm.width,wm.height,apps_slate_theme ? 0x102521 : 0x101a2b); rect(0,0,wm.width,40,0x18283f); label(20,16,"AgenticOS",0xf2f7ff); label(wm.width-200,16,"Desktop / RAM files",0x8db5df);
    label(24,64,"Your tools. Your operating system.",0x829fbd);
    for (unsigned int i = 0; i < wm.count; ++i) { render_window(wm.order[i]); }
    rect(0,wm.height-38,wm.width,38,0x1b2b41); button(10,wm.height-31,110,"Applications",menu);
    int task_width = wm.count == 0 ? 112 : (wm.width-140)/(int)wm.count; if (task_width > 112) { task_width = 112; }
    for (unsigned int i = 0; i < wm.count; ++i) { unsigned int slot = wm.order[i]; button(130+(int)i*task_width,wm.height-31,task_width-4,title(wm.windows[slot].app),wm.active == (int)slot); }
    if (menu) { rect(10,wm.height-280,190,240,0x253b57); label(24,wm.height-264,"Launch an application",0x97bfe5); for (unsigned int app = APP_TERMINAL; app <= APP_SETTINGS; ++app) { button(20,wm.height-244+(int)(app-1)*34,170,title(app),false); } }

    for (int row = 0; row < 14; ++row) { rect(mouse_x+1,mouse_y+row+1,row/2+2,1,0x000000); rect(mouse_x,mouse_y+row,row/2+1,1,0xffffff); }
    if (user_call(SYS_GUI_PRESENT,0,0,0) != 0) { failed = true; }
    if (interactive) { trace("GUI frame presented"); }
}
static void calculate(void) { int64_t value; if (wm_calculate(expression,&value)) { (void)text_number(calculation,value); char message[96] = "GUI calc="; size_t offset = text_length(message); text_copy(message+offset,calculation,sizeof(message)-offset); trace(message); } else { text_copy(calculation,"Invalid / overflow / divide by zero",sizeof(calculation)); } }
static void calc_key(uint32_t key) { size_t length = text_length(expression); if (key == '\n' || key == '=') { calculate(); } else if (key == 'C' || key == 'c') { expression[0] = 0; text_copy(calculation,"0",sizeof(calculation)); } else if (key == '\b') { if (length != 0) { expression[length-1] = 0; } } else if (((key >= '0' && key <= '9') || key == '+' || key == '-' || key == '*' || key == '/' || key == '(' || key == ')' || key == ' ') && length+1 < sizeof(expression)) { expression[length] = (char)key; expression[length+1] = 0; } }
static void open_selected(void) { if (selected < 0 || selected >= (int)entry_count) { return; } char path[128]; if (!text_path(files_path,entries[selected].name,path)) { return; } if (entries[selected].info.directory != 0) { text_copy(files_path,path,sizeof(files_path)); refresh_files(); return; } long fd = user_call(SYS_OPEN,(uintptr_t)path,FS_READ,0); file_text[0] = 0; if (fd < 0) { return; } size_t offset = 0; while (offset+1 < sizeof(file_text)) { size_t amount = sizeof(file_text)-1-offset; if (amount > SYS_IO_MAX) { amount = SYS_IO_MAX; } long result = user_read((uint64_t)fd,file_text+offset,amount); if (result <= 0) { break; } offset += (size_t)result; } file_text[offset] = 0; (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0); trace("GUI file opened"); }
static void key_event(uint32_t key, uint32_t modifiers) {
    if (key == 27) { if (editor.dirty) { text_copy(editor.status,"Unsaved changes: Save or Discard first",sizeof(editor.status)); launch(APP_EDITOR); } else { quitting = true; } return; } if (wm.active < 0) { return; }
    unsigned int app = wm.windows[wm.active].app;
    if (app == APP_EDITOR) { bool was_dirty = editor.dirty; editor_key(key,modifiers); if (was_dirty && !editor.dirty) { refresh_files(); } }
    else if (app == APP_CALCULATOR) { calc_key(key); }
    else if (app == APP_TERMINAL) {
        if (key == '\n') { emit("> "); emit(terminal_input); emit("\n"); (void)commands_run(terminal_input); input_length = 0; terminal_input[0] = 0; refresh_files(); trace("GUI terminal command"); if (commands_leaving()) { wm_close(&wm,(unsigned int)wm.active); commands_init(emit); } }
        else if (key == '\b') { if (input_length != 0) { terminal_input[--input_length] = 0; } }
        else if (key >= 32 && key <= 126 && input_length+1 < sizeof(terminal_input)) { terminal_input[input_length++] = (char)key; terminal_input[input_length] = 0; }
    } else if (app == APP_FILES) { if (key == '\n') { open_selected(); } else if (key == '\b') { char path[128]; if (text_path(files_path,"..",path)) { text_copy(files_path,path,sizeof(files_path)); refresh_files(); } } }
}
static void click(void) {
    int x = mouse_x, y = mouse_y;
    if (y >= wm.height-38) { if (x < 124) { menu = !menu; return; } int task_width = wm.count == 0 ? 112 : (wm.width-140)/(int)wm.count; if (task_width > 112) { task_width = 112; } if (x >= 130) { unsigned int position = (unsigned int)((x-130)/task_width); if (position < wm.count) { wm_focus(&wm,wm.order[position]); } } return; }
    if (menu && x >= 10 && x < 200 && y >= wm.height-280) { int index = (y-(wm.height-244))/34; if (y >= wm.height-244 && index >= 0 && index < 6) { launch((unsigned int)index+1u); } return; }

    menu = false; int hit = wm_hit(&wm,x,y); if (hit < 0) { return; } wm_focus(&wm,(unsigned int)hit); trace("GUI focus"); struct window *window = &wm.windows[hit];
    if (y < window->y+25) { if (x >= window->x+window->width-28) { if (window->app == APP_EDITOR && editor.dirty) { text_copy(editor.status,"Unsaved changes: Save or Discard first",sizeof(editor.status)); return; } wm_close(&wm,(unsigned int)hit); trace("GUI close"); } else { wm.dragging = hit; wm.anchor_x = x-window->x; wm.anchor_y = y-window->y; } return; }
    if (window->app == APP_EDITOR) {
        int local_x = x-window->x, local_y = y-window->y;
        if (local_y >= 34 && local_y < 58) { if (local_x >= 12 && local_x < 76) { editor_choose_path(); } else if (local_x >= 82 && local_x < 146) { if (editor_save() == 0) { refresh_files(); } } else if (local_x >= 152 && local_x < 216) { editor_new(false); } else if (local_x >= 222 && local_x < 302) { editor_new(true); } }
    } else if (window->app == APP_MONITOR) { monitor_refresh(); trace("APP monitor refreshed"); }
    else if (window->app == APP_SETTINGS) { if (y >= window->y+62 && y < window->y+86) { if (x >= window->x+14 && x < window->x+144) { if (settings_select(false) == 0) { trace("APP theme ocean"); } } else if (x >= window->x+152 && x < window->x+282) { if (settings_select(true) == 0) { trace("APP theme slate"); } } } }
    else if (window->app == APP_CALCULATOR) { int column = (x-window->x-16)/62, row = (y-window->y-84)/40; if (x >= window->x+16 && y >= window->y+84 && column >= 0 && column < 4 && row >= 0 && row < 4) { static const char keys[] = "789/456*123-0C=+"; calc_key((uint32_t)(unsigned char)keys[row*4+column]); } }
    else if (window->app == APP_FILES) {
        int local_x = x-window->x, local_y = y-window->y;
        if (local_y >= 52 && local_y < 76) { if (local_x >= 12 && local_x < 76) { text_copy(files_path,"/home",sizeof(files_path)); refresh_files(); } else if (local_x >= 82 && local_x < 130) { char path[128]; if (text_path(files_path,"..",path)) { text_copy(files_path,path,sizeof(files_path)); refresh_files(); } } else if (local_x >= 136 && local_x < 200) { char path[128]; if (text_path(files_path,"untitled",path)) { long fd = user_call(SYS_OPEN,(uintptr_t)path,FS_CREATE|FS_WRITE,0); if (fd >= 0) { (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0); } refresh_files(); } } else if (local_x >= 206 && local_x < 282 && selected >= 0) { char path[128]; if (text_path(files_path,entries[selected].name,path)) { (void)user_call(SYS_UNLINK,(uintptr_t)path,0,0); refresh_files(); } } else if (local_x >= 288 && local_x < 352 && selected >= 0) { char path[128]; if (text_path(files_path,entries[selected].name,path) && entries[selected].info.directory == 0) { launch(APP_EDITOR); (void)editor_open(path); } } }
        else if (local_x >= 10 && local_x < 180 && local_y >= 86) { int index = (local_y-86)/11; if (index >= 0 && index < (int)entry_count) { selected = index; open_selected(); } }
    }
}
int user_main(uint64_t pid, uint64_t mode) {
    (void)pid; interactive = mode == 0; struct gui_info info; if (user_call(SYS_GUI_INFO,(uintptr_t)&info,0,0) != 0) { return 1; }
    wm_init(&wm,(int)info.width,(int)info.height); mouse_x = wm.width/2; mouse_y = wm.height/2; commands_init(emit); apps_init(); emit("AgenticOS terminal\nType help to list commands.\n"); refresh_files(); launch(APP_TERMINAL); launch(APP_FILES); launch(APP_CALCULATOR); render();
    if (mode == 1) { if (commands_run("write /home/gui-check 'native desktop file'") != 0 || commands_run("cat /home/gui-check") != 0 || commands_run("rm /home/gui-check") != 0) { return 2; } text_copy(expression,"6*7",sizeof(expression)); calculate(); if (!text_equal(calculation,"42")) { return 3; } refresh_files(); render(); trace("native desktop checks passed"); return failed ? 4 : 0; }
    if (mode == 2) {
        launch(APP_EDITOR); editor_new(true); text_copy(editor.path,"/home/apps-check",sizeof(editor.path)); const char *sample = "Native editor saved this file."; for (size_t i = 0; sample[i] != 0; ++i) { editor_key((uint32_t)(unsigned char)sample[i],0); }
        if (editor_save() != 0 || editor_open("/home/apps-check") != 0 || !text_equal(editor.text,sample)) { return 6; }
        unsigned int fillers = 0; bool full = false; char fill[SYS_IO_MAX]; for (size_t i = 0; i < sizeof(fill); ++i) { fill[i] = 'q'; }
        for (unsigned int file = 0; file < 8 && !full; ++file) { char path[] = "/tmp/app-quota0"; path[14] = (char)('0'+file); long fd = user_call(SYS_OPEN,(uintptr_t)path,FS_CREATE|FS_WRITE|FS_EXCLUSIVE,0); if (fd < 0) { return 12; } ++fillers; for (size_t offset = 0; offset < FS_FILE_MAX; offset += sizeof(fill)) { if (user_write((uint64_t)fd,fill,sizeof(fill)) < 0) { full = true; break; } } (void)user_call(SYS_CLOSE,(uint64_t)fd,0,0); }
        editor.cursor = editor.length; editor_key('!',0); if (!full || editor_save() != FS_ENOSPC) { return 13; }
        for (unsigned int file = 0; file < fillers; ++file) { char path[] = "/tmp/app-quota0"; path[14] = (char)('0'+file); if (user_call(SYS_UNLINK,(uintptr_t)path,0,0) != 0) { return 14; } }
        editor.dirty = false; if (editor_open("/home/apps-check") != 0 || !text_equal(editor.text,sample)) { return 15; }
        if (user_call(SYS_UNLINK,(uintptr_t)"/home/apps-check",0,0) != 0) { return 7; }
        int64_t value; if (!wm_calculate("2*(3+4)-5",&value) || value != 9) { return 8; }
        launch(APP_MONITOR); monitor_refresh(); if (monitor.count < 2 || monitor.system.free_frames == 0) { return 9; }
        launch(APP_SETTINGS); settings_select(true); if (!apps_slate_theme) { return 10; } settings_select(false); (void)user_call(SYS_UNLINK,(uintptr_t)"/home/.settings",0,0); render(); trace("native application checks passed"); return failed ? 11 : 0;
    }
    trace("GUI READY");
    while (!quitting && !failed) {
        struct input_event event; bool changed = false;
        while (user_call(SYS_GUI_INPUT,(uintptr_t)&event,0,0) == 1) {
            if (event.kind == INPUT_KEY) { key_event(event.key,event.modifiers); changed = true; }
            else if (event.kind == INPUT_MOUSE) { mouse_x += event.dx; mouse_y += event.dy; if (mouse_x < 0) { mouse_x = 0; } if (mouse_x >= wm.width) { mouse_x = wm.width-1; } if (mouse_y < 0) { mouse_y = 0; } if (mouse_y >= wm.height) { mouse_y = wm.height-1; } bool pressed = (event.buttons & 1u) != 0; if (pressed && (buttons & 1u) == 0) { click(); } int dragging = wm.dragging; wm_drag(&wm,mouse_x,mouse_y,pressed); if (dragging >= 0 && !pressed) { trace("GUI drag completed"); } buttons = event.buttons; changed = true; }
        }
        bool monitor_open = false; for (unsigned int i = 0; i < WM_MAX; ++i) { if (wm.windows[i].live && wm.windows[i].app == APP_MONITOR) { monitor_open = true; } }
        static uint64_t last_refresh; struct system_info clock; if (monitor_open && user_call(SYS_SYSTEM,(uintptr_t)&clock,0,0) == 0 && clock.ticks-last_refresh >= 100) { monitor_refresh(); last_refresh = clock.ticks; changed = true; }
        if (changed) { render(); } (void)user_call(SYS_SLEEP,1,0,0);
    }
    return failed ? 5 : 0;
}
