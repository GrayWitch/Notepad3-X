// encoding: UTF-8
#pragma once

#include "TypeDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MARKDOWN_PREVIEW_MAX_SOURCE (256 * 1024)

HWND MarkdownPreview_Create(HWND parent, int id, HMODULE* module);
void MarkdownPreview_Render(HWND preview, const char* utf8, size_t length, HPATHL source,
                            UINT dpi, bool dark, bool resetScroll);

#ifdef __cplusplus
}
#endif
