# Code signing and notarization

The builds are **unsigned today**. macOS Gatekeeper shows "cannot be
opened because the developer cannot be verified" and Windows SmartScreen
shows "Windows protected your PC" until this is done. Everything in the
build is ready for it; what is missing is credentials, and credentials
have to be bought by a person with a legal identity — they cannot be
generated, borrowed, or worked around.

This file says exactly what to buy and what to do with it. The CI
workflow already has the steps; they are skipped when the secrets are
absent, so adding the secrets is the whole switch.

## What each platform actually requires

| | What you buy | Cost | Renews |
|---|---|---|---|
| **macOS** | Apple Developer Program membership → a *Developer ID Application* certificate and a *Developer ID Installer* certificate | $99/year | yearly |
| **Windows** | An OV or EV code-signing certificate from a CA (Sectigo, DigiCert, SSL.com …) | ~$200–600/year | 1–3 years |
| **Linux** | Nothing. There is no equivalent gate. | — | — |

Two things worth knowing before spending money:

- **Windows OV vs EV.** OV is cheaper and can be automated, but SmartScreen
  still warns until the certificate builds reputation across enough
  downloads. EV clears SmartScreen immediately, costs more, and since June
  2023 must live on a hardware token or an approved cloud HSM — which
  means CI signing needs a cloud signing service, not a file in a secret.
- **macOS notarization is separate from signing** and is the part
  Gatekeeper actually checks. Signing without notarizing still warns.

## macOS

1. Join the Apple Developer Program.
2. In Xcode → Settings → Accounts, create **Developer ID Application** and
   **Developer ID Installer** certificates. Export both as one `.p12`.
3. Create an app-specific password at appleid.apple.com for notarization.
4. Add these repository secrets:

   | Secret | What it is |
   |---|---|
   | `MACOS_CERTIFICATE_P12` | the `.p12`, base64-encoded |
   | `MACOS_CERTIFICATE_PASSWORD` | its export password |
   | `MACOS_SIGN_IDENTITY` | e.g. `Developer ID Application: Your Name (TEAMID)` |
   | `MACOS_INSTALLER_IDENTITY` | e.g. `Developer ID Installer: Your Name (TEAMID)` |
   | `APPLE_ID` | your Apple ID email |
   | `APPLE_TEAM_ID` | the 10-character team ID |
   | `APPLE_APP_PASSWORD` | the app-specific password |

Sign order matters and is easy to get wrong: **plugins and the app first,
then the installer**. A `.pkg` signed before its payload is signed will
notarize and still refuse to launch, because the thing inside it is
unsigned.

```bash
# every bundle, deepest first
codesign --force --options runtime --timestamp \
         --sign "$MACOS_SIGN_IDENTITY" <each .vst3 / .component / .app>

# then the installer
productsign --sign "$MACOS_INSTALLER_IDENTITY" unsigned.pkg abcTrain.pkg

# then notarize and staple, so it works offline too
xcrun notarytool submit abcTrain.dmg --apple-id "$APPLE_ID" \
      --team-id "$APPLE_TEAM_ID" --password "$APPLE_APP_PASSWORD" --wait
xcrun stapler staple abcTrain.dmg
```

`--options runtime` is not optional: notarization rejects anything not
built with the hardened runtime.

## Windows

With an OV certificate as a `.pfx`:

| Secret | What it is |
|---|---|
| `WINDOWS_CERTIFICATE_PFX` | the `.pfx`, base64-encoded |
| `WINDOWS_CERTIFICATE_PASSWORD` | its password |

```powershell
signtool sign /f cert.pfx /p $env:WINDOWS_CERTIFICATE_PASSWORD `
              /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 `
              abcTrain-setup.exe
```

The timestamp is what keeps already-downloaded builds valid after the
certificate expires. Signing without one means every release stops
verifying the day the certificate lapses.

With an EV certificate the private key is on hardware and `signtool`
cannot reach it from CI. The options are a cloud signing service
(Azure Trusted Signing, SSL.com eSigner, DigiCert KeyLocker) or signing
on a machine with the token attached.

## Until then

The README says plainly that the builds are unsigned and what the warning
looks like, with the click-through for each OS. That is the honest
position: telling someone how to bypass a security warning is only
acceptable when you also tell them *why* it is there.
