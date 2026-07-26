; abcTrain Windows installer (Inno Setup 6).
;
; Compiled by CI (`iscc installer\windows_setup.iss /DMyAppVersion=X.Y.Z`)
; after `cmake --build` has already produced every target's
; *_artefacts\Release tree - this script only packages that output, it
; doesn't build anything itself. See decisions/008-installers.md for the
; overall design (why this half of the installer story can offer a real
; free-text custom path, unlike the macOS .pkg).
;
; NOT compiled/tested locally (no Windows in this environment) - written
; against standard, long-stable Inno Setup 6 syntax and verified on read-
; through, but CI is the first real compile of this file. Watch it.

#ifndef MyAppVersion
  ; Overridden by CI with /DMyAppVersion=<version> so the installer
  ; carries the same version the app reports (see CMakeLists.txt's
  ; abctrain-version.txt). This literal is only the local-build fallback.
  #define MyAppVersion "1.0.0"
#endif

#define MyAppName "abcTrain"
#define MyAppPublisher "EarSnap"
#define MyAppURL "https://github.com/bogggare567/abcTrain"
#define RepoRoot "..\"

[Setup]
AppId={{8F2B6C1E-4C3B-4B7A-9A1E-ABC7A19E0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\abcTrain
DefaultGroupName=abcTrain
DisableProgramGroupPage=yes
LicenseFile={#RepoRoot}LICENSE
OutputDir=.
OutputBaseFilename=abcTrain-{#MyAppVersion}-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Full installation (all plugins, VST3 + Standalone)"
Name: "compact"; Description: "Compact installation (all plugins, VST3 only)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

; Component tree: each plugin is a top-level checkbox; VST3/Standalone are
; its children. Unchecking a plugin's top-level box unchecks (and skips
; installing) both of its formats - checking/unchecking a parent and its
; children is standard Inno Setup component-tree behavior, no extra code
; needed for that part.
[Components]
Name: "eartrainer"; Description: "Ear Trainer - ear-training games (guess the boosted/cut band, compression strength, reverb type; points, levels, daily streak)"; Types: full compact custom
Name: "eartrainer\vst3"; Description: "VST3 format (Ableton Live, Cubase, Reaper, Studio One, and most other DAWs)"; Types: full compact custom
Name: "eartrainer\standalone"; Description: "Standalone app (runs on its own, no DAW needed)"; Types: full custom

Name: "learnereq"; Description: "Learner EQ - real 4-band EQ with live spectrum + response curve, a guided Lesson"; Types: full compact custom
Name: "learnereq\vst3"; Description: "VST3 format (Ableton Live, Cubase, Reaper, Studio One, and most other DAWs)"; Types: full compact custom
Name: "learnereq\standalone"; Description: "Standalone app (runs on its own, no DAW needed)"; Types: full custom

Name: "learnercomp"; Description: "Learner Comp - real compressor with live spectrum, GR/peak meters, 4 presets, a guided Lesson"; Types: full compact custom
Name: "learnercomp\vst3"; Description: "VST3 format (Ableton Live, Cubase, Reaper, Studio One, and most other DAWs)"; Types: full compact custom
Name: "learnercomp\standalone"; Description: "Standalone app (runs on its own, no DAW needed)"; Types: full custom

Name: "learnerverb"; Description: "Learner Verb - real reverb (Room/Hall/Plate/Spring) with live spectrum, 4 presets, a guided Lesson"; Types: full compact custom
Name: "learnerverb\vst3"; Description: "VST3 format (Ableton Live, Cubase, Reaper, Studio One, and most other DAWs)"; Types: full compact custom
Name: "learnerverb\standalone"; Description: "Standalone app (runs on its own, no DAW needed)"; Types: full custom

[Files]
; VST3 (JUCE builds these as bundle-style folders on Windows, same shape
; as the macOS .vst3 - hence the "\*" + recursesubdirs pattern rather than
; copying a single file).
Source: "{#RepoRoot}build\EarTrainer_artefacts\Release\VST3\Ear Trainer.vst3\*"; DestDir: "{code:GetVST3Dir}\Ear Trainer.vst3"; Components: eartrainer\vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#RepoRoot}build\LearnerEQ_artefacts\Release\VST3\Learner EQ.vst3\*"; DestDir: "{code:GetVST3Dir}\Learner EQ.vst3"; Components: learnereq\vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#RepoRoot}build\LearnerComp_artefacts\Release\VST3\Learner Comp.vst3\*"; DestDir: "{code:GetVST3Dir}\Learner Comp.vst3"; Components: learnercomp\vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#RepoRoot}build\LearnerVerb_artefacts\Release\VST3\Learner Verb.vst3\*"; DestDir: "{code:GetVST3Dir}\Learner Verb.vst3"; Components: learnerverb\vst3; Flags: recursesubdirs createallsubdirs ignoreversion

; Standalone .exe (single file, no bundle) into {app} (the directory
; chosen on the standard Select Destination Location page, default
; {pf}\abcTrain).
Source: "{#RepoRoot}build\EarTrainer_artefacts\Release\Standalone\Ear Trainer.exe"; DestDir: "{app}"; Components: eartrainer\standalone; Flags: ignoreversion
Source: "{#RepoRoot}build\LearnerEQ_artefacts\Release\Standalone\Learner EQ.exe"; DestDir: "{app}"; Components: learnereq\standalone; Flags: ignoreversion
Source: "{#RepoRoot}build\LearnerComp_artefacts\Release\Standalone\Learner Comp.exe"; DestDir: "{app}"; Components: learnercomp\standalone; Flags: ignoreversion
Source: "{#RepoRoot}build\LearnerVerb_artefacts\Release\Standalone\Learner Verb.exe"; DestDir: "{app}"; Components: learnerverb\standalone; Flags: ignoreversion

[Icons]
Name: "{group}\Ear Trainer"; Filename: "{app}\Ear Trainer.exe"; Components: eartrainer\standalone
Name: "{group}\Learner EQ"; Filename: "{app}\Learner EQ.exe"; Components: learnereq\standalone
Name: "{group}\Learner Comp"; Filename: "{app}\Learner Comp.exe"; Components: learnercomp\standalone
Name: "{group}\Learner Verb"; Filename: "{app}\Learner Verb.exe"; Components: learnerverb\standalone
Name: "{group}\Documentation && License"; Filename: "{#MyAppURL}"
Name: "{group}\Uninstall abcTrain"; Filename: "{uninstallexe}"

[Run]
; Both entries are unchecked by default and only offered if that build was
; actually installed - the user picks at most one, same spirit as the
; macOS DMG's "Open Plugins Folder.command" helper.
Filename: "{app}"; Description: "Open the install folder"; Flags: postinstall shellexec skipifsilent unchecked; Check: StandaloneInstalled
Filename: "{app}\Ear Trainer.exe"; Description: "Launch Ear Trainer"; Flags: postinstall nowait skipifsilent unchecked; Components: eartrainer\standalone

[Code]
var
  VST3DirPage: TInputDirWizardPage;

function AnyVST3Selected(): Boolean;
begin
  Result := IsComponentSelected('eartrainer\vst3')
    or IsComponentSelected('learnereq\vst3')
    or IsComponentSelected('learnercomp\vst3')
    or IsComponentSelected('learnerverb\vst3');
end;

function StandaloneInstalled(): Boolean;
begin
  Result := IsComponentSelected('eartrainer\standalone')
    or IsComponentSelected('learnereq\standalone')
    or IsComponentSelected('learnercomp\standalone')
    or IsComponentSelected('learnerverb\standalone');
end;

function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

procedure InitializeWizard;
begin
  // A second, custom directory page for VST3 specifically (the standard
  // Select Destination Location page only covers {app}, which this
  // script uses for the Standalone .exe copies) - defaults to the
  // conventional per-machine VST3 folder, but the user can retype it to
  // anything, same as {app} on the page before it.
  //
  // Anchored after wpSelectComponents (not wpSelectDir): the default
  // wizard order is Welcome -> License -> SelectDir -> SelectComponents,
  // so inserting after SelectComponents is what makes ShouldSkipPage's
  // IsComponentSelected() check below see the user's actual choice -
  // anchoring after SelectDir would run that check before the user had
  // picked any components yet.
  VST3DirPage := CreateInputDirPage(wpSelectComponents,
    'Select VST3 Plug-In Location', 'Where should VST3 plug-ins be installed?',
    'VST3 plug-ins will be installed in the following folder, so any VST3 host on this machine can find them.' + #13#10 +
    'To continue, click Next. If you would like to select a different folder, click Browse.',
    False, '');
  VST3DirPage.Add('');
  VST3DirPage.Values[0] := ExpandConstant('{commoncf}\VST3');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  // No VST3 format was selected on the Components page - nothing to ask.
  Result := False;
  if (PageID = VST3DirPage.ID) and (not AnyVST3Selected()) then
    Result := True;
end;
