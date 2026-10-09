#ifndef FIZMO_DIALOGS_HPP
#define FIZMO_DIALOGS_HPP

#include "../Basic/basic_includes.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

#if defined(OS_LINUX)
#include <fcntl.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#if defined(OS_WINDOWS)
#include <shobjidl.h>
#endif

namespace fizmo {
namespace windows {
namespace dialogs {

struct FileFilter {
    std::string              name;
    std::vector<std::string> patterns;
};

struct FileDialogOptions {
    std::string             title;
    std::string             default_path;
    std::string             default_name;
    std::vector<FileFilter> filters;
    bool                    multiple          = false;
    bool                    confirm_overwrite = true;
    void*                   parent            = nullptr;
};

enum class MessageIcon : std::uint8_t { Info = 0, Warning, Error, Question };
enum class MessageButtons : std::uint8_t { Ok = 0, OkCancel, YesNo, YesNoCancel, RetryCancel };
enum class MessageResult : std::uint8_t { Ok = 0, Cancel, Yes, No, Retry };

enum class Backend : std::uint8_t { None = 0, Native, Zenity, KDialog, X11 };

namespace detail {

 std::string join_patterns(const FileFilter& f, const char* sep);

 std::vector<std::string> split_lines(const std::string& s, char sep = '\n');

#if defined(OS_LINUX)

 bool find_program(const std::string& name, std::string& out);

 int run_capture(const std::string& program, const std::vector<std::string>& args, std::string& output);

 Backend linux_backend(std::string& program);

 std::vector<std::string> zenity_file_args(const FileDialogOptions& o, int kind);

 std::vector<std::string> kdialog_file_args(const FileDialogOptions& o, int kind);

 std::vector<std::string> run_file_dialog(const FileDialogOptions& o, int kind);

struct X11Box {
    ::Display* dpy = nullptr;
    ::Window   win = 0;
    ::GC       gc  = nullptr;
    XFontStruct* font = nullptr;
    std::vector<std::string> lines;
    std::vector<std::pair<std::string, MessageResult>> buttons;
    std::vector<::XRectangle> rects;
    int hover = -1, pressed = -1, focus = 0;
    int width = 0, height = 0, line_h = 14;

    int text_width(const std::string& s) const;

    void layout();

    unsigned long rgb(int r, int g, int b) const;

    void draw();

    int hit(int x, int y) const;
};

 std::vector<std::pair<std::string, MessageResult>> button_set(MessageButtons b);

 MessageResult cancel_result(MessageButtons b);

 MessageResult x11_message_box(const std::string& title, const std::string& message, MessageButtons buttons, void* parent, int auto_click_ms);

 MessageResult linux_message_box(const std::string& title, const std::string& message, MessageButtons buttons, MessageIcon icon, void* parent);

#endif

#if defined(OS_WINDOWS)

inline std::wstring wide(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(n > 0 ? n : 0), L'\0');
    if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &out[0], n);
    return out;
}

inline std::string utf8(const wchar_t* s) {
    if (!s) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, &out[0], n, nullptr, nullptr);
    return out;
}

inline std::string item_path(IShellItem* item) {
    if (!item) return {};
    PWSTR p = nullptr;
    std::string out;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) { out = utf8(p); CoTaskMemFree(p); }
    return out;
}

template <typename T>
struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** operator&() noexcept { return &p; }
    T* operator->() const noexcept { return p; }
    explicit operator bool() const noexcept { return p != nullptr; }
};

