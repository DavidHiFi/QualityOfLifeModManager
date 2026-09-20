; Quality of Life Mod Manager - Windows installer (NSIS 3, per-user, no admin).
;
; Built by scripts\build-setup.ps1, which passes DIST (staged dist tree),
; VERSION (from src/Version.h) and OUTFILE on the makensis command line:
;   makensis /DDIST=<dir> /DVERSION=<x.y.z> /DOUTFILE=<setup.exe> scripts\setup.nsi
;
; What it installs: the whole dist tree - the exe plus its Qt 6 runtime,
; the MinGW runtime (libgcc/libstdc++/winpthread) and the plugin folders.
; Nothing is downloaded at install time and no external redistributable is
; needed: objdump on the exe shows no MSVCP/VCRUNTIME import, and Qt uses
; the system's Schannel/DirectX. The only requirements, checked in .onInit,
; are 64-bit Windows 10 or later.

!ifndef DIST
  !error "DIST not defined - build through scripts\build-setup.ps1"
!endif
!ifndef VERSION
  !error "VERSION not defined - build through scripts\build-setup.ps1"
!endif
!ifndef OUTFILE
  !error "OUTFILE not defined - build through scripts\build-setup.ps1"
!endif

!include "MUI2.nsh"
!include "WinVer.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

Name "Quality of Life Mod Manager"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\Quality of Life Mod Manager"
InstallDirRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "InstallLocation"
RequestExecutionLevel user
SetCompressor /SOLID lzma

Icon "${__FILEDIR__}\..\resources\icons\icon.ico"
UninstallIcon "${__FILEDIR__}\..\resources\icons\icon.ico"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Quality of Life Mod Manager"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "DavidHiFi"
VIAddVersionKey "LegalCopyright" "Licensed under the GNU Lesser General Public License v3"
VIAddVersionKey "FileDescription" "Quality of Life Mod Manager Setup"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\QualityOfLifeModManager.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Run Quality of Life Mod Manager now"
!define MUI_LICENSEPAGE_TEXT_TOP "This app is free software under the GNU Lesser General Public License v3. The installer carries its whole runtime, so no extra downloads are needed."
!define MUI_LICENSEPAGE_BUTTON "Install"

!insertmacro MUI_PAGE_LICENSE "${__FILEDIR__}\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Quality of Life Mod Manager needs 64-bit Windows.$\nThis machine is 32-bit, so the app cannot run here."
    Abort
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_ICONSTOP "Quality of Life Mod Manager needs Windows 10 or later.$\nQt 6 does not run on older Windows."
    Abort
  ${EndIf}
FunctionEnd

; If a previous copy is installed, its exe may be running and locked.
; Delete fails on a locked (or missing) file, so probe it first.
Function CloseRunningApp
  retry:
  ${If} ${FileExists} "$INSTDIR\QualityOfLifeModManager.exe"
    ClearErrors
    Delete "$INSTDIR\uninstall.probe"
    Rename "$INSTDIR\QualityOfLifeModManager.exe" "$INSTDIR\uninstall.probe"
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Quality of Life Mod Manager is still running.$\nClose it (check the system tray), then press Retry." IDRETRY retry
      Abort "Install cancelled while the app was running."
    ${Else}
      Rename "$INSTDIR\uninstall.probe" "$INSTDIR\QualityOfLifeModManager.exe"
    ${EndIf}
  ${EndIf}
FunctionEnd

