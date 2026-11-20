# Requires the linked Notepad3 build and an interactive Windows desktop.
[CmdletBinding()]
param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\Bin\Release_x64_v145\Notepad3.exe'),
    [switch]$UseGdi
)

$ErrorActionPreference = 'Stop'

Add-Type @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

public static class MarkdownPreviewWindow {
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll")]
    public static extern IntPtr GetDlgItem(IntPtr parent, int id);
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")]
    private static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")]
    private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll", EntryPoint="GetWindowLongW")]
    public static extern int GetStyle(IntPtr hwnd, int index = -16);
    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", SetLastError=true)]
    private static extern IntPtr SendRaw(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam,
                                         uint flags, uint timeout, out IntPtr result);
    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", CharSet=CharSet.Unicode, SetLastError=true)]
    private static extern IntPtr ReadRaw(IntPtr hwnd, uint message, IntPtr wparam, StringBuilder lparam,
                                         uint flags, uint timeout, out IntPtr result);

    [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", SetLastError=true)]
    private static extern IntPtr ReadRect(IntPtr hwnd, uint message, IntPtr wparam, out Rect rect,
                                         uint flags, uint timeout, out IntPtr result);

    public static void Send(IntPtr hwnd, uint message, int wparam = 0, int lparam = 0) {
        if (SendRaw(hwnd, message, (IntPtr)wparam, (IntPtr)lparam, 2, 5000, out _) == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }

    public static Rect ClientRectInPixels(IntPtr hwnd) {
        var previous = SetThreadDpiAwarenessContext((IntPtr)(-4)); // PER_MONITOR_AWARE_V2
        try {
            GetClientRect(hwnd, out var rect);
            return rect;
        } finally {
            if (previous != IntPtr.Zero) SetThreadDpiAwarenessContext(previous);
        }
    }

    public static Rect FormattingRect(IntPtr hwnd) {
        if (ReadRect(hwnd, 0x00B2, IntPtr.Zero, out var rect, 2, 5000, out _) == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return rect;
    }

    public static string Text(IntPtr hwnd) {
        var text = new StringBuilder(4096);
        if (ReadRaw(hwnd, 0x000D, (IntPtr)text.Capacity, text, 2, 5000, out _) == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());
        return text.ToString();
    }
}
'@

function Wait-For([scriptblock]$Condition, [string]$Description) {
    $until = [Diagnostics.Stopwatch]::StartNew()
    do {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 50
    } while ($until.ElapsedMilliseconds -lt 8000)
    throw "Timed out: $Description"
}

function Assert-That([bool]$Condition, [string]$Description) {
    if (!$Condition) { throw "Failed: $Description" }
    Write-Host "PASS: $Description"
}

function Start-PrivateEditor([string]$Exe, [string]$File) {
    $process = Start-Process -FilePath (Resolve-Path -LiteralPath $Exe).Path `
        -ArgumentList '/n', '/f0', ('"{0}"' -f $File) -PassThru
    try {
        Wait-For {
            $process.Refresh()
            if ($process.HasExited) { throw 'Notepad3 exited at startup' }
            $process.MainWindowHandle -ne [IntPtr]::Zero -and $process.MainWindowTitle.Contains((Split-Path -Leaf $File))
        } 'isolated editor window with the fixture loaded'
        return $process
    } catch {
        $process.Refresh()
        if (!$process.HasExited) {
            Write-Warning "Window title before cleanup: $($process.MainWindowTitle)"
            Stop-Process -Id $process.Id
        }
        throw
    }
}

function Get-PrivateMiB($Process) {
    Start-Sleep -Milliseconds 500
    $Process.Refresh()
    return [math]::Round($Process.PrivateMemorySize64 / 1MB, 2)
}

$file = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'test_files\markdown\Preview.md')).Path
$edit = 0xFB03
$preview = 0xFB09
$splitter = 0xFB0A
$command = 0x0111
$togglePreview = 41066
$process = $null
$largeProcess = $null
$largeFile = Join-Path ([IO.Path]::GetTempPath()) ('np3-large-preview-' + [guid]::NewGuid() + '.md')
try {
    $process = Start-PrivateEditor $Executable $file
    $main = $process.MainWindowHandle
    $source = [MarkdownPreviewWindow]::GetDlgItem($main, $edit)
    Assert-That ($source -ne [IntPtr]::Zero) 'The source editor exists'
    if ($UseGdi) {
        # Isolate the preview from the editor's DirectWrite resize allocations.
        [MarkdownPreviewWindow]::Send($source, 2630, 0) # SCI_SETTECHNOLOGY, SC_TECHNOLOGY_DEFAULT
    }
    Assert-That (([MarkdownPreviewWindow]::GetDlgItem($main, $preview) -eq [IntPtr]::Zero) -and
                 ([MarkdownPreviewWindow]::GetDlgItem($main, $splitter) -eq [IntPtr]::Zero)) 'No preview or splitter is created at startup'
    $whole = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$whole)
    $offMiB = Get-PrivateMiB $process

    [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
    $pane = [MarkdownPreviewWindow]::GetDlgItem($main, $preview)
    Assert-That ($pane -ne [IntPtr]::Zero) 'The View command creates a side pane'
    Assert-That (0 -ne ([MarkdownPreviewWindow]::GetStyle($pane) -band 0x0800)) 'The pane is read-only'
    Assert-That (0 -eq ([MarkdownPreviewWindow]::GetStyle($pane, -20) -band 0x0200)) 'The pane has no bright Win32 client edge'
    $content = [MarkdownPreviewWindow]::FormattingRect($pane)
    $client = [MarkdownPreviewWindow]::ClientRectInPixels($pane)
    Assert-That ($content.Left -ge 8 -and ($client.Right - $content.Right) -ge 8 -and $content.Top -ge 6) `
        "The preview has horizontal and top padding (content: $($content.Left), $($content.Top), $($content.Right); client width: $($client.Right))"
    $split = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$split)
    Assert-That (($split.Right - $split.Left) -lt ($whole.Right - $whole.Left)) 'The pane shares the editor area'
    $text = [MarkdownPreviewWindow]::Text($pane)
    Assert-That ($text.Contains('Markdown preview · 中文 😀') -and !$text.Contains('# Markdown preview')) 'Unicode heading renders without its Markdown marker'
    Assert-That ($text.Contains('native preview with emphasis') -and $text.Contains('{\literal} & unchanged')) 'Inline formatting and RTF escaping preserve the text'
    Assert-That ($text.Contains('3. Ordered lists') -and $text.Contains('4. The next item')) 'Ordered list start and increment are preserved'
    Assert-That ($text.Contains('Feature table') -and $text.Contains('Long content wraps inside the column') -and !$text.Contains('| :---')) 'Table text renders without Markdown table delimiters'
    # WM_GETTEXT exposes a RichEdit picture as a single space, not its alt text.
    Assert-That ($text -match 'Images\r?\n[ \uFFFC]\r?\n\[Image: External image\]') 'A relative local PNG renders as an embedded picture'
    Assert-That ($text.Contains('[Image: External image]') -and $text.Contains('[Image: Missing image]')) 'External and missing images render as placeholders'
    $onMiB = Get-PrivateMiB $process
    $divider = [MarkdownPreviewWindow]::GetDlgItem($main, $splitter)
    Assert-That ($divider -ne [IntPtr]::Zero) 'The split view has a draggable divider'
    $dragStart = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($divider, [ref]$dragStart)
    [MarkdownPreviewWindow]::Send($divider, 0x0201, 1, 0x000A0003) # WM_LBUTTONDOWN
    [MarkdownPreviewWindow]::Send($divider, 0x0200, 1, 0x000A004B) # WM_MOUSEMOVE to the right
    $dragged = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($divider, [ref]$dragged)
    $beforeRelease = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$beforeRelease)
    Assert-That ($dragged.Left -gt $dragStart.Left -and $beforeRelease.Right -eq $split.Right) 'Dragging moves only the divider, without reflowing the panes'
    [MarkdownPreviewWindow]::Send($divider, 0x0202, 0, 0x000A0003) # WM_LBUTTONUP
    $expanded = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$expanded)
    Assert-That ($expanded.Right -gt $split.Right) 'Releasing the divider applies the new ratio'
    [MarkdownPreviewWindow]::Send($divider, 0x0201, 1, 0x000A0003)
    [MarkdownPreviewWindow]::Send($divider, 0x0200, 1, 0x000AFFC0) # move left
    [MarkdownPreviewWindow]::Send($divider, 0x001F) # WM_CANCELMODE
    $cancelled = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($divider, [ref]$cancelled)
    Assert-That ([math]::Abs($cancelled.Left - $dragged.Left) -le 2) 'Cancelling a drag restores the last committed position'
    [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
    $firstCloseMiB = Get-PrivateMiB $process
    [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
    $pane = [MarkdownPreviewWindow]::GetDlgItem($main, $preview)
    Assert-That ($pane -ne [IntPtr]::Zero) 'The preview can be reopened'
    $reopened = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$reopened)
    Assert-That ($reopened.Right -eq $expanded.Right) 'Reopening remembers the ratio in the current window'

    [MarkdownPreviewWindow]::Send($source, 2025, 2) # SCI_GOTOPOS: after '# '
    [MarkdownPreviewWindow]::Send($source, 0x0102, [int][char]'Z') # WM_CHAR
    Wait-For { [MarkdownPreviewWindow]::Text($pane).Contains('ZMarkdown preview') } 'unsaved edit in the preview'
    [MarkdownPreviewWindow]::Send($main, $command, 40300) # IDM_EDIT_UNDO
    Wait-For { ![MarkdownPreviewWindow]::Text($pane).Contains('ZMarkdown preview') } 'undo in the preview'
    Assert-That $true 'Unsaved edits and undo refresh the pane'

    [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
    Assert-That (([MarkdownPreviewWindow]::GetDlgItem($main, $preview) -eq [IntPtr]::Zero) -and
                 ([MarkdownPreviewWindow]::GetDlgItem($main, $splitter) -eq [IntPtr]::Zero)) 'Closing the pane destroys both controls'
    $restored = [MarkdownPreviewWindow+Rect]::new()
    [void][MarkdownPreviewWindow]::GetWindowRect($source, [ref]$restored)
    Assert-That (($restored.Right - $restored.Left) -eq ($whole.Right - $whole.Left)) 'Closing restores full editor width'
    $afterEditsMiB = Get-PrivateMiB $process
    $cyclesMiB = foreach ($cycle in 1..3) {
        [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
        [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
        Get-PrivateMiB $process
    }
    Write-Host "Private MiB (off / on / closed / closed after edits / subsequent closes): $offMiB / $onMiB / $firstCloseMiB / $afterEditsMiB / $($cyclesMiB -join ', ')"

    [MarkdownPreviewWindow]::Send($main, $command, $togglePreview)
    [MarkdownPreviewWindow]::Send($main, $command, 40000) # IDM_FILE_NEW
    Wait-For { [MarkdownPreviewWindow]::GetDlgItem($main, $preview) -eq [IntPtr]::Zero } 'closing the preview on a non-Markdown document'
    Assert-That $true 'Switching to a plain new document closes the preview'

    [MarkdownPreviewWindow]::Send($main, 0x0010) # WM_CLOSE
    if (!$process.WaitForExit(5000)) { throw 'Editor failed to close' }

    [IO.File]::WriteAllText($largeFile, ("A long Markdown document.`r`n`r`n" * 10240))
    $largeProcess = Start-PrivateEditor $Executable $largeFile
    $largeMain = $largeProcess.MainWindowHandle
    [MarkdownPreviewWindow]::Send($largeMain, $command, $togglePreview)
    $largePane = [MarkdownPreviewWindow]::GetDlgItem($largeMain, $preview)
    Assert-That ([MarkdownPreviewWindow]::Text($largePane).Contains('256 KiB')) 'Oversized documents show a limit message without rendering'
    [MarkdownPreviewWindow]::Send($largeMain, 0x0010)
    if (!$largeProcess.WaitForExit(5000)) { throw 'Editor with a large document failed to close' }
} finally {
    foreach ($p in @($process, $largeProcess)) {
        if ($p -and !$p.HasExited) { Stop-Process -Id $p.Id }
    }
    if (Test-Path -LiteralPath $largeFile) { Remove-Item -LiteralPath $largeFile }
}
