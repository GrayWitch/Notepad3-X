// encoding: UTF-8
#include "MarkdownPreview.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include <richedit.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "md4c/md4c.h"
extern "C" {
#include "PathLib.h"
#include "md4c/entity.h"
}

#pragma comment(lib, "windowscodecs.lib")

namespace {
using Microsoft::WRL::ComPtr;
constexpr size_t MaxRtf = 2 * 1024 * 1024;
constexpr DWORD MaxImageBytes = 384 * 1024;
constexpr unsigned MaxImagePixels = 2 * 1024 * 1024;
constexpr unsigned MaxImageAttempts = 8;
constexpr unsigned MaxTableColumns = 12;

void AppendUnicode(std::wstring& text, unsigned cp)
{
    if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
        cp = 0xfffd;
    }
    if (cp <= 0xffff) {
        text += static_cast<wchar_t>(cp);
    } else {
        cp -= 0x10000;
        text += static_cast<wchar_t>(0xd800 + (cp >> 10));
        text += static_cast<wchar_t>(0xdc00 + (cp & 0x3ff));
    }
}

std::wstring Decode(MD_TEXTTYPE type, std::string_view text)
{
    std::wstring result;
    if (type == MD_TEXT_NULLCHAR) {
        AppendUnicode(result, 0xfffd);
    } else if (type == MD_TEXT_ENTITY && text.size() > 2) {
        if (text[1] == '#') {
            unsigned cp = 0;
            bool const hex = text[2] == 'x' || text[2] == 'X';
            const char* const end = text.data() + text.size() - 1;
            auto const number = std::from_chars(text.data() + (hex ? 3 : 2), end, cp, hex ? 16 : 10);
            if (number.ptr == end && number.ec == std::errc{}) {
                AppendUnicode(result, cp);
                return result;
            }
        } else if (auto const* entity = entity_lookup(text.data(), text.size())) {
            AppendUnicode(result, entity->codepoints[0]);
            if (entity->codepoints[1]) {
                AppendUnicode(result, entity->codepoints[1]);
            }
            return result;
        }
    }
    if (result.empty() && !text.empty()) {
        result.resize(text.size());
        int const length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                               result.data(), static_cast<int>(result.size()));
        result.resize(length);
    }
    return result;
}

std::wstring ImagePath(const MD_ATTRIBUTE& src)
{
    if (src.size > 2048) {
        return {};
    }
    std::wstring path;
    for (MD_SIZE i = 0; src.substr_offsets && src.substr_offsets[i] < src.size; ++i) {
        MD_OFFSET const offset = src.substr_offsets[i];
        path += Decode(src.substr_types[i], {src.text + offset, src.substr_offsets[i + 1] - offset});
    }
    // Allow only relative local paths, not URI schemes, absolute paths, ADS or devices.
    if (path.empty() || path.front() == L'/' || path.front() == L'\\' ||
        path.find_first_of(L":<>|?*\"") != path.npos ||
        std::any_of(path.begin(), path.end(), [](wchar_t ch) { return ch < L' '; })) {
        return {};
    }
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return path;
}

std::vector<BYTE> ReadLocalImage(HPATHL source, const std::wstring& relative)
{
    if (relative.empty() || Path_IsEmpty(source)) {
        return {};
    }
    using Path = std::unique_ptr<std::remove_pointer_t<HPATHL>, decltype(&Path_Release)>;
    Path path(Path_Copy(source), Path_Release);
    if (!path || !Path_RemoveFileSpec(path.get()) || !Path_Append(path.get(), relative.c_str())) {
        return {};
    }
    std::wstring full(Path_Get(path.get()));
    if (full.starts_with(L"\\\\?\\")) {
        full.erase(0, 4);
    }
    if (full.size() < 4 || full[1] != L':' || full[2] != L'\\') {
        return {};
    }
    wchar_t root[] = {full[0], L':', L'\\', L'\0'};
    UINT const driveType = GetDriveTypeW(root);
    if (driveType != DRIVE_FIXED && driveType != DRIVE_REMOVABLE) {
        return {};
    }
    LPCWSTR const ext = Path_FindExtension(path.get());
    if (!ext || (CompareStringOrdinal(ext, -1, L".png", -1, TRUE) != CSTR_EQUAL &&
                 CompareStringOrdinal(ext, -1, L".jpg", -1, TRUE) != CSTR_EQUAL &&
                 CompareStringOrdinal(ext, -1, L".jpeg", -1, TRUE) != CSTR_EQUAL)) {
        return {};
    }

    // Skip junctions/symlinks and offline/cloud files in the image path.
    Path prefix(Path_Allocate(nullptr), Path_Release);
    if (!prefix) {
        return {};
    }
    for (size_t end = 3;; ++end) {
        end = full.find(L'\\', end);
        bool const leaf = end == full.npos;
        if (!leaf) {
            Path_Reset(prefix.get(), full.substr(0, end).c_str());
        }
        DWORD const attributes = Path_GetFileAttributes(leaf ? path.get() : prefix.get());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_OFFLINE)) ||
            (leaf == ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0))) {
            return {};
        }
        if (leaf) {
            break;
        }
    }

    HANDLE const file = Path_CreateFile(path.get(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }
    std::unique_ptr<void, decltype(&CloseHandle)> handle(file, CloseHandle);
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > MaxImageBytes) {
        return {};
    }
    std::vector<BYTE> bytes(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) || read != bytes.size()) {
        return {};
    }
    return bytes;
}

