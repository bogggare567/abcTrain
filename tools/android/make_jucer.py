#!/usr/bin/env python3
"""Android project for abcTrain, generated from the CMake build (ADR 071).

JUCE's CMake API cannot target Android; only the Projucer can. A
hand-kept .jucer would be a second list of every source file, and it would
drift from CMakeLists.txt within a week. So this script reads what CMake
already knows - compile_commands.json of a host build - and writes the
Projucer project from it:

  - the sources: every file CMake compiles into the EarTrainer target,
    minus JUCE's own modules (the Projucer adds those);
  - the modules: the JUCE modules that target compiles, plus the
    plugin client for the Standalone wrapper;
  - the binary data (strings, fonts, lessons, sounds, icons, licence):
    the sources juceaide generated for each juce_add_binary_data target,
    copied next to the project under their own namespaces, exactly as the
    desktop build compiles them;
  - the version (versions.json "app") and the Android version code.

Usage (CI does this - .github/workflows/android.yml):

    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    cmake --build build --target I18nData FontData SampleData LessonData UiSoundData BrandData
    python3 tools/android/make_jucer.py --build build --juce build/_deps/juce-src --out build-android
    Projucer --resave build-android/abcTrain.jucer
    cd build-android/Builds/Android && ./gradlew assembleRelease

Signing: ANDROID_KEYSTORE (path), ANDROID_KEYSTORE_PASS, ANDROID_KEY_ALIAS,
ANDROID_KEY_PASS from the environment. Without them the APK is signed with
the build machine's debug key, which installs fine but cannot update an
APK signed with another key - see docs/wiki/Mobile.md.
"""

import argparse
import json
import os
import re
import shutil
import sys
import uuid
from pathlib import Path
from xml.sax.saxutils import quoteattr

ROOT = Path(__file__).resolve().parents[2]

# Compile definitions CMake sets that the Projucer sets itself (per-target
# plugin macros, module availability) or that belong to the desktop.
SKIP_DEFINE = re.compile(
    r"^(JucePlugin_|JUCE_MODULE_AVAILABLE_|JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED|JUCE_SHARED_CODE|"
    r"JUCE_STANDALONE_APPLICATION|JUCE_TARGET_HAS_BINARY_DATA|JUCE_PLUGINHOST_|JUCE_USE_CURL|"
    r"JUCE_ALSA|JUCE_JACK|JUCE_USE_X|LINUX|NDEBUG|_NDEBUG|DEBUG|_DEBUG)")

BINARY_DATA = ["I18nData", "FontData", "SampleData", "LessonData", "UiSoundData", "BrandData"]