Section "Install"
  Call CloseRunningApp

  SetOutPath "$INSTDIR"
  ; Everything the app needs to run: exe, Qt/MinGW DLLs, plugins, tools.
  ; Junk never ships: the portable zip itself, the runtime cache, settings.
  File /r /x *.zip /x cache /x QualityOfLife.ini /x uninstall.probe "${DIST}\*.*"

  ; Every DLL the exe needs must have landed next to it. They all ship
  ; inside this Setup, so a failure here means a broken install, not a
  ; missing download - stop instead of leaving a half-installed app.
  !macro CheckFile REL
    IfFileExists "$INSTDIR\${REL}" +2
      Abort "Install failed: ${REL} did not land in $INSTDIR. Re-run this Setup; if it repeats, re-download it."
  !macroend
  !insertmacro CheckFile "QualityOfLifeModManager.exe"
  !insertmacro CheckFile "Qt6Core.dll"
  !insertmacro CheckFile "Qt6Gui.dll"
  !insertmacro CheckFile "Qt6Widgets.dll"
  !insertmacro CheckFile "Qt6Network.dll"
  !insertmacro CheckFile "Qt6Svg.dll"
  !insertmacro CheckFile "libgcc_s_seh-1.dll"
  !insertmacro CheckFile "libstdc++-6.dll"
  !insertmacro CheckFile "libwinpthread-1.dll"
  !insertmacro CheckFile "D3Dcompiler_47.dll"
  !insertmacro CheckFile "platforms\qwindows.dll"
  !insertmacro CheckFile "styles\qmodernwindowsstyle.dll"
  !insertmacro CheckFile "tls\qschannelbackend.dll"
  !insertmacro CheckFile "imageformats\qjpeg.dll"
  !insertmacro CheckFile "install.ps1"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "DisplayName" "Quality of Life Mod Manager"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "Publisher" "DavidHiFi"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "DisplayIcon" "$INSTDIR\QualityOfLifeModManager.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "URLInfoAbout" "https://github.com/DavidHiFi/QualityOfLifeModManager"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\Quality of Life Mod Manager"
  CreateShortCut "$SMPROGRAMS\Quality of Life Mod Manager\Quality of Life Mod Manager.lnk" "$INSTDIR\QualityOfLifeModManager.exe" "" "$INSTDIR\QualityOfLifeModManager.exe" 0
  CreateShortCut "$SMPROGRAMS\Quality of Life Mod Manager\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
  CreateShortCut "$DESKTOP\Quality of Life Mod Manager.lnk" "$INSTDIR\QualityOfLifeModManager.exe" "" "$INSTDIR\QualityOfLifeModManager.exe" 0
SectionEnd

Section "Uninstall"
  Call un.CloseRunningApp

  Delete "$INSTDIR\QualityOfLifeModManager.exe"
  Delete "$INSTDIR\Qt6Core.dll"
  Delete "$INSTDIR\Qt6Gui.dll"
  Delete "$INSTDIR\Qt6Widgets.dll"
  Delete "$INSTDIR\Qt6Network.dll"
  Delete "$INSTDIR\Qt6Svg.dll"
  Delete "$INSTDIR\libgcc_s_seh-1.dll"
  Delete "$INSTDIR\libstdc++-6.dll"
  Delete "$INSTDIR\libwinpthread-1.dll"
  Delete "$INSTDIR\D3Dcompiler_47.dll"
  Delete "$INSTDIR\install.ps1"
  Delete "$INSTDIR\Install to Start menu.bat"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\tls"
  RMDir /r "$INSTDIR\iconengines"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\generic"
  RMDir /r "$INSTDIR\networkinformation"
  RMDir /r "$INSTDIR\tools"
  RMDir /r "$INSTDIR\cache"
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$SMPROGRAMS\Quality of Life Mod Manager\Quality of Life Mod Manager.lnk"
  Delete "$SMPROGRAMS\Quality of Life Mod Manager\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Quality of Life Mod Manager"
  Delete "$DESKTOP\Quality of Life Mod Manager.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\QualityOfLifeModManager"
  ; QualityOfLife.ini is deliberately left behind, so a reinstall keeps
  ; the user's settings (same rule as scripts\install.ps1).
  RMDir "$INSTDIR"
SectionEnd

Function un.CloseRunningApp
  retry:
  ${If} ${FileExists} "$INSTDIR\QualityOfLifeModManager.exe"
    ClearErrors
    Delete "$INSTDIR\uninstall.probe"
    Rename "$INSTDIR\QualityOfLifeModManager.exe" "$INSTDIR\uninstall.probe"
    ${If} ${Errors}
      MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Quality of Life Mod Manager is still running.$\nClose it (check the system tray), then press Retry." IDRETRY retry
      Abort "Uninstall cancelled while the app was running."
    ${Else}
      Rename "$INSTDIR\uninstall.probe" "$INSTDIR\QualityOfLifeModManager.exe"
    ${EndIf}
  ${EndIf}
FunctionEnd