inline std::vector<std::string> windows_file_dialog(const FileDialogOptions& o, int kind) {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const bool uninit = SUCCEEDED(init);
    std::vector<std::string> out;

    {
        ComPtr<IFileDialog> dlg;
        const CLSID clsid = kind == 1 ? CLSID_FileSaveDialog : CLSID_FileOpenDialog;
        const IID iid = kind == 1 ? IID_IFileSaveDialog : IID_IFileOpenDialog;

        if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, iid, reinterpret_cast<void**>(&dlg)))) {
            FILEOPENDIALOGOPTIONS flags = 0;
            dlg->GetOptions(&flags);
            flags |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
            if (kind == 0) flags |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
            if (kind == 0 && o.multiple) flags |= FOS_ALLOWMULTISELECT;
            if (kind == 2) flags |= FOS_PICKFOLDERS;
            if (kind == 1) { if (o.confirm_overwrite) flags |= FOS_OVERWRITEPROMPT; else flags &= ~static_cast<FILEOPENDIALOGOPTIONS>(FOS_OVERWRITEPROMPT); }
            dlg->SetOptions(flags);
            if (!o.title.empty()) dlg->SetTitle(wide(o.title).c_str());
            if (!o.default_name.empty()) dlg->SetFileName(wide(o.default_name).c_str());

            std::vector<std::wstring> names, specs;
            std::vector<COMDLG_FILTERSPEC> spec;
            if (kind != 2) {
                for (const FileFilter& f : o.filters) {
                    names.push_back(wide(f.name.empty() ? join_patterns(f, ";") : f.name));
                    specs.push_back(wide(join_patterns(f, ";")));
                }
                for (std::size_t i = 0; i < names.size(); ++i) spec.push_back(COMDLG_FILTERSPEC{ names[i].c_str(), specs[i].c_str() });
                if (!spec.empty()) { dlg->SetFileTypes(static_cast<UINT>(spec.size()), spec.data()); dlg->SetFileTypeIndex(1); }
                if (kind == 1 && !o.filters.empty() && !o.filters[0].patterns.empty()) {
                    const std::string& p = o.filters[0].patterns[0];
                    const std::size_t dot = p.rfind('.');
                    if (dot != std::string::npos && p.find('*', dot) == std::string::npos) dlg->SetDefaultExtension(wide(p.substr(dot + 1)).c_str());
                }
            }

            if (!o.default_path.empty()) {
                ComPtr<IShellItem> folder;
                if (SUCCEEDED(SHCreateItemFromParsingName(wide(o.default_path).c_str(), nullptr, IID_IShellItem, reinterpret_cast<void**>(&folder)))) dlg->SetFolder(folder.p);
            }

            if (SUCCEEDED(dlg->Show(static_cast<HWND>(o.parent)))) {
                if (kind == 0) {
                    ComPtr<IShellItemArray> items;
                    if (SUCCEEDED(static_cast<IFileOpenDialog*>(dlg.p)->GetResults(&items))) {
                        DWORD count = 0;
                        items->GetCount(&count);
                        for (DWORD i = 0; i < count; ++i) {
                            ComPtr<IShellItem> item;
                            if (SUCCEEDED(items->GetItemAt(i, &item))) { const std::string s = item_path(item.p); if (!s.empty()) out.push_back(s); }
                        }
                    }
                } else {
                    ComPtr<IShellItem> item;
                    if (SUCCEEDED(dlg->GetResult(&item))) { const std::string s = item_path(item.p); if (!s.empty()) out.push_back(s); }
                }
            }
        }
    }

    if (uninit) CoUninitialize();
    return out;
}

inline MessageResult windows_message_box(const std::string& title, const std::string& message, MessageButtons buttons, MessageIcon icon, void* parent) {
    UINT flags = MB_SETFOREGROUND;
    switch (icon) {
        case MessageIcon::Warning:  flags |= MB_ICONWARNING; break;
        case MessageIcon::Error:    flags |= MB_ICONERROR; break;
        case MessageIcon::Question: flags |= MB_ICONQUESTION; break;
        default:                    flags |= MB_ICONINFORMATION; break;
    }
    switch (buttons) {
        case MessageButtons::OkCancel:    flags |= MB_OKCANCEL; break;
        case MessageButtons::YesNo:       flags |= MB_YESNO; break;
        case MessageButtons::YesNoCancel: flags |= MB_YESNOCANCEL; break;
        case MessageButtons::RetryCancel: flags |= MB_RETRYCANCEL; break;
        default:                          flags |= MB_OK; break;
    }
    switch (MessageBoxW(static_cast<HWND>(parent), wide(message).c_str(), wide(title).c_str(), flags)) {
        case IDOK:    return MessageResult::Ok;
        case IDYES:   return MessageResult::Yes;
        case IDNO:    return MessageResult::No;
        case IDRETRY: return MessageResult::Retry;
        default:      return buttons == MessageButtons::Ok ? MessageResult::Ok : (buttons == MessageButtons::YesNo ? MessageResult::No : MessageResult::Cancel);
    }
}

#endif

 std::vector<std::string> file_dialog(const FileDialogOptions& o, int kind);

} // namespace detail

 Backend backend();

 bool file_dialogs_available();

 std::optional<std::string> open_file(const FileDialogOptions& o = {});

inline std::vector<std::string> open_files(const FileDialogOptions& o = {}) {
    FileDialogOptions multi = o;
    multi.multiple = true;
    return detail::file_dialog(multi, 0);
}

 std::optional<std::string> save_file(const FileDialogOptions& o = {});

 std::optional<std::string> select_folder(const FileDialogOptions& o = {});

 MessageResult message_box(const std::string& title, const std::string& message, MessageButtons buttons = MessageButtons::Ok, MessageIcon icon = MessageIcon::Info, void* parent = nullptr);

inline void show_info(const std::string& title, const std::string& message, void* parent = nullptr) { message_box(title, message, MessageButtons::Ok, MessageIcon::Info, parent); }
inline void show_warning(const std::string& title, const std::string& message, void* parent = nullptr) { message_box(title, message, MessageButtons::Ok, MessageIcon::Warning, parent); }
inline void show_error(const std::string& title, const std::string& message, void* parent = nullptr) { message_box(title, message, MessageButtons::Ok, MessageIcon::Error, parent); }

 bool ask_yes_no(const std::string& title, const std::string& message, void* parent = nullptr);

} // namespace dialogs
} // namespace windows
} // namespace fizmo

#endif // FIZMO_DIALOGS_HPP