def uid():
    return uuid.uuid4().hex[:6]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--build", required=True, help="host CMake build dir (with compile_commands.json)")
    ap.add_argument("--juce", required=True, help="JUCE source dir")
    ap.add_argument("--out", required=True, help="where to write abcTrain.jucer")
    args = ap.parse_args()

    build = Path(args.build).resolve()
    juce = Path(args.juce).resolve()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)

    commands = json.loads((build / "compile_commands.json").read_text())
    target = [c for c in commands if "/EarTrainer.dir/" in c["output"].replace("\\", "/")]
    if not target:
        sys.exit("no EarTrainer sources in compile_commands.json - configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON")

    sources, modules, defines = [], {"juce_audio_plugin_client"}, {}
    for c in target:
        f = c["file"].replace("\\", "/")
        m = re.search(r"/modules/(juce_\w+)/", f)
        if m:
            modules.add(m.group(1))
        elif f not in sources:
            sources.append(f)
        for d in re.findall(r"-D(\S+)", c["command"] if "command" in c else " ".join(c["arguments"])):
            name, _, value = d.partition("=")
            if not SKIP_DEFINE.match(name):
                defines[name] = value or "1"

    # Binary data: juceaide's output for each target, under its own folder
    # (every target has a BinaryData1.cpp).
    generated = out / "generated"
    shutil.rmtree(generated, ignore_errors=True)
    header_paths = [str(ROOT), str(generated)]
    binary_sources = []
    for t in BINARY_DATA:
        src = build / f"juce_binarydata_{t}" / "JuceLibraryCode"
        if not src.is_dir():
            sys.exit(f"{src} is missing - build the {t} target first")
        dst = generated / t
        shutil.copytree(src, dst)
        header_paths.append(str(dst))
        binary_sources += sorted(str(p) for p in dst.glob("*.cpp"))
    shutil.copy(build / "generated" / "shared" / "VersionInfo.h", (generated / "shared").mkdir(exist_ok=True) or generated / "shared" / "VersionInfo.h")

    versions = json.loads((ROOT / "versions.json").read_text())
    version = versions["app"]
    major, minor, patch = (int(x) for x in version.split("."))
    version_code = major * 10000 + minor * 100 + patch

    # The module options CMake sets as plain definitions become the
    # project's JUCEOPTIONS (the Projucer writes them where modules see them).
    options = {k: v for k, v in defines.items() if k.startswith("JUCE_")}
    others = {k: v for k, v in defines.items() if not k.startswith("JUCE_")}
    options.update({"JUCE_USE_CURL": "0", "JUCE_WEB_BROWSER": "0", "JUCE_VST3_CAN_REPLACE_VST2": "0"})

    def rel(p):
        return os.path.relpath(p, out).replace("\\", "/")

    def file_item(path, compile_=True):
        return (f'      <FILE id="{uid()}" name={quoteattr(Path(path).name)} compile="{1 if compile_ else 0}" '
                f'resource="0" file={quoteattr(rel(path))}/>')

    icon_big, icon_small = uid(), uid()
    groups = [
        "    <GROUP id=\"src\" name=\"Source\">",
        *[file_item(s) for s in sources],
        "    </GROUP>",
        "    <GROUP id=\"bin\" name=\"BinaryData\">",
        *[file_item(s) for s in binary_sources],
        "    </GROUP>",
        "    <GROUP id=\"res\" name=\"Resources\">",
        f'      <FILE id="{icon_big}" name="eartrainer.png" compile="0" resource="0" file={quoteattr(rel(ROOT / "assets/icons/eartrainer.png"))}/>',
        f'      <FILE id="{icon_small}" name="eartrainer_small.png" compile="0" resource="0" file={quoteattr(rel(ROOT / "assets/icons/eartrainer_small.png"))}/>',
        "    </GROUP>",
    ]

    keystore = os.environ.get("ANDROID_KEYSTORE", "")
    signing = ""
    if keystore:
        signing = (f' androidKeyStore={quoteattr(keystore)} androidKeyStorePass={quoteattr(os.environ.get("ANDROID_KEYSTORE_PASS", ""))}'
                   f' androidKeyAlias={quoteattr(os.environ.get("ANDROID_KEY_ALIAS", ""))}'
                   f' androidKeyAliasPass={quoteattr(os.environ.get("ANDROID_KEY_PASS", ""))}')
    else:
        print("note: no ANDROID_KEYSTORE - the APK is signed with this machine's debug key", file=sys.stderr)

    module_list = sorted(modules)
    headers = "&#10;".join(header_paths)
    defines_attr = "&#10;".join(f"{k}={v}" for k, v in sorted(others.items()))

    jucer = f"""<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated by tools/android/make_jucer.py from the CMake build. Do not edit: regenerate. -->
<JUCERPROJECT id="abcTrn" name="abcTrain" projectType="audioplug" useAppConfig="0"
              addUsingNamespaceToJuceHeader="0" jucerFormatVersion="1" version="{version}"
              companyName="soundkorb" companyWebsite="https://soundkorb.ru" bundleIdentifier="ru.soundkorb.abctrain"
              pluginFormats="buildStandalone" pluginName="ABC Ear Trainer" pluginDesc="ABC Ear Trainer"
              pluginManufacturer="soundkorb" pluginManufacturerCode="Sndk" pluginCode="Eqg1"
              pluginCharacteristicsValue="" cppLanguageStandard="17" defines="{defines_attr}">
  <MAINGROUP id="main" name="abcTrain">
{chr(10).join(groups)}
  </MAINGROUP>
  <EXPORTFORMATS>
    <ANDROIDSTUDIO targetFolder="Builds/Android" androidMinimumSDK="26" androidTargetSDK="35"
                   androidVersionCode="{version_code}" androidInternetNeeded="1" androidMicNeeded="0"
                   androidVibratePermission="1" androidScreenOrientation="unspecified"
                   bigIcon="{icon_big}" smallIcon="{icon_small}"{signing}>
      <CONFIGURATIONS>
        <!-- Debug first: the Projucer writes a Gradle build type for a
             release configuration only once it has seen the debug one. -->
        <CONFIGURATION isDebug="1" name="Debug" headerPath="{headers}"/>
        <CONFIGURATION isDebug="0" name="Release" optimisation="3" headerPath="{headers}"/>
      </CONFIGURATIONS>
      <MODULEPATHS>
{chr(10).join(f'        <MODULEPATH id="{m}" path={quoteattr(str(juce / "modules"))}/>' for m in module_list)}
      </MODULEPATHS>
    </ANDROIDSTUDIO>
  </EXPORTFORMATS>
  <MODULES>
{chr(10).join(f'    <MODULE id="{m}" showAllCode="1" useLocalCopy="0" useGlobalPath="0"/>' for m in module_list)}
  </MODULES>
  <JUCEOPTIONS {" ".join(f'{k}="{v}"' for k, v in sorted(options.items()))}/>
</JUCERPROJECT>
"""
    (out / "abcTrain.jucer").write_text(jucer, encoding="utf-8")
    print(f"{out / 'abcTrain.jucer'}: {len(sources)} sources, {len(binary_sources)} binary data files, "
          f"{len(module_list)} modules, version {version} ({version_code})")


if __name__ == "__main__":
    main()
