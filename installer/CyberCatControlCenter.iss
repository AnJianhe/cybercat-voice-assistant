#define AppName "赛博猫猫控制中心"
#define AppVersion "1.1.0"

[Setup]
AppId={{BA0CBE0F-4977-4D6A-AC65-422096274E9D}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=CyberCat
DefaultDirName={localappdata}\Programs\CyberCatControlCenter
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=no
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=CyberCatControlCenter-Setup-1.1.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\CyberCatControlCenter.exe
CloseApplications=yes
RestartApplications=no
VersionInfoVersion=1.1.0.0
VersionInfoDescription={#AppName} 安装程序
InfoBeforeFile=安装说明.txt

[Languages]
Name: "chinesesimp"; MessagesFile: "ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："

[Files]
Source: "..\CyberCatControlCenter.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "安装说明.txt"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\CyberCatControlCenter.exe"; WorkingDir: "{app}"
Name: "{group}\卸载{#AppName}"; Filename: "{uninstallexe}"
Name: "{group}\使用说明"; Filename: "{app}\安装说明.txt"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\CyberCatControlCenter.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\CyberCatControlCenter.exe"; Description: "启动{#AppName}"; Flags: nowait postinstall skipifsilent

; 不使用 [UninstallDelete] 通配删除，保留用户在安装目录保存的日志及其他文件。
