@echo off
setlocal enableextensions
cd /d "%~dp0"

:: Quick help check before elevation check
if /i "%~1"=="/help" goto :CLI_HELP
if /i "%~1"=="-help" goto :CLI_HELP
if /i "%~1"=="/?" goto :CLI_HELP

:: Check for Administrator privileges, auto-elevate if needed
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [INFO] 请求管理员权限以配置系统右键注册表...
    echo [INFO] Requesting administrative privileges...
    if "%~1"=="" (
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    ) else (
        powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -ArgumentList '%*' -WorkingDirectory '%~dp0' -Verb RunAs"
    )
    exit /b
)

:: Locate Notepad3.exe
set "NP3_EXE=%~dp0Notepad3.exe"
if not exist "%NP3_EXE%" (
    if exist "%~dp0..\Bin\Release_x64_v145\Notepad3.exe" (
        set "NP3_EXE=%~dp0..\Bin\Release_x64_v145\Notepad3.exe"
    ) else if exist "%~dp0..\Notepad3.exe" (
        set "NP3_EXE=%~dp0..\Notepad3.exe"
    )
)

if not exist "%NP3_EXE%" (
    echo [ERROR] 未找到 Notepad3.exe！
    echo [ERROR] Notepad3.exe was not found in: %~dp0
    echo.
    pause
    exit /b 1
)

:: Command line arguments handling
if /i "%~1"=="/add" goto :CLI_ADD_ZH
if /i "%~1"=="-add" goto :CLI_ADD_ZH
if /i "%~1"=="add" goto :CLI_ADD_ZH
if /i "%~1"=="/install" goto :CLI_ADD_ZH
if /i "%~1"=="-install" goto :CLI_ADD_ZH
if /i "%~1"=="install" goto :CLI_ADD_ZH
if /i "%~1"=="/a" goto :CLI_ADD_ZH
if /i "%~1"=="-a" goto :CLI_ADD_ZH
if /i "%~1"=="/i" goto :CLI_ADD_ZH
if /i "%~1"=="-i" goto :CLI_ADD_ZH

if /i "%~1"=="/add_en" goto :CLI_ADD_EN
if /i "%~1"=="-add_en" goto :CLI_ADD_EN
if /i "%~1"=="add_en" goto :CLI_ADD_EN

if /i "%~1"=="/remove" goto :CLI_REMOVE
if /i "%~1"=="-remove" goto :CLI_REMOVE
if /i "%~1"=="remove" goto :CLI_REMOVE
if /i "%~1"=="/delete" goto :CLI_REMOVE
if /i "%~1"=="-delete" goto :CLI_REMOVE
if /i "%~1"=="delete" goto :CLI_REMOVE
if /i "%~1"=="/uninstall" goto :CLI_REMOVE
if /i "%~1"=="-uninstall" goto :CLI_REMOVE
if /i "%~1"=="uninstall" goto :CLI_REMOVE
if /i "%~1"=="/r" goto :CLI_REMOVE
if /i "%~1"=="-r" goto :CLI_REMOVE
if /i "%~1"=="/u" goto :CLI_REMOVE
if /i "%~1"=="-u" goto :CLI_REMOVE

:: Interactive Menu
:MENU
cls
title Notepad3 右键菜单管理 / Context Menu Manager
echo ============================================================
echo           Notepad3 右键菜单管理 / Context Menu Manager
echo ============================================================
echo.
echo   程序路径 / Application:
echo   "%NP3_EXE%"
echo.
echo   [1] 添加右键菜单 ("使用 Notepad3 打开")
echo   [2] 添加英文右键菜单 ("Open with Notepad3")
echo   [3] 移除右键菜单 (从系统右键中彻底删除)
echo   [0] 退出 / Exit
echo.
echo ============================================================
set "CHOICE="
set /p "CHOICE=请输入选项 [1/2/3/0]: "

if "%CHOICE%"=="1" goto :MENU_ADD_ZH
if "%CHOICE%"=="2" goto :MENU_ADD_EN
if "%CHOICE%"=="3" goto :MENU_REMOVE
if "%CHOICE%"=="0" exit /b 0
goto :MENU

:MENU_ADD_ZH
call :ADD_REG "使用 Notepad3 打开"
echo.
echo ============================================================
echo   [OK] 已成功添加 “使用 Notepad3 打开” 到 Windows 右键菜单！
echo ============================================================
echo.
pause
goto :MENU

:MENU_ADD_EN
call :ADD_REG "Open with Notepad3"
echo.
echo ============================================================
echo   [OK] Successfully added "Open with Notepad3" to Context Menu!
echo ============================================================
echo.
pause
goto :MENU

:MENU_REMOVE
call :REMOVE_REG
echo.
echo ============================================================
echo   [OK] 已成功从系统右键菜单中移除 Notepad3！
echo   [OK] Notepad3 context menu entry has been removed.
echo ============================================================
echo.
pause
goto :MENU

:: CLI handlers
:CLI_ADD_ZH
call :ADD_REG "使用 Notepad3 打开"
echo [OK] 已成功添加 “使用 Notepad3 打开” 到 Windows 右键菜单。
exit /b 0

:CLI_ADD_EN
call :ADD_REG "Open with Notepad3"
echo [OK] Successfully added "Open with Notepad3" to Windows Context Menu.
exit /b 0

:CLI_REMOVE
call :REMOVE_REG
echo [OK] 已成功从系统右键菜单中移除 Notepad3。
exit /b 0

:CLI_HELP
echo 用法 / Usage:
echo   %~nx0           打开交互式菜单 / Open interactive menu
echo   %~nx0 /add      添加中文右键菜单 ("使用 Notepad3 打开")
echo   %~nx0 /add_en   添加英文右键菜单 ("Open with Notepad3")
echo   %~nx0 /remove   从右键菜单移除 / Remove from context menu
echo   %~nx0 /help     显示帮助 / Show this help
exit /b 0

:: Core functions
:ADD_REG
set "MENU_LABEL=%~1"
reg delete "HKCU\Software\Classes\*\shell\Notepad3" /f >nul 2>&1
reg add "HKCR\*\shell\Notepad3" /ve /t REG_SZ /d "%MENU_LABEL%" /f >nul
reg add "HKCR\*\shell\Notepad3" /v "Icon" /t REG_SZ /d "\"%NP3_EXE%\",0" /f >nul
reg add "HKCR\*\shell\Notepad3\command" /ve /t REG_SZ /d "\"%NP3_EXE%\" \"%%1\"" /f >nul
exit /b 0

:REMOVE_REG
reg delete "HKCR\*\shell\Notepad3" /f >nul 2>&1
reg delete "HKLM\Software\Classes\*\shell\Notepad3" /f >nul 2>&1
reg delete "HKCU\Software\Classes\*\shell\Notepad3" /f >nul 2>&1
exit /b 0
