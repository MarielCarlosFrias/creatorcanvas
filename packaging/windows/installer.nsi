; CreatorCanvas NSIS Modern Installer Script
; Produces a Windows installer with Start Menu, Desktop shortcuts, Uninstaller, and file association

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"

; General Configuration
Name "CreatorCanvas"
OutFile "CreatorCanvas-0.2.0-Setup.exe"
InstallDir "$PROGRAMFILES64\CreatorCanvas"
InstallDirRegKey HKLM "Software\CreatorCanvas" "Install_Dir"
RequestExecutionLevel admin
; O instalador é gerado a partir da pasta criada por build_portable.ps1.
; Essa pasta já contém o executável, Qt, plugins e ONNX Runtime.
!define VCREDIST_URL "https://aka.ms/vs/17/release/vc_redist.x64.exe"
!define VCREDIST_FILE "$TEMP\CreatorCanvas-vc_redist.x64.exe"

!ifexist "CreatorCanvas-portable\CreatorCanvas.exe"
!else
    !error "CreatorCanvas-portable is missing. Run build_portable.ps1 before makensis."
!endif

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

; Verifica o runtime MSVC antes de iniciar o programa. As DLLs do Qt e do
; ONNX Runtime são instaladas acima; o único runtime de sistema necessário
; que pode faltar é o Visual C++ Redistributable x64.
Function .onInstSuccess
    ReadRegDWORD $0 HKLM "SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64" "Installed"
    ${If} $0 != 1
        MessageBox MB_ICONINFORMATION|MB_YESNO \
            "O Microsoft Visual C++ Redistributable x64 não está instalado. Deseja que o instalador o descarregue e instale automaticamente?" \
            IDYES download_vcredist IDNO runtime_skipped
    download_vcredist:
        DetailPrint "A descarregar o Microsoft Visual C++ Redistributable x64..."
        nsExec::ExecToLog 'powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "try { Invoke-WebRequest -Uri ''${VCREDIST_URL}'' -OutFile ''${VCREDIST_FILE}'' -UseBasicParsing; exit 0 } catch { exit 1 }"'
        Pop $1
        ${If} $1 == 0
            DetailPrint "A instalar o Microsoft Visual C++ Redistributable x64..."
            ExecWait '"${VCREDIST_FILE}" /install /quiet /norestart' $1
            Delete "${VCREDIST_FILE}"
            ${If} $1 != 0
                MessageBox MB_ICONEXCLAMATION "Não foi possível instalar automaticamente o runtime do Visual C++. O programa poderá não iniciar até que ele seja instalado manualmente."
            ${EndIf}
        ${Else}
            MessageBox MB_ICONEXCLAMATION "Não foi possível descarregar o runtime do Visual C++. Verifique a ligação à Internet e instale o Microsoft Visual C++ Redistributable x64 manualmente se o programa não iniciar."
        ${EndIf}
    runtime_skipped:
    ${EndIf}
FunctionEnd

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
