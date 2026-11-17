#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "Scintilla.h"

using DocPos = ptrdiff_t;
using DocPosU = size_t;
using DocPosCR = Sci_PositionCR;
using HSTRINGW = std::wstring;
struct EDITFINDREPLACE {
    int fuFlags = 0;
    HSTRINGW chFindPattern;
};
#define INIT_EFR_DATA {}
using CLPCEDITFINDREPLACE = const EDITFINDREPLACE*;
struct { EDITFINDREPLACE EFR_Data; } Settings;
struct { HWND hwndEdit; HWND hwndDlgFindReplace; } Globals;
constexpr UINT Encoding_SciCP = CP_UTF8;
static void DuplicateEFR(EDITFINDREPLACE* dst, const EDITFINDREPLACE* src) { *dst = *src; }
static void ReleaseEFR(EDITFINDREPLACE*) {}
bool StrgIsNotEmpty(const HSTRINGW& text) { return !text.empty(); }
static void* AllocMem(size_t size, DWORD) { return calloc(1, size); }
static void FreeMem(void* ptr) { free(ptr); }

// Searching, selections, Unicode positions, and arrow-key movement run through
// the real Scintilla control. Find-dialog data and the Alt+Enter mark-all handoff
// are stubbed so the test needs no visible application window.
static EDITFINDREPLACE liveFind;
static void _SetSearchFlags(HWND, EDITFINDREPLACE* efr) { *efr = liveFind; }
static LPCWSTR _EditGetFindStrg(HWND, EDITFINDREPLACE* efr, bool) { return efr->chFindPattern.c_str(); }
#define SciCall(m, w, l) SendMessage(Globals.hwndEdit, (m), (w), (l))
#include "MultiSelectionWrappers.inc"
static DocPos Sci_GetDocEndPosition() { return SciCall(SCI_GETLENGTH, 0, 0); }
static void Sci_ScrollSelectionToView() { SciCall(SCI_SCROLLCARET, 0, 0); }
static bool StrIsNotEmpty(LPCWSTR text) { return text && *text; }
int GetMarkAllOccSearchFlags() { return 0; } // Pre-fix source compatibility.
static void UndoTransActionBegin() {}
static void EndUndoTransAction() {}
static std::wstring markedPattern;
static int markedFlags;
static bool markedAfterClose;
static void EditMarkAll(LPCWSTR text, int flags, DocPos, DocPos, bool) {
    markedPattern = text;
    markedFlags = flags;
    markedAfterClose = !IsWindow(Globals.hwndDlgFindReplace);
}
#include "MultiSelection.inc"

