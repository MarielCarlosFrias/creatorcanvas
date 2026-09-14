; CreatorCanvas NSIS Modern Installer Script
; Produces a Windows installer with Start Menu, Desktop shortcuts, Uninstaller, and file association

!include "MUI2.nsh"
!include "FileFunc.nsh"

; General Configuration
Name "CreatorCanvas"
OutFile "CreatorCanvas-0.2.0-Setup.exe"
InstallDir "$PROGRAMFILES64\CreatorCanvas"
InstallDirRegKey HKLM "Software\CreatorCanvas" "Install_Dir"
RequestExecutionLevel admin

; UI Configuration
!define MUI_ABORTWARNING
!define MUI_ICON "..\..\resources\icons\creatorcanvas.ico"
!define MUI_UNICON "..\..\resources\icons\creatorcanvas.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN "$INSTDIR\CreatorCanvas.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Iniciar o CreatorCanvas agora"
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "PortugueseBR"
!insertmacro MUI_LANGUAGE "English"

Section "CreatorCanvas Core" SecCore
    SetOutPath "$INSTDIR"
    
    ; Copy all files from staging directory
    File /r "CreatorCanvas-portable\*.*"
    
    ; Registry keys for install location
    WriteRegStr HKLM "Software\CreatorCanvas" "Install_Dir" "$INSTDIR"
    
    ; Add or Remove Programs (Windows Control Panel & Settings)
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "DisplayName" "CreatorCanvas"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "DisplayVersion" "0.2.0"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "DisplayIcon" "$INSTDIR\CreatorCanvas.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "Publisher" "CreatorCanvas Team"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas" "NoRepair" 1

    ; Register .creatorcanvas file association
    WriteRegStr HKCR ".creatorcanvas" "" "CreatorCanvas.Project"
    WriteRegStr HKCR "CreatorCanvas.Project" "" "CreatorCanvas Project File"
    WriteRegStr HKCR "CreatorCanvas.Project\DefaultIcon" "" "$INSTDIR\CreatorCanvas.exe,0"
    WriteRegStr HKCR "CreatorCanvas.Project\shell\open\command" "" '"$INSTDIR\CreatorCanvas.exe" "%1"'

    ; Write Uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Start Menu Shortcuts
    CreateDirectory "$SMPROGRAMS\CreatorCanvas"
    CreateShortcut "$SMPROGRAMS\CreatorCanvas\CreatorCanvas.lnk" "$INSTDIR\CreatorCanvas.exe"
    CreateShortcut "$SMPROGRAMS\CreatorCanvas\Uninstall CreatorCanvas.lnk" "$INSTDIR\uninstall.exe"
    
    ; Desktop Shortcut
    CreateShortcut "$DESKTOP\CreatorCanvas.lnk" "$INSTDIR\CreatorCanvas.exe"
SectionEnd

Section "Uninstall"
    ; Remove registry keys
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\CreatorCanvas"
    DeleteRegKey HKLM "Software\CreatorCanvas"
    DeleteRegKey HKCR ".creatorcanvas"
    DeleteRegKey HKCR "CreatorCanvas.Project"

    ; Remove shortcuts
    Delete "$DESKTOP\CreatorCanvas.lnk"
    Delete "$SMPROGRAMS\CreatorCanvas\*.*"
    RMDir "$SMPROGRAMS\CreatorCanvas"

    ; Remove installed files
    RMDir /r "$INSTDIR"
SectionEnd