struct List {
    unsigned next;
    bool ordered;
    bool pending = false;
    char task = 0;
};

struct Renderer {
    std::string rtf;
    std::vector<List> lists;
    ComPtr<IWICImagingFactory> imaging;
    HPATHL source;
    int width;
    int quotes = 0;
    unsigned columns = 0;
    unsigned imageDepth = 0;
    unsigned imageAttempts = 0;
    unsigned remainingImagePixels = MaxImagePixels;
    bool imageShown = false;
    bool paragraph = false;
    bool cell = false;
    bool codeBlock = false;
    bool codeBreak = false;

    Renderer(HPATHL path, int widthTwips, bool dark) : source(path), width(widthTwips)
    {
        rtf = R"({\rtf1\ansi\uc1\deff0{\fonttbl{\f0 Segoe UI;}{\f1 Consolas;}})";
        rtf += dark ? R"({\colortbl;\red230\green232\blue235;\red142\green190\blue255;\red173\green179\blue188;\red49\green53\blue60;})"
                    : R"({\colortbl;\red32\green36\blue42;\red22\green83\blue154;\red84\green95\blue108;\red239\green242\blue246;})";
    }

    void Escape(std::wstring_view text)
    {
        for (wchar_t ch : text) {
            if (ch == L'\r') {
                continue;
            }
            if (codeBreak) {
                rtf += "\\line ";
                codeBreak = false;
            }
            switch (ch) {
            case L'\\': case L'{': case L'}':
                rtf += '\\';
                rtf += static_cast<char>(ch);
                break;
            case L'\t': rtf += "\\tab "; break;
            case L'\n':
                if (codeBlock) {
                    codeBreak = true; // suppress the final newline emitted by MD4C
                } else {
                    rtf += "\\line ";
                }
                break;
            default:
                if (ch >= 32 && ch <= 126) {
                    rtf += static_cast<char>(ch);
                } else {
                    rtf += "\\u" + std::to_string(static_cast<short>(ch)) + '?';
                }
                break;
            }
        }
    }

    void EndParagraph()
    {
        if (paragraph && !cell) {
            rtf += "\\par}\n";
            paragraph = false;
        }
    }

    void StartParagraph(const char* style = "\\sa110\\sl260\\slmult1")
    {
        if (cell) {
            return;
        }
        EndParagraph();
        paragraph = true;
        rtf += "{\\pard\\plain\\f0\\fs22\\cf";
        rtf += quotes ? '3' : '1';
        rtf += style;
        int const indent = std::min(width / 3, static_cast<int>(lists.size()) * 300 + quotes * 220);
        if (indent) {
            rtf += "\\li" + std::to_string(indent);
        }
        if (!lists.empty() && lists.back().pending) {
            auto& list = lists.back();
            list.pending = false;
            rtf += "\\fi-240 ";
            if (list.task) {
                Escape(list.task == ' ' ? L"☐ " : L"☑ ");
            } else if (list.ordered) {
                rtf += std::to_string(list.next) + ". ";
            } else {
                Escape(L"• ");
            }
            if (list.ordered) {
                ++list.next;
            }
        } else {
            rtf += ' ';
        }
    }

    void EnsureParagraph()
    {
        if (!paragraph && !cell) {
            StartParagraph(lists.empty() ? "\\sa110\\sl260\\slmult1" : "\\sa40");
        }
    }

