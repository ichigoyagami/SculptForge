; NSIS 3.09+, built from the modified SculptForge 0.10.8 Windows staging tree.
; Pass paths with -D on Unix or /D on Windows before this script's filename.
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "WinVer.nsh"
Target amd64-unicode
Unicode true
Name "SculptForge 0.10.8"
OutFile "${SF_OUTPUT}"
InstallDir "$PROGRAMFILES64\SculptForge"
InstallDirRegKey HKLM "Software\SculptForge\BaselineInstaller" "InstallDir"
RequestExecutionLevel admin
SetCompressor zlib
ShowInstDetails show
ShowUninstDetails show
VIProductVersion "0.10.8.1"
VIAddVersionKey "ProductName" "SculptForge"
VIAddVersionKey "FileDescription" "SculptForge 0.10.8 Gizmo deformers and upstream merge setup"
VIAddVersionKey "FileVersion" "0.10.8.1"
VIAddVersionKey "LegalCopyright" "SculptForge contributors; application GPL-3.0-or-later"
!define MUI_ICON "${SF_ICON}"
!define MUI_UNICON "${SF_ICON}"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\bin\sculptforge.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SF_PAYLOAD}/doc/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "This package requires Windows 10 or later, 64-bit."
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
FunctionEnd

Function .onVerifyInstDir
  IfFileExists "$INSTDIR\bin\sculptforge.exe" 0 dir_ok
  ReadRegStr $0 HKLM "Software\SculptForge\BaselineInstaller" "InstallDir"
  StrCmp $0 $INSTDIR dir_ok
  SetErrors
  Return
dir_ok:
  ClearErrors
FunctionEnd

Section "Application and source code" SecApp
  SectionIn RO
  SetRegView 64
  SetShellVarContext all
  SetOutPath "$INSTDIR"
  File /r "${SF_PAYLOAD}/*"
  SetOutPath "$INSTDIR\support"
  File /r "${SF_EXTRA}/*"
  SetOutPath "$INSTDIR\source"
  File /r "${SF_SOURCE}/*"
  WriteUninstaller "$INSTDIR\Uninstall-SculptForge.exe"
  WriteRegStr HKLM "Software\SculptForge\BaselineInstaller" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "DisplayName" "SculptForge 0.10.8"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "DisplayVersion" "0.10.8"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "Publisher" "SculptForge contributors"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "DisplayIcon" "$INSTDIR\bin\sculptforge.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "UninstallString" '$\"$INSTDIR\Uninstall-SculptForge.exe$\"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "QuietUninstallString" '$\"$INSTDIR\Uninstall-SculptForge.exe$\" /S'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline" "NoRepair" 1
  SetOutPath "$INSTDIR\bin"
  CreateDirectory "$SMPROGRAMS\SculptForge"
  CreateShortCut "$SMPROGRAMS\SculptForge\SculptForge.lnk" "$INSTDIR\bin\sculptforge.exe"
  CreateShortCut "$SMPROGRAMS\SculptForge\SculptForge - Test Installation.lnk" "$SYSDIR\cmd.exe" '/c $\"$\"$INSTDIR\support\Test-Installation.cmd$\"$\"'
  CreateShortCut "$SMPROGRAMS\SculptForge\Read Me.lnk" "$INSTDIR\support\READ-ME-FIRST.txt"
  CreateShortCut "$SMPROGRAMS\SculptForge\Uninstall SculptForge.lnk" "$INSTDIR\Uninstall-SculptForge.exe"
SectionEnd

Section "Desktop shortcut" SecDesktop
  SetShellVarContext all
  CreateShortCut "$DESKTOP\SculptForge.lnk" "$INSTDIR\bin\sculptforge.exe"
SectionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  ReadRegStr $0 HKLM "Software\SculptForge\BaselineInstaller" "InstallDir"
  StrCmp $0 $INSTDIR uninstall_owned
  MessageBox MB_OK|MB_ICONSTOP "The installation path does not match the registered package. No files were removed."
  Abort
uninstall_owned:
  ; Only named package files are removed. Never recursively delete a user's folder.
  !include "${SF_SOURCE}/uninstall-files.nsh"
  Delete "$INSTDIR\Uninstall-SculptForge.exe"
  Delete "$DESKTOP\SculptForge.lnk"
  Delete "$SMPROGRAMS\SculptForge\SculptForge.lnk"
  Delete "$SMPROGRAMS\SculptForge\SculptForge - Test Installation.lnk"
  Delete "$SMPROGRAMS\SculptForge\Read Me.lnk"
  Delete "$SMPROGRAMS\SculptForge\Uninstall SculptForge.lnk"
  RMDir "$SMPROGRAMS\SculptForge"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SculptForge-Baseline"
  DeleteRegKey HKLM "Software\SculptForge\BaselineInstaller"
  RMDir "$INSTDIR"
SectionEnd
