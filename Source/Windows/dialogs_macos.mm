#include "fizmo_library.hpp"
#include "dialogs.hpp"

#ifdef OS_MACOS

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <string>
#include <vector>

namespace {

NSString* ns_text(const std::string& s) {
    NSString* out = [[NSString alloc] initWithBytes:s.data() length:s.size() encoding:NSUTF8StringEncoding];
    return out ? out : @"";
}

std::string std_text(NSString* s) {
    if (!s) return {};
    const char* utf8 = [s UTF8String];
    return utf8 ? std::string(utf8) : std::string();
}

void run_on_main(void (^work)(void)) {
    if ([NSThread isMainThread]) work();
    else dispatch_sync(dispatch_get_main_queue(), work);
}

void ensure_app() {
    [NSApplication sharedApplication];
    if ([NSApp activationPolicy] == NSApplicationActivationPolicyProhibited) [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

NSArray<UTType*>* content_types(const std::vector<fizmo::windows::dialogs::FileFilter>& filters) API_AVAILABLE(macos(11.0)) {
    NSMutableArray<UTType*>* types = [NSMutableArray array];
    for (const auto& f : filters) {
        for (const std::string& pattern : f.patterns) {
            if (pattern == "*" || pattern == "*.*") return nil;
            const std::size_t dot = pattern.rfind('.');
            if (dot == std::string::npos || dot + 1 >= pattern.size()) continue;
            UTType* type = [UTType typeWithFilenameExtension:ns_text(pattern.substr(dot + 1))];
            if (type) [types addObject:type];
        }
    }
    return [types count] ? types : nil;
}

void apply_common(NSSavePanel* panel, const fizmo::windows::dialogs::FileDialogOptions& o) {
    if (!o.title.empty()) {
        [panel setTitle:ns_text(o.title)];
        [panel setMessage:ns_text(o.title)];
    }
    if (!o.default_path.empty()) {
        BOOL dir = NO;
        NSString* path = ns_text(o.default_path);
        if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&dir]) {
            [panel setDirectoryURL:[NSURL fileURLWithPath:(dir ? path : [path stringByDeletingLastPathComponent]) isDirectory:YES]];
        }
    }
    if (!o.filters.empty()) {
        if (@available(macOS 11.0, *)) {
            NSArray<UTType*>* types = content_types(o.filters);
            if (types) [panel setAllowedContentTypes:types];
        }
    }
    [panel setCanCreateDirectories:YES];
}

} // namespace

namespace fizmo {
namespace windows {
namespace dialogs {
namespace detail {

std::vector<std::string> macos_file_dialog(const FileDialogOptions& o, int kind) {
    __block std::vector<std::string> out;
    run_on_main(^{
        @autoreleasepool {
            ensure_app();
            if (kind == 1) {
                NSSavePanel* panel = [NSSavePanel savePanel];
                apply_common(panel, o);
                if (!o.default_name.empty()) [panel setNameFieldStringValue:ns_text(o.default_name)];
                if ([panel runModal] == NSModalResponseOK && [panel URL]) out.push_back(std_text([[panel URL] path]));
                return;
            }
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            apply_common(panel, o);
            [panel setCanChooseFiles:kind == 2 ? NO : YES];
            [panel setCanChooseDirectories:kind == 2 ? YES : NO];
            [panel setAllowsMultipleSelection:(kind == 0 && o.multiple) ? YES : NO];
            if ([panel runModal] != NSModalResponseOK) return;
            for (NSURL* url in [panel URLs]) out.push_back(std_text([url path]));
        }
    });
    return out;
}

MessageResult macos_message_box(const std::string& title, const std::string& message, MessageButtons buttons, MessageIcon icon, void* parent) {
    (void)parent;
    __block MessageResult result = MessageResult::Cancel;
    run_on_main(^{
        @autoreleasepool {
            ensure_app();
            NSAlert* alert = [[NSAlert alloc] init];
            [alert setMessageText:ns_text(title)];
            [alert setInformativeText:ns_text(message)];
            [alert setAlertStyle:(icon == MessageIcon::Warning || icon == MessageIcon::Error) ? NSAlertStyleCritical : NSAlertStyleInformational];
            std::vector<MessageResult> map;
            auto add = [&](NSString* label, MessageResult r) {
                [alert addButtonWithTitle:label];
                map.push_back(r);
            };
            switch (buttons) {
                case MessageButtons::Ok:          add(@"OK", MessageResult::Ok); break;
                case MessageButtons::OkCancel:    add(@"OK", MessageResult::Ok); add(@"Cancel", MessageResult::Cancel); break;
                case MessageButtons::YesNo:       add(@"Yes", MessageResult::Yes); add(@"No", MessageResult::No); break;
                case MessageButtons::YesNoCancel: add(@"Yes", MessageResult::Yes); add(@"No", MessageResult::No); add(@"Cancel", MessageResult::Cancel); break;
                case MessageButtons::RetryCancel: add(@"Retry", MessageResult::Retry); add(@"Cancel", MessageResult::Cancel); break;
            }
            [NSApp activateIgnoringOtherApps:YES];
            const NSModalResponse r = [alert runModal];
            const NSInteger index = r - NSAlertFirstButtonReturn;
            if (index >= 0 && static_cast<std::size_t>(index) < map.size()) result = map[static_cast<std::size_t>(index)];
            else result = buttons == MessageButtons::Ok ? MessageResult::Ok : MessageResult::Cancel;
        }
    });
    return result;
}

} // namespace detail
} // namespace dialogs
} // namespace windows
} // namespace fizmo

#endif // OS_MACOS