    bool Picture(const MD_ATTRIBUTE& src)
    {
        if (imageAttempts >= MaxImageAttempts || rtf.size() >= MaxRtf / 2) {
            return false;
        }
        ++imageAttempts;
        auto bytes = ReadLocalImage(source, ImagePath(src));
        if (bytes.empty()) {
            return false;
        }
        if (!imaging && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                IID_PPV_ARGS(imaging.ReleaseAndGetAddressOf())))) {
            return false;
        }
        bool const png = bytes.size() >= 8 && memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0;
        bool const jpg = bytes.size() >= 3 && memcmp(bytes.data(), "\xff\xd8\xff", 3) == 0;
        if (!png && !jpg) {
            return false;
        }
        ComPtr<IWICStream> stream;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        UINT x = 0, y = 0;
        if (FAILED(imaging->CreateStream(&stream)) ||
            FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size()))) ||
            FAILED(CoCreateInstance(png ? CLSID_WICPngDecoder : CLSID_WICJpegDecoder, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(decoder.GetAddressOf()))) ||
            FAILED(decoder->Initialize(stream.Get(), WICDecodeMetadataCacheOnDemand)) ||
            FAILED(decoder->GetFrame(0, &frame)) || FAILED(frame->GetSize(&x, &y)) ||
            !x || !y || x > 4096 || y > 4096 || x > remainingImagePixels / y) {
            return false;
        }
        remainingImagePixels -= x * y;
        int const available = cell ? width / static_cast<int>(columns) - 180 : width - 440;
        int const goalX = std::min({static_cast<int>(x * 15), std::max(60, available), static_cast<int>(7200 * x / y)});
        int const goalY = std::max(1, MulDiv(goalX, y, x));
        rtf += png ? "{\\pict\\pngblip" : "{\\pict\\jpegblip";
        rtf += "\\picw" + std::to_string(x) + "\\pich" + std::to_string(y) +
               "\\picwgoal" + std::to_string(std::max(1, goalX)) + "\\pichgoal" + std::to_string(goalY) + '\n';
        constexpr char hex[] = "0123456789abcdef";
        rtf.reserve(rtf.size() + bytes.size() * 2 + 1);
        for (BYTE value : bytes) {
            rtf += hex[value >> 4];
            rtf += hex[value & 15];
        }
        rtf += '}';
        return true;
    }

    void EnterBlock(MD_BLOCKTYPE type, const void* detail)
    {
        switch (type) {
        case MD_BLOCK_QUOTE: EndParagraph(); ++quotes; break;
        case MD_BLOCK_UL: EndParagraph(); lists.push_back({0, false}); break;
        case MD_BLOCK_OL:
            EndParagraph();
            lists.push_back({static_cast<const MD_BLOCK_OL_DETAIL*>(detail)->start, true});
            break;
        case MD_BLOCK_LI: {
            auto const& item = *static_cast<const MD_BLOCK_LI_DETAIL*>(detail);
            lists.back().pending = true;
            lists.back().task = item.is_task ? item.task_mark : 0;
            break;
        }
        case MD_BLOCK_P: StartParagraph(); break;
        case MD_BLOCK_H: {
            constexpr int sizes[] = {40, 33, 28, 25, 23, 22};
            auto const level = static_cast<const MD_BLOCK_H_DETAIL*>(detail)->level;
            StartParagraph("\\sb180\\sa120\\keepn");
            rtf += "\\b\\cf2\\fs" + std::to_string(sizes[level - 1]) + ' ';
            break;
        }
        case MD_BLOCK_HR:
            StartParagraph("\\sa90\\brdrb\\brdrs\\brdrw12\\brdrcf3");
            EndParagraph();
            break;
        case MD_BLOCK_CODE:
            StartParagraph("\\sb60\\sa140\\shading10000\\cbpat4");
            rtf += "\\f1\\fs20 ";
            codeBlock = true;
            break;
        case MD_BLOCK_TABLE:
            EndParagraph();
            columns = static_cast<const MD_BLOCK_TABLE_DETAIL*>(detail)->col_count;
            break;
        case MD_BLOCK_TR:
            if (columns <= MaxTableColumns) {
                rtf += "\\trowd\\trgaph75\\trleft0";
                for (unsigned i = 1; i <= columns; ++i) {
                    rtf += "\\clbrdrb\\brdrs\\brdrw8\\brdrcf4\\cellx" + std::to_string(width * i / columns);
                }
                rtf += '\n';
            }
            break;
        case MD_BLOCK_TH: case MD_BLOCK_TD: {
            auto const align = static_cast<const MD_BLOCK_TD_DETAIL*>(detail)->align;
            if (columns <= MaxTableColumns) {
                cell = true;
                rtf += "\\pard\\intbl\\plain\\f0\\fs20\\cf1";
                rtf += align == MD_ALIGN_RIGHT ? "\\qr" : (align == MD_ALIGN_CENTER ? "\\qc" : "\\ql");
                rtf += type == MD_BLOCK_TH ? "\\b\\cf2 " : " ";
            } else {
                EnsureParagraph();
            }
            break;
        }
        default: break;
        }
    }

    void LeaveBlock(MD_BLOCKTYPE type)
    {
        switch (type) {
        case MD_BLOCK_QUOTE: EndParagraph(); --quotes; break;
        case MD_BLOCK_UL: case MD_BLOCK_OL: EndParagraph(); lists.pop_back(); break;
        case MD_BLOCK_LI:
            if (lists.back().pending) { EnsureParagraph(); }
            EndParagraph();
            break;
        case MD_BLOCK_CODE: codeBlock = codeBreak = false; EndParagraph(); break;
        case MD_BLOCK_H: case MD_BLOCK_P: case MD_BLOCK_DOC: EndParagraph(); break;
        case MD_BLOCK_TH: case MD_BLOCK_TD:
            rtf += cell ? "\\cell\n" : "\\tab ";
            cell = false;
            break;
        case MD_BLOCK_TR:
            if (columns <= MaxTableColumns) {
                rtf += "\\row\\pard\n";
            } else {
                EndParagraph();
            }
            break;
        case MD_BLOCK_TABLE: columns = 0; break;
        default: break;
        }
    }

    void EnterSpan(MD_SPANTYPE type, const void* detail)
    {
        EnsureParagraph();
        if (imageDepth) {
            imageDepth += (type == MD_SPAN_IMG);
            return;
        }
        switch (type) {
        case MD_SPAN_EM: rtf += "{\\i "; break;
        case MD_SPAN_STRONG: rtf += "{\\b "; break;
        case MD_SPAN_DEL: rtf += "{\\strike "; break;
        case MD_SPAN_A: rtf += "{\\ul\\cf2 "; break;
        case MD_SPAN_CODE: rtf += "{\\f1\\highlight4 "; break;
        case MD_SPAN_IMG:
            imageDepth = 1;
            imageShown = Picture(static_cast<const MD_SPAN_IMG_DETAIL*>(detail)->src);
            if (!imageShown) {
                rtf += "{\\i\\cf3 [Image: ";
            }
            break;
        default: break;
        }
    }

    void LeaveSpan(MD_SPANTYPE type)
    {
        if (imageDepth) {
            if (type == MD_SPAN_IMG && !--imageDepth && !imageShown) {
                rtf += "]}";
            }
            return;
        }
        switch (type) {
        case MD_SPAN_EM: case MD_SPAN_STRONG: case MD_SPAN_DEL: case MD_SPAN_A: case MD_SPAN_CODE:
            rtf += '}';
            break;
        default: break;
        }
    }

    void Text(MD_TEXTTYPE type, std::string_view text)
    {
        if (imageDepth && imageShown) {
            return;
        }
        EnsureParagraph();
        if (type == MD_TEXT_BR) {
            rtf += "\\line ";
        } else if (type == MD_TEXT_SOFTBR) {
            rtf += ' ';
        } else {
            Escape(Decode(type, text));
        }
    }
};

