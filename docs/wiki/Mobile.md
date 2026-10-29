# abcTrain on a phone or tablet

*[Русская версия](ru-Mobile)*

The mobile version is the same app: the nine exercises, the threshold
staircase, the lessons, Live as a participant (joining a seminar, battles).
Its layout is made for a thumb. What a phone cannot have is left out: your
own VST3/AU plugins and hosting a seminar.

## Android - the APK from a release

1. On the phone, open the [latest release](https://github.com/bogggare567/abcTrain/releases/latest)
   and download `abcTrain-…-android.apk`.
2. Open it. Android asks whether this source (the browser or Files) may
   install apps: allow it.
3. Install. Android 8.0 or newer.

**Updating:** install the new APK over the old one; progress stays. If
Android says "App not installed" or reports a signature conflict, the old
build was signed with another key (test builds can be): uninstall it
first. That loses local progress, so sign in to Live and sync before.

Google Play comes later.

## iPhone and iPad - build it with Xcode (free)

Without the App Store the only way onto an iPhone is your own build. A free
Apple account signs it for **7 days**; after that the app stops opening
until you build it again. Progress is kept.

You need a Mac with Xcode 16+ and a cable.

1. **Xcode**: install it from the App Store and open it once. Xcode →
   Settings → Accounts → + → Apple ID. A "(Personal Team)" appears; its
   10-character ID is shown under Manage Certificates, or in the signing
   error of a first build.
2. **Source and CMake**:
   ```
   brew install cmake git
   git clone https://github.com/bogggare567/abcTrain.git
   cd abcTrain
   cmake -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DABCTRAIN_IOS_TEAM=YOUR_TEAM_ID
   open build-ios/EarTrainer.xcodeproj
   ```
3. **The phone**: connect it, unlock it, tap Trust. On iOS 16+ turn on
   Settings → Privacy & Security → **Developer Mode** and restart.
4. **Build**: pick the **EarTrainer_Standalone** scheme and your phone at
   the top of Xcode, press ▶. The first build takes 5-10 minutes.
5. **Trust**: on first launch the iPhone says "Untrusted Developer":
   Settings → General → **VPN & Device Management** → your Apple ID → Trust.

After 7 days: connect the phone and press ▶ again (`git pull` first for
the latest version).

**Your own sounds on an iPhone:** the app shows up in Files → On My iPhone
→ abcTrain; put a folder of sounds or a pack there.

TestFlight (install by link, no Xcode, no 7 days) needs the paid Apple
Developer Program ($99/year). The link goes here once there is one.

## For the repository owner: the Android signing key

APKs from different releases update one another only if they are signed
with the same key. Make it once and keep it safe - a lost key means
everyone reinstalls.

```
keytool -genkeypair -v -keystore abctrain.keystore -alias abctrain \
  -keyalg RSA -keysize 4096 -validity 36500
base64 -i abctrain.keystore | pbcopy
```

GitHub → Settings → Secrets and variables → Actions:
`ANDROID_KEYSTORE_BASE64` (the copied text), `ANDROID_KEYSTORE_PASS`,
`ANDROID_KEY_ALIAS` (`abctrain`), `ANDROID_KEY_PASS`. Without them the
Android workflow signs with a throwaway key and warns about it.
