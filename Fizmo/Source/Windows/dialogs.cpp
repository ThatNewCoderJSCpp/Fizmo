#define ALL_FIZMO
#include <fizmo/includes.hpp>
#include "dialogs.hpp"

namespace fizmo {
namespace windows {
namespace dialogs {
namespace detail {

std::string join_patterns(const FileFilter& f, const char* sep) {
    std::string out;
    for (std::size_t i = 0; i < f.patterns.size(); ++i) {
        if (i) out += sep;
        out += f.patterns[i];
    }
    return out.empty() ? std::string("*") : out;
}

std::vector<std::string> split_lines(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur.push_back(c);
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

#if defined(OS_LINUX)
bool find_program(const std::string& name, std::string& out) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string p = path;
    std::size_t start = 0;

    while (start <= p.size()) {
        std::size_t end = p.find(':', start);
        if (end == std::string::npos) end = p.size();
        const std::string dir = p.substr(start, end - start);
        if (!dir.empty()) {
            const std::string full = dir + "/" + name;
            struct stat st;
            if (::stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode) && ::access(full.c_str(), X_OK) == 0) { out = full; return true; }
        }
        start = end + 1;
    }

    return false;
}
#endif

#if defined(OS_LINUX)
int run_capture(const std::string& program, const std::vector<std::string>& args, std::string& output) {
    output.clear();
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) return -1;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fds[1], 1);
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int rc = posix_spawn(&pid, program.c_str(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    ::close(fds[1]);

    if (rc != 0) { ::close(fds[0]); return -1; }

    char buf[4096];
    for (;;) {
        const ssize_t n = ::read(fds[0], buf, sizeof(buf));
        if (n > 0) { output.append(buf, static_cast<std::size_t>(n)); continue; }
        if (n < 0 && errno == EINTR) continue;
        break;
    }

    ::close(fds[0]);
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

#if defined(OS_LINUX)
Backend linux_backend(std::string& program) {
    const char* force = std::getenv("FIZMO_DIALOG");
    const std::string want = force ? force : "";
    if (want == "x11") return Backend::X11;
    const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
    const bool kde = desktop && std::strstr(desktop, "KDE");
    if (want != "zenity" && (kde || want == "kdialog") && find_program("kdialog", program)) return Backend::KDialog;
    if (want != "kdialog" && find_program("zenity", program)) return Backend::Zenity;
    if (find_program("kdialog", program)) return Backend::KDialog;
    return Backend::X11;
}
#endif

#if defined(OS_LINUX)
std::vector<std::string> zenity_file_args(const FileDialogOptions& o, int kind) {
    std::vector<std::string> a{ "--file-selection" };
    if (kind == 1) a.push_back("--save");
    if (kind == 2) a.push_back("--directory");
    if (kind == 1 && o.confirm_overwrite) a.push_back("--confirm-overwrite");
    if (kind == 0 && o.multiple) { a.push_back("--multiple"); a.push_back("--separator=\n"); }
    if (!o.title.empty()) a.push_back("--title=" + o.title);
    std::string start = o.default_path;
    if (!o.default_name.empty()) start = (start.empty() ? std::string() : (start.back() == '/' ? start : start + "/")) + o.default_name;
    else if (!start.empty() && start.back() != '/') start += "/";
    if (!start.empty()) a.push_back("--filename=" + start);
    if (kind != 2) for (const FileFilter& f : o.filters) a.push_back("--file-filter=" + (f.name.empty() ? std::string() : f.name + " | ") + join_patterns(f, " "));
    return a;
}
#endif

#if defined(OS_LINUX)
std::vector<std::string> kdialog_file_args(const FileDialogOptions& o, int kind) {
    std::vector<std::string> a;
    if (kind == 0) a.push_back("--getopenfilename");
    else if (kind == 1) a.push_back("--getsavefilename");
    else a.push_back("--getexistingdirectory");
    std::string start = o.default_path.empty() ? std::string(".") : o.default_path;
    if (!o.default_name.empty()) start += (start.back() == '/' ? "" : "/") + o.default_name;
    a.push_back(start);

    if (kind != 2 && !o.filters.empty()) {
        std::string filter;
        for (std::size_t i = 0; i < o.filters.size(); ++i) {
            if (i) filter += "\n";
            filter += (o.filters[i].name.empty() ? std::string("Files") : o.filters[i].name) + " (" + join_patterns(o.filters[i], " ") + ")";
        }
        a.push_back(filter);
    }

    if (kind == 0 && o.multiple) { a.push_back("--multiple"); a.push_back("--separate-output"); }
    if (!o.title.empty()) { a.push_back("--title"); a.push_back(o.title); }
    return a;
}
#endif

#if defined(OS_LINUX)
std::vector<std::string> run_file_dialog(const FileDialogOptions& o, int kind) {
    std::string program;
    const Backend b = linux_backend(program);
    if (b == Backend::X11) return {};
    std::string out;
    const int rc = run_capture(program, b == Backend::Zenity ? zenity_file_args(o, kind) : kdialog_file_args(o, kind), out);
    if (rc != 0) return {};
    std::vector<std::string> paths = split_lines(out);
    if (!(kind == 0 && o.multiple) && paths.size() > 1) paths.resize(1);
    return paths;
}
#endif

#if defined(OS_LINUX)
auto X11Box::text_width(const std::string& s) const -> int { return font ? XTextWidth(font, s.c_str(), static_cast<int>(s.size())) : static_cast<int>(s.size()) * 7; }
#endif

#if defined(OS_LINUX)
auto X11Box::layout() -> void {
        int w = 260;
        for (const std::string& l : lines) w = std::max(w, text_width(l) + 40);
        int bw = 0;
        for (auto& b : buttons) bw += std::max(80, text_width(b.first) + 24) + 10;
        w = std::max(w, bw + 30);
        width = std::min(w, 900);
        line_h = font ? font->ascent + font->descent + 4 : 14;
        height = 30 + static_cast<int>(lines.size()) * line_h + 60;
        rects.clear();
        int x = width - 20;
        for (std::size_t i = buttons.size(); i-- > 0;) {
            const int w2 = std::max(80, text_width(buttons[i].first) + 24);
            x -= w2;
            rects.insert(rects.begin(), ::XRectangle{ static_cast<short>(x), static_cast<short>(height - 46), static_cast<unsigned short>(w2), 28 });
            x -= 10;
        }
    }
#endif

#if defined(OS_LINUX)
auto X11Box::rgb(int r, int g, int b) const -> unsigned long {
        XColor c{};
        c.red = static_cast<unsigned short>(r * 257);
        c.green = static_cast<unsigned short>(g * 257);
        c.blue = static_cast<unsigned short>(b * 257);
        c.flags = DoRed | DoGreen | DoBlue;
        XAllocColor(dpy, DefaultColormap(dpy, DefaultScreen(dpy)), &c);
        return c.pixel;
    }
#endif

#if defined(OS_LINUX)
auto X11Box::draw() -> void {
        XSetForeground(dpy, gc, rgb(242, 242, 242));
        XFillRectangle(dpy, win, gc, 0, 0, static_cast<unsigned int>(width), static_cast<unsigned int>(height));
        XSetForeground(dpy, gc, rgb(20, 20, 20));
        const int ascent = font ? font->ascent : 11;
        for (std::size_t i = 0; i < lines.size(); ++i)
            XDrawString(dpy, win, gc, 20, 24 + static_cast<int>(i) * line_h + ascent, lines[i].c_str(), static_cast<int>(lines[i].size()));

        for (std::size_t i = 0; i < rects.size(); ++i) {
            const ::XRectangle& r = rects[i];
            const bool active = static_cast<int>(i) == pressed;
            XSetForeground(dpy, gc, active ? rgb(190, 205, 230) : (static_cast<int>(i) == hover ? rgb(225, 232, 245) : rgb(255, 255, 255)));
            XFillRectangle(dpy, win, gc, r.x, r.y, r.width, r.height);
            XSetForeground(dpy, gc, static_cast<int>(i) == focus ? rgb(60, 110, 200) : rgb(160, 160, 160));
            XDrawRectangle(dpy, win, gc, r.x, r.y, r.width - 1u, r.height - 1u);
            XSetForeground(dpy, gc, rgb(20, 20, 20));
            const std::string& t = buttons[i].first;
            XDrawString(dpy, win, gc, r.x + (r.width - text_width(t)) / 2, r.y + (r.height + ascent) / 2 - 1, t.c_str(), static_cast<int>(t.size()));
        }

        XFlush(dpy);
    }
#endif

#if defined(OS_LINUX)
auto X11Box::hit(int x, int y) const -> int {
        for (std::size_t i = 0; i < rects.size(); ++i) {
            const ::XRectangle& r = rects[i];
            if (x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height) return static_cast<int>(i);
        }
        return -1;
    }
#endif

#if defined(OS_LINUX)
std::vector<std::pair<std::string, MessageResult>> button_set(MessageButtons b) {
    switch (b) {
        case MessageButtons::OkCancel:    return { { "OK", MessageResult::Ok }, { "Cancel", MessageResult::Cancel } };
        case MessageButtons::YesNo:       return { { "Yes", MessageResult::Yes }, { "No", MessageResult::No } };
        case MessageButtons::YesNoCancel: return { { "Yes", MessageResult::Yes }, { "No", MessageResult::No }, { "Cancel", MessageResult::Cancel } };
        case MessageButtons::RetryCancel: return { { "Retry", MessageResult::Retry }, { "Cancel", MessageResult::Cancel } };
        default:                          return { { "OK", MessageResult::Ok } };
    }
}
#endif

#if defined(OS_LINUX)
MessageResult cancel_result(MessageButtons b) {
    switch (b) {
        case MessageButtons::Ok:    return MessageResult::Ok;
        case MessageButtons::YesNo: return MessageResult::No;
        default:                    return MessageResult::Cancel;
    }
}
#endif

#if defined(OS_LINUX)
MessageResult x11_message_box(const std::string& title, const std::string& message, MessageButtons buttons, void* parent, int auto_click_ms) {
    X11Box box;
    box.dpy = XOpenDisplay(nullptr);
    if (!box.dpy) return cancel_result(buttons);
    box.lines = split_lines(message);
    if (box.lines.empty()) box.lines.push_back(std::string());
    box.buttons = button_set(buttons);
    box.font = XLoadQueryFont(box.dpy, "-*-helvetica-medium-r-normal--12-*-*-*-*-*-iso10646-1");
    if (!box.font) box.font = XLoadQueryFont(box.dpy, "fixed");
    box.layout();
    const int screen = DefaultScreen(box.dpy);
    const int sx = (DisplayWidth(box.dpy, screen) - box.width) / 2, sy = (DisplayHeight(box.dpy, screen) - box.height) / 3;
    box.win = XCreateSimpleWindow(box.dpy, RootWindow(box.dpy, screen), sx, sy, static_cast<unsigned int>(box.width), static_cast<unsigned int>(box.height), 1, BlackPixel(box.dpy, screen), WhitePixel(box.dpy, screen));
    XSelectInput(box.dpy, box.win, ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | StructureNotifyMask);
    XStoreName(box.dpy, box.win, title.c_str());
    const ::Atom utf8 = XInternAtom(box.dpy, "UTF8_STRING", 0);
    const ::Atom net_name = XInternAtom(box.dpy, "_NET_WM_NAME", 0);
    XChangeProperty(box.dpy, box.win, net_name, utf8, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(title.c_str()), static_cast<int>(title.size()));
    ::Atom wm_delete = XInternAtom(box.dpy, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(box.dpy, box.win, &wm_delete, 1);
    const ::Atom type = XInternAtom(box.dpy, "_NET_WM_WINDOW_TYPE", 0);
    const ::Atom dialog = XInternAtom(box.dpy, "_NET_WM_WINDOW_TYPE_DIALOG", 0);
    XChangeProperty(box.dpy, box.win, type, XA_ATOM, 32, PropModeReplace, reinterpret_cast<const unsigned char*>(&dialog), 1);
    if (parent) XSetTransientForHint(box.dpy, box.win, static_cast<const x11::Handle*>(parent)->window);
    XSizeHints hints{};
    hints.flags = PMinSize | PMaxSize | PPosition;
    hints.x = sx;
    hints.y = sy;
    hints.min_width = hints.max_width = box.width;
    hints.min_height = hints.max_height = box.height;
    XSetWMNormalHints(box.dpy, box.win, &hints);
    box.gc = XCreateGC(box.dpy, box.win, 0, nullptr);
    if (box.font) XSetFont(box.dpy, box.gc, box.font->fid);
    XMapRaised(box.dpy, box.win);
    XFlush(box.dpy);

    MessageResult result = cancel_result(buttons);
    bool done = false;
    const auto start = std::chrono::steady_clock::now();

    while (!done) {
        if (auto_click_ms >= 0 && std::chrono::steady_clock::now() - start > std::chrono::milliseconds(auto_click_ms)) {
            result = box.buttons[static_cast<std::size_t>(box.focus)].second;
            break;
        }

        if (!XPending(box.dpy)) {
            if (auto_click_ms >= 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
            XEvent wait;
            XPeekEvent(box.dpy, &wait);
        }

        XEvent ev;
        XNextEvent(box.dpy, &ev);

        switch (ev.type) {
            case x11::kExpose:
                if (ev.xexpose.count == 0) box.draw();
                break;
            case x11::kMotionNotify: {
                const int h = box.hit(ev.xmotion.x, ev.xmotion.y);
                if (h != box.hover) { box.hover = h; box.draw(); }
                break;
            }
            case x11::kButtonPress:
                if (ev.xbutton.button == 1) { box.pressed = box.hit(ev.xbutton.x, ev.xbutton.y); box.draw(); }
                break;
            case x11::kButtonRelease:
                if (ev.xbutton.button == 1) {
                    const int h = box.hit(ev.xbutton.x, ev.xbutton.y);
                    if (h >= 0 && h == box.pressed) { result = box.buttons[static_cast<std::size_t>(h)].second; done = true; }
                    box.pressed = -1;
                    box.draw();
                }
                break;
            case x11::kKeyPress: {
                const ::KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Escape) { result = cancel_result(buttons); done = true; }
                else if (ks == XK_Return || ks == XK_KP_Enter || ks == XK_space) { result = box.buttons[static_cast<std::size_t>(box.focus)].second; done = true; }
                else if (ks == XK_Tab || ks == XK_Right) { box.focus = (box.focus + 1) % static_cast<int>(box.buttons.size()); box.draw(); }
                else if (ks == XK_Left || ks == XK_ISO_Left_Tab) { box.focus = (box.focus + static_cast<int>(box.buttons.size()) - 1) % static_cast<int>(box.buttons.size()); box.draw(); }
                break;
            }
            case x11::kClientMessage:
                if (static_cast<::Atom>(ev.xclient.data.l[0]) == wm_delete) { result = cancel_result(buttons); done = true; }
                break;
            default:
                break;
        }
    }

    if (box.font) XFreeFont(box.dpy, box.font);
    XFreeGC(box.dpy, box.gc);
    XDestroyWindow(box.dpy, box.win);
    XCloseDisplay(box.dpy);
    return result;
}
#endif

#if defined(OS_LINUX)
MessageResult linux_message_box(const std::string& title, const std::string& message, MessageButtons buttons, MessageIcon icon, void* parent) {
    std::string program;
    const Backend b = linux_backend(program);
    const char* auto_env = std::getenv("FIZMO_DIALOG_AUTOCLICK_MS");
    if (b == Backend::X11 || auto_env) return x11_message_box(title, message, buttons, parent, auto_env ? std::atoi(auto_env) : -1);
    std::string out;
    std::vector<std::string> a;

    if (b == Backend::Zenity) {
        const char* kind = icon == MessageIcon::Error ? "--error" : (icon == MessageIcon::Warning ? "--warning" : "--info");
        if (buttons != MessageButtons::Ok) kind = "--question";
        a = { kind, "--title=" + title, "--text=" + message, "--no-markup" };
        if (buttons == MessageButtons::OkCancel) { a.push_back("--ok-label=OK"); a.push_back("--cancel-label=Cancel"); }
        if (buttons == MessageButtons::RetryCancel) { a.push_back("--ok-label=Retry"); a.push_back("--cancel-label=Cancel"); }
        if (buttons == MessageButtons::YesNo) { a.push_back("--ok-label=Yes"); a.push_back("--cancel-label=No"); }
        if (buttons == MessageButtons::YesNoCancel) { a.push_back("--ok-label=Yes"); a.push_back("--cancel-label=No"); a.push_back("--extra-button=Cancel"); }
        if (icon == MessageIcon::Warning && buttons != MessageButtons::Ok) a.push_back("--icon-name=dialog-warning");
        if (icon == MessageIcon::Error && buttons != MessageButtons::Ok) a.push_back("--icon-name=dialog-error");
        const int rc = run_capture(program, a, out);
        const bool extra = !split_lines(out).empty();
        switch (buttons) {
            case MessageButtons::Ok:          return MessageResult::Ok;
            case MessageButtons::OkCancel:    return rc == 0 ? MessageResult::Ok : MessageResult::Cancel;
            case MessageButtons::RetryCancel: return rc == 0 ? MessageResult::Retry : MessageResult::Cancel;
            case MessageButtons::YesNo:       return rc == 0 ? MessageResult::Yes : MessageResult::No;
            case MessageButtons::YesNoCancel: return rc == 0 ? MessageResult::Yes : (extra || rc != 1 ? MessageResult::Cancel : MessageResult::No);
        }
        return MessageResult::Cancel;
    }

    switch (buttons) {
        case MessageButtons::Ok:          a = { icon == MessageIcon::Error ? "--error" : (icon == MessageIcon::Warning ? "--sorry" : "--msgbox"), message }; break;
        case MessageButtons::OkCancel:    a = { "--warningcontinuecancel", message, "--continue-label", "OK" }; break;
        case MessageButtons::RetryCancel: a = { "--warningcontinuecancel", message, "--continue-label", "Retry" }; break;
        case MessageButtons::YesNo:       a = { icon == MessageIcon::Warning ? "--warningyesno" : "--yesno", message }; break;
        case MessageButtons::YesNoCancel: a = { icon == MessageIcon::Warning ? "--warningyesnocancel" : "--yesnocancel", message }; break;
    }

    a.push_back("--title");
    a.push_back(title);
    const int rc = run_capture(program, a, out);
    switch (buttons) {
        case MessageButtons::Ok:          return MessageResult::Ok;
        case MessageButtons::OkCancel:    return rc == 0 ? MessageResult::Ok : MessageResult::Cancel;
        case MessageButtons::RetryCancel: return rc == 0 ? MessageResult::Retry : MessageResult::Cancel;
        case MessageButtons::YesNo:       return rc == 0 ? MessageResult::Yes : MessageResult::No;
        case MessageButtons::YesNoCancel: return rc == 0 ? MessageResult::Yes : (rc == 1 ? MessageResult::No : MessageResult::Cancel);
    }
    return MessageResult::Cancel;
}
#endif

std::vector<std::string> file_dialog(const FileDialogOptions& o, int kind) {
#if defined(OS_WINDOWS)
    return windows_file_dialog(o, kind);
#elif defined(OS_LINUX)
    return run_file_dialog(o, kind);
#else
    (void)o; (void)kind;
    return {};
#endif
}

} // namespace detail
} // namespace dialogs
} // namespace windows
} // namespace fizmo

namespace fizmo {
namespace windows {
namespace dialogs {

Backend backend() {
#if defined(OS_WINDOWS)
    return Backend::Native;
#elif defined(OS_LINUX)
    std::string program;
    return detail::linux_backend(program);
#else
    return Backend::None;
#endif
}

bool file_dialogs_available() {
    const Backend b = backend();
    return b == Backend::Native || b == Backend::Zenity || b == Backend::KDialog;
}

std::optional<std::string> open_file(const FileDialogOptions& o) {
    FileDialogOptions single = o;
    single.multiple = false;
    std::vector<std::string> r = detail::file_dialog(single, 0);
    if (r.empty()) return std::nullopt;
    return r.front();
}

std::optional<std::string> save_file(const FileDialogOptions& o) {
    std::vector<std::string> r = detail::file_dialog(o, 1);
    if (r.empty()) return std::nullopt;
    return r.front();
}

std::optional<std::string> select_folder(const FileDialogOptions& o) {
    std::vector<std::string> r = detail::file_dialog(o, 2);
    if (r.empty()) return std::nullopt;
    return r.front();
}

MessageResult message_box(const std::string& title, const std::string& message, MessageButtons buttons, MessageIcon icon, void* parent) {
#if defined(OS_WINDOWS)
    return detail::windows_message_box(title, message, buttons, icon, parent);
#elif defined(OS_LINUX)
    return detail::linux_message_box(title, message, buttons, icon, parent);
#else
    (void)title; (void)message; (void)icon; (void)parent;
    return buttons == MessageButtons::Ok ? MessageResult::Ok : MessageResult::Cancel;
#endif
}

bool ask_yes_no(const std::string& title, const std::string& message, void* parent) {
    return message_box(title, message, MessageButtons::YesNo, MessageIcon::Question, parent) == MessageResult::Yes;
}

} // namespace dialogs
} // namespace windows
} // namespace fizmo