// Never unwind through MD4C: abort via its callback API so it frees parser state.
template<class Action>
int Visit(void* state, Action action) noexcept
{
    auto& renderer = *static_cast<Renderer*>(state);
    try {
        action(renderer);
        return renderer.rtf.size() > MaxRtf ? 1 : 0;
    } catch (...) {
        return 1;
    }
}

constexpr MD_PARSER Parser = {
    0, MD_FLAG_TABLES | MD_FLAG_TASKLISTS | MD_FLAG_STRIKETHROUGH | MD_FLAG_NOHTML,
    [](MD_BLOCKTYPE type, void* detail, void* self) { return Visit(self, [&](Renderer& r) { r.EnterBlock(type, detail); }); },
    [](MD_BLOCKTYPE type, void*, void* self) { return Visit(self, [&](Renderer& r) { r.LeaveBlock(type); }); },
    [](MD_SPANTYPE type, void* detail, void* self) { return Visit(self, [&](Renderer& r) { r.EnterSpan(type, detail); }); },
    [](MD_SPANTYPE type, void*, void* self) { return Visit(self, [&](Renderer& r) { r.LeaveSpan(type); }); },
    [](MD_TEXTTYPE type, const char* text, MD_SIZE size, void* self) { return Visit(self, [&](Renderer& r) { r.Text(type, {text, size}); }); },
    nullptr, nullptr
};