LRESULT CALLBACK FindWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_COMMAND && LOWORD(wParam) == IDCANCEL) {
        // Simulate WM_DESTROY clearing the live dialog and restoring its initial selection.
        liveFind = {};
        SciCall_SetSelection(0, 0);
        Globals.hwndDlgFindReplace = nullptr;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

using Range = std::pair<DocPos, DocPos>;
static std::vector<Range> ranges() {
    std::vector<Range> result;
    for (DocPosU i = 0; i < SciCall_GetSelections(); ++i) {
        result.emplace_back(SciCall_GetSelectionNStart(i), SciCall_GetSelectionNEnd(i));
    }
    std::sort(result.begin(), result.end());
    return result;
}
static void reset(const char* text, DocPos caret = 0) {
    Globals.hwndDlgFindReplace = nullptr;
    Settings.EFR_Data = {};
    liveFind = {};
    SciCall(SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(text));
    SciCall_SetSelection(caret, caret);
}
static void select(DocPos start, DocPos end) { SciCall_SetSelection(end, start); }

int main() {
    const HINSTANCE instance = GetModuleHandle(nullptr);
    if (!Scintilla_RegisterClasses(instance)) { return 2; }
    Globals.hwndEdit = CreateWindowExW(0, L"Scintilla", L"", WS_POPUP, 0, 0, 800, 600, nullptr, nullptr, instance, nullptr);
    if (!Globals.hwndEdit) { return 2; }
    SciCall(SCI_SETCODEPAGE, SC_CP_UTF8, 0);
    SciCall(SCI_SETMULTIPLESELECTION, 1, 0);
    SciCall(SCI_SETADDITIONALSELECTIONTYPING, 1, 0);
    int failures = 0;
    int checks = 0;
    const auto check = [&](const char* name, std::vector<Range> expected) {
        ++checks;
        std::sort(expected.begin(), expected.end());
        if (ranges() != expected) {
            ++failures;
            std::cerr << "FAIL: " << name << " actual:";
            for (const auto& [start, end] : ranges()) { std::cerr << ' ' << start << ':' << end; }
            std::cerr << '\n';
        }
    };
    reset("foo foo foo");
    EditSelectionMultiSelectNext();
    check("first Ctrl+D selects word", {{0, 3}});
    EditSelectionMultiSelectNext();
    check("second Ctrl+D adds occurrence", {{0, 3}, {4, 7}});
    EditSelectionMultiSelectNext();
    EditSelectionMultiSelectNext();
    check("exhausted matches retain all selections", {{0, 3}, {4, 7}, {8, 11}});

    for (const UINT arrow : {SCI_CHARLEFT, SCI_CHARRIGHT, SCI_LINEUP, SCI_LINEDOWN}) {
        reset("foo bar\nfoo bar\nfoo bar\nfoo bar");
        select(8, 11);
        SciCall_AddSelection(19, 16);
        SciCall(arrow, 0, 0);
        EditSelectionMultiSelectNext();
        if (arrow == SCI_CHARLEFT || arrow == SCI_CHARRIGHT) {
            check("horizontal move then Ctrl+D retains both carets", {{8, 11}, {16, 19}});
        } else if (arrow == SCI_LINEUP) {
            check("up then Ctrl+D retains both carets", {{0, 3}, {8, 11}});
        } else {
            check("down then Ctrl+D retains both carets", {{16, 19}, {24, 27}});
        }
        EditSelectionMultiSelectNext();
        ++checks;
        if (SciCall_GetSelections() != 3) { ++failures; std::cerr << "FAIL: continue adding after arrow\n"; }
    }

    reset("foo bar\nfoo bar\nfoo bar");
    select(0, 3);
    SciCall_AddSelection(11, 8);
    SciCall(SCI_CHARRIGHT, 0, 0);
    EditSelectionMultiSelectNext();
    SendMessage(Globals.hwndEdit, WM_CHAR, 'x', 1);
    char edited[64] = {};
    SciCall(SCI_GETTEXT, sizeof(edited), reinterpret_cast<LPARAM>(edited));
    ++checks;
    if (std::string(edited) != "x bar\nx bar\nfoo bar") {
        ++failures; std::cerr << "FAIL: typing edits every expanded selection\n";
    }
    SciCall(SCI_UNDO, 0, 0);
    SciCall(SCI_GETTEXT, sizeof(edited), reinterpret_cast<LPARAM>(edited));
    ++checks;
    if (std::string(edited) != "foo bar\nfoo bar\nfoo bar") {
        ++failures; std::cerr << "FAIL: multi-selection typing undoes together\n";
    }

    reset("foobar foobar", 1);
    SciCall_AddSelection(3, 3);
    EditSelectionMultiSelectNext();
    check("carets in the same word become one editable selection", {{0, 6}});
    EditSelectionMultiSelectNext();
    check("continue after merging duplicate word selections", {{0, 6}, {7, 13}});

    reset("foo bar foo bar foo bar");
    select(0, 3);
    SciCall_AddSelection(11, 8);
    SciCall(SCI_WORDRIGHT, 0, 0);
    EditSelectionMultiSelectNext();
    check("expand words after moving multiple carets to another word", {{4, 7}, {12, 15}});
    EditSelectionMultiSelectNext();
    check("continue with newly selected text", {{4, 7}, {12, 15}, {20, 23}});

    reset("foo foo foo foo");
    select(8, 11);
    EditSelectionMultiSelectNext();
    EditSelectionMultiSelectNext();
    check("wrap forward", {{0, 3}, {8, 11}, {12, 15}});
    EditSelectionMultiSelectNext();
    check("continue after wrapped main selection", {{0, 3}, {4, 7}, {8, 11}, {12, 15}});

    reset("foo bar foo bar");
    select(0, 3);
    SciCall_AddSelection(7, 4);
    EditSelectionMultiSelectNext();
    check("use main selection text", {{0, 3}, {4, 7}, {12, 15}});

    reset("foo Foo foobar foo");
    Settings.EFR_Data.fuFlags = SCFIND_WHOLEWORD;
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("saved whole-word and case-insensitive flags", {{0, 3}, {4, 7}});
    Settings.EFR_Data.fuFlags |= SCFIND_MATCHCASE;
    EditSelectionMultiSelectNext();
    check("saved case-sensitive flags use main selection's case", {{0, 3}, {4, 7}});
    EditSelectionMultiSelectAll();
    check("select all shares saved conditions and main selection text", {{4, 7}});

    reset("foo foobar foo");
    select(0, 3);
    EditSelectionMultiSelectAll();
    check("whole-word matching is not inferred from selection boundaries", {{0, 3}, {4, 7}, {11, 14}});

    reset("foo foobar xfoo");
    Settings.EFR_Data.fuFlags = SCFIND_WORDSTART;
    select(0, 3);
    EditSelectionMultiSelectAll();
    check("saved word-start option", {{0, 3}, {4, 7}});

    reset("a.b aXb a.b");
    Settings.EFR_Data.fuFlags = SCFIND_REGEXP;
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("selected text stays literal with a saved regex option", {{0, 3}, {8, 11}});

    HWND const dialog = CreateWindowExW(0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    reset("foo Foo foobar foo");
    Globals.hwndDlgFindReplace = dialog;
    liveFind = { SCFIND_WHOLEWORD, L"foo" };
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("read live find options", {{0, 3}, {4, 7}});
    liveFind.fuFlags |= SCFIND_MATCHCASE;
    EditSelectionMultiSelectNext();
    check("live option changes apply immediately", {{0, 3}, {4, 7}, {15, 18}});
    EditSelectionMultiSelectAll();
    check("select all uses live options", {{0, 3}, {15, 18}});

    reset("foo Foo foobar foo");
    Globals.hwndDlgFindReplace = dialog;
    liveFind = { SCFIND_WHOLEWORD, L"" };
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("empty live find pattern uses selected text with live options", {{0, 3}, {4, 7}});

    reset("id1 id22 id333");
    Globals.hwndDlgFindReplace = dialog;
    liveFind = { SCFIND_REGEXP, L"id[0-9]+" };
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("live regex adds variable-length matches", {{0, 3}, {4, 8}});
    EditSelectionMultiSelectSkip();
    check("skip uses the same regex", {{0, 3}, {9, 14}});
    EditSelectionMultiSelectAll();
    check("select all regex matches", {{0, 3}, {4, 8}, {9, 14}});
    liveFind.chFindPattern = L"[";
    EditSelectionMultiSelectNext();
    check("invalid regex preserves selections", {{0, 3}, {4, 8}, {9, 14}});

    reset("foo foo");
    select(0, 3);
    SciCall_AddSelection(7, 4);
    EditSelectionMultiSelectSkip();
    check("skip with no available match preserves both selections", {{0, 3}, {4, 7}});
    reset("foo foo foo");
    select(0, 3);
    EditSelectionMultiSelectSkip();
    check("skip with single selection", {{4, 7}});
    SciCall_AddSelection(11, 8);
    EditSelectionMultiSelectSkip();
    check("skip wraps and preserves other selections", {{0, 3}, {4, 7}});

    reset("\xE4\xB8\xAD foo \xE4\xB8\xAD foo");
    select(0, 3);
    EditSelectionMultiSelectNext();
    check("UTF-8 byte positions", {{0, 3}, {8, 11}});

    reset("abc");
    Globals.hwndDlgFindReplace = dialog;
    liveFind = { SCFIND_REGEXP, L"(?=.)|$" };
    EditSelectionMultiSelectAll();
    check("zero-length regex progresses and includes EOF once", {{0, 0}, {1, 1}, {2, 2}, {3, 3}});
    reset("abc");
    Globals.hwndDlgFindReplace = dialog;
    liveFind = { SCFIND_REGEXP, L"$" };
    select(0, 1);
    EditSelectionMultiSelectNext();
    check("next accepts a zero-length EOF match", {{0, 1}, {3, 3}});
    EditSelectionMultiSelectNext();
    check("next does not duplicate zero-length match", {{0, 1}, {3, 3}});
    SciCall_SetSelection(3, 3);
    liveFind.chFindPattern = L"(?=.)|$";
    EditSelectionMultiSelectNext();
    check("zero-length find selections continue without word expansion", {{0, 0}, {3, 3}});

    reset("foo foo");
    select(0, 3);
    SciCall_SetTargetRange(2, 5);
    SciCall_SetSearchFlags(SCFIND_WORDSTART);
    EditSelectionMultiSelectNext();
    ++checks;
    if (SciCall_GetTargetStart() != 2 || SciCall_GetTargetEnd() != 5 || SciCall_GetSearchFlags() != SCFIND_WORDSTART) {
        ++failures; std::cerr << "FAIL: restore target and search flags\n";
    }
#if TEST_FIND_CLOSE
    WNDCLASSW findClass = {};
    findClass.hInstance = instance;
    findClass.lpfnWndProc = FindWindowProc;
    findClass.lpszClassName = L"NP3MultiSelectionFind";
    RegisterClassW(&findClass);
    HWND const closeDialog = CreateWindowExW(0, findClass.lpszClassName, L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, instance, nullptr);
    Globals.hwndDlgFindReplace = closeDialog;
    liveFind = { SCFIND_REGEXP | SCFIND_MATCHCASE | SCFIND_WHOLEWORD, L"id[0-9]+" };
    _EditSelectAllFromFind(closeDialog);
    ++checks;
    if (markedPattern != L"id[0-9]+" || markedFlags != (SCFIND_REGEXP | SCFIND_MATCHCASE | SCFIND_WHOLEWORD) || !markedAfterClose) {
        ++failures; std::cerr << "FAIL: Alt+Enter retains pattern and flags after dialog destruction\n";
    }
#endif
    DestroyWindow(dialog);
    DestroyWindow(Globals.hwndEdit);
    Scintilla_ReleaseResources();
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
