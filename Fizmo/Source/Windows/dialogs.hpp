#ifndef FIZMO_DIALOGS_HPP
#define FIZMO_DIALOGS_HPP

#include "../Basic/basic_includes.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>


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