DWORD CALLBACK StreamRtf(DWORD_PTR cookie, LPBYTE buffer, LONG capacity, LONG* written)
{
    auto& remaining = *reinterpret_cast<std::string_view*>(cookie);
    size_t const count = std::min(remaining.size(), static_cast<size_t>(capacity));
    memcpy(buffer, remaining.data(), count);
    remaining.remove_prefix(count);
    *written = static_cast<LONG>(count);
    return 0;
}
} // namespace

HWND MarkdownPreview_Create(HWND parent, int id, HMODULE* module)
{
    // Load on first use; the main window owns one module reference until shutdown.
    if (!*module) {
        *module = LoadLibraryExW(L"MSFTEDIT.DLL", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!*module) {
            return nullptr;
        }
    }
    HWND const preview = CreateWindowExW(WS_EX_CLIENTEDGE | WS_EX_NOINHERITLAYOUT, MSFTEDIT_CLASS, L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
    if (preview) {
        SendMessage(preview, EM_SETUNDOLIMIT, 0, 0);
    }
    return preview;
}

void MarkdownPreview_Render(HWND preview, const char* utf8, size_t length, HPATHL source, int widthTwips,
                            bool dark, bool resetScroll)
{
    // The Scintilla range pointer is borrowed; copy before sending any window messages.
    std::string snapshot;
    bool copied = !length;
    if (utf8 && length <= MARKDOWN_PREVIEW_MAX_SOURCE) {
        try {
            snapshot.assign(utf8, length);
            copied = true;
        } catch (...) {
            // Use the same fallback as a rendering failure.
        }
    }
    POINT scroll{};
    if (!resetScroll) {
        SendMessage(preview, EM_GETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    }
    SendMessage(preview, WM_SETREDRAW, FALSE, 0);
    SendMessage(preview, EM_SETBKGNDCOLOR, 0, dark ? RGB(32, 35, 40) : RGB(255, 255, 255));
    // RichEdit requires writable mode for EM_STREAMIN; no input is dispatched here.
    SendMessage(preview, EM_SETREADONLY, FALSE, 0);
    const wchar_t* const displayError = L"Markdown preview could not be displayed.";
    const wchar_t* failure = nullptr;
    try {
        if (length > MARKDOWN_PREVIEW_MAX_SOURCE) {
            failure = L"Markdown preview is limited to 256 KiB of source text.";
        } else if (!copied) {
            failure = displayError;
        } else {
            Renderer renderer(source, std::max(600, widthTwips), dark);
            if (md_parse(snapshot.data(), static_cast<MD_SIZE>(snapshot.size()), &Parser, &renderer) == 0) {
                renderer.rtf += '}';
                std::string_view remaining(renderer.rtf);
                EDITSTREAM stream{reinterpret_cast<DWORD_PTR>(&remaining), 0, StreamRtf};
                SendMessage(preview, EM_STREAMIN, SF_RTF, reinterpret_cast<LPARAM>(&stream));
                if (stream.dwError) {
                    failure = displayError;
                }
            } else {
                failure = L"Markdown preview exceeds its rendering limits or could not be parsed.";
            }
        }
    } catch (...) {
        failure = displayError;
    }
    if (failure) {
        SetWindowTextW(preview, failure);
        CHARFORMATW format{};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = dark ? RGB(230, 232, 235) : RGB(32, 36, 42);
        SendMessage(preview, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&format));
    }
    SendMessage(preview, EM_SETREADONLY, TRUE, 0);
    SendMessage(preview, EM_SETSEL, 0, 0);
    SendMessage(preview, EM_SETSCROLLPOS, 0, reinterpret_cast<LPARAM>(&scroll));
    SendMessage(preview, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(preview, nullptr, TRUE);
}
