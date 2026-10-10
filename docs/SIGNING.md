# Code Signing

Guidelines for code signing HARP's release packages: what each platform requires, how to set up
the credentials, how to hand them off to a new maintainer, and what tends to go wrong. The
[build workflow](../.github/workflows/build.yml) does the signing itself, so the work described
here is mostly creating accounts and certificates, and storing the results as repository secrets.

## Table of Contents

- [Overview](#overview)
- [macOS](#macos)
  - [Signing and Notarization](#signing-and-notarization)
  - [Choosing an Account](#choosing-an-account)
  - [Setting Up](#setting-up)
  - [Renewing and Handing Off](#renewing-and-handing-off)
  - [Troubleshooting](#troubleshooting)
- [Windows](#windows)
  - [Options](#options)
  - [Setting Up Artifact Signing](#setting-up-artifact-signing)
  - [Handing Off Artifact Signing](#handing-off-artifact-signing)
- [Linux](#linux)

## Overview

Operating systems treat software downloaded from the internet differently depending on whether it
is signed:

- **macOS** refuses to open an app unless it is signed with a Developer ID certificate _and_
  notarized by Apple. Users can override this in System Settings → Privacy & Security, but a
  release should never require it.
- **Windows** shows a SmartScreen warning ("Windows protected your PC") for an executable from an
  unknown publisher. Users can get past it with "More info" → "Run anyway".
- **Linux** does not check signatures.

The credentials are stored in the repository under Settings → Secrets and variables → Actions,
which requires admin access to the repository.

**Each signing step runs only when its credentials are present.** A build without them, such as one
for a pull request from a fork, still succeeds, and produces unsigned packages. A build with only
some of them either fails or produces packages that are signed but still blocked.

**Secrets cannot be read back once saved**, not even by an admin. Anything that will be needed
again has to be kept elsewhere (see [Renewing and Handing Off](#renewing-and-handing-off)).

## macOS

### Signing and Notarization

macOS requires two separate things of a downloaded app, and the credentials cover both:

1. **Signing** proves who made the app and that it has not been modified since. It uses a
   _Developer ID Application_ certificate and its private key, which the workflow imports from a
   password-protected `.p12` file.
2. **Notarization** has Apple scan the signed app for malware. The workflow uploads the DMG to
   Apple's notary service, waits for it to be accepted, and _staples_ the resulting ticket to the
   DMG, so that Gatekeeper can verify it even offline.

Uploading to the notary service requires signing in to Apple as a member of the developer team that
signed the app. The certificate alone is not enough. Apple Accounts require two-factor
authentication, which an automated build cannot complete, so Apple issues an _app-specific
password_ for this purpose in place of the account's real password.

| Secret | Contents | Used For |
|---|---|---|
| `MACOS_CERTIFICATE` | The Developer ID Application certificate and private key, as a base64-encoded `.p12` file | Signing |
| `MACOS_CERTIFICATE_PASSWORD` | The password used to export the `.p12` file | Signing |
| `APPLE_ID` | The email address of an Apple Account on the developer team | Notarization |
| `APPLE_APP_PASSWORD` | An app-specific password for that Apple Account | Notarization |

The signing identity and the team ID are both read from the certificate, so they need no secrets of
their own.

### Choosing an Account

Developer ID certificates require a paid membership of the
[Apple Developer Program](https://developer.apple.com/programs/) (US$99 per year), since a free
Apple Account cannot create them. A membership is either individual or organization, which decides
whose name the certificate carries and how easily it changes hands.

**Individual memberships are the quickest to set up.** One person enrolls with their own Apple
Account, and the certificate names them (_e.g._ `Developer ID Application: Jane Doe (ABCDE12345)`).
The membership belongs to that person, and Apple transfers one only in exceptional circumstances, so
when they leave the project, a successor has to enroll separately and replace the credentials (see
[Renewing and Handing Off](#renewing-and-handing-off)).

**Organization memberships outlive any one person's involvement.** The certificate carries the
organization's name, and the team can have several members with different
[roles](https://developer.apple.com/help/account/manage-your-team/roles/). The requirements are
stricter (see [Apple's enrollment page](https://developer.apple.com/programs/enroll/)):

- The organization must be a legal entity that can enter into contracts with Apple. DBAs, trade
  names, and branches are not accepted, so an informal group such as a research lab can only enroll
  through the institution or company to which it belongs.
- It needs a D-U-N-S number, which Dun & Bradstreet assigns for free (many institutions already
  have one), a public website on its own domain, and a work email address on that domain.
- The person enrolling must have the legal authority to bind the organization, and becomes its
  _Account Holder_.
- Only the Account Holder can create Developer ID certificates, and the team can have at most five
  at a time. The role can be
  [transferred](https://developer.apple.com/help/account/manage-your-team/transfer-the-account-holder-role/)
  to another member with the same legal authority.
- Nonprofits, accredited educational institutions, and government entities that do not sell anything
  through their apps can [request a fee waiver](https://developer.apple.com/support/fee-waiver/).

An individual membership can also be converted to an organization membership by contacting Apple,
provided its holder founded the organization and it has a D-U-N-S number. Confirm with Apple whether
existing certificates carry over before relying on them.

A university, or another institution hosting a project, may already have an organization
membership. Its Account Holder would have to create the certificate, and since a Developer ID
certificate can sign anything in the institution's name, they may be reluctant to hand out its
`.p12` file.

### Setting Up

These steps are the same for a first setup, a renewal, and a handoff. Steps 2 to 6 have to be done
on the same Mac, since the private key is created there and never leaves it until the export.

1. Enroll in the Apple Developer Program, and accept any pending agreements at
   [developer.apple.com/account](https://developer.apple.com/account). Notarization fails while an
   agreement is outstanding.
2. Open Keychain Access (with Spotlight, since recent versions of macOS no longer list it in the
   Utilities folder) and choose Keychain Access → Certificate Assistant → Request a Certificate From
   a Certificate Authority. Enter the account's email address and select "Saved to disk".
3. As the Account Holder, open the
   [new certificate page](https://developer.apple.com/account/resources/certificates/add), choose
   **Developer ID Application** with the G2 Sub-CA, and upload the request. Download the certificate
   and double-click it to add it to the login keychain.
4. Unless Xcode is installed, also download and double-click Apple's
   [Developer ID - G2](https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer) intermediate
   certificate, which Xcode installs but the Command Line Tools do not. The certificate cannot sign
   anything without it.
5. Check that `security find-identity -v -p codesigning` lists `Developer ID Application: ...` as a
   valid identity. If it does not, see [Troubleshooting](#troubleshooting).
6. In Keychain Access, under login → My Certificates, expand the certificate to check that a private
   key is underneath. Then right-click the certificate and export it as a `.p12` file with a strong
   password.
7. Signed in as the Apple Account that will submit notarizations, create an app-specific password
   at [account.apple.com](https://account.apple.com) → Sign-In and Security → App-Specific
   Passwords.
8. Package and notarize a build locally (see [Distribution](../README.md#distribution)), which
   reports problems sooner and more clearly than the workflow. The first submissions from a new
   account can take much longer than usual.
9. Set the four secrets listed under [Signing and Notarization](#signing-and-notarization). The
   value of `MACOS_CERTIFICATE` is the output of `base64 -i <FILE>.p12`.
10. Push a commit, download the DMG from the workflow run, and check it:

    ```bash
    spctl -a -vv -t open --context context:primary-signature <FILE>.dmg
    ```

    The output should include `accepted` and `source=Notarized Developer ID`.

### Renewing and Handing Off

**Certificates are valid for five years**, though never beyond the expiry of Apple's intermediate
certificate (September 2031 for G2). Packages signed while the certificate was valid keep working
after it expires, so only new builds need a new one. Create it as under [Setting Up](#setting-up),
and replace `MACOS_CERTIFICATE` and `MACOS_CERTIFICATE_PASSWORD`.

**Revoke a certificate only if its private key has been compromised.** Apple blocks every app signed
with a revoked certificate, including copies users have already installed. Developer ID certificates
cannot be revoked from the developer account. Instead, Apple's
[certificate support page](https://developer.apple.com/support/certificates/) asks for revocation
requests by email to product-security@apple.com. A certificate that is no longer needed, including
one whose private key was lost, can simply be left to expire.

**Keep the membership active.** If it lapses, existing releases keep working, but no new
certificates can be created, and notarization requires an active membership.

When signing passes to a new maintainer:

- **Within an organization membership**, add the new maintainer to the team. They create their own
  app-specific password and replace `APPLE_ID` and `APPLE_APP_PASSWORD`. The certificate can stay,
  as long as its `.p12` file and password are still available, and the Account Holder role is
  transferred if the outgoing maintainer held it.
- **Between individual memberships**, the new maintainer enrolls, creates a certificate, and
  replaces all four secrets at once, since the certificate and the Apple Account have to belong to
  the same team. Releases signed by the previous team keep working.

Either way:

- **Keep the `.p12` file and its password in a password manager shared by the maintainers**, or
  delete them and create a new certificate whenever one is needed. GitHub cannot return them.
- **App-specific passwords stop working when the Apple Account's password changes**, so whoever
  owns `APPLE_ID` should know that it is in use.
- **Expect users to be asked for permissions again after a change of team.** macOS ties some
  permissions, such as access to the Documents folder, to the team that signed the app.

### Troubleshooting

- **`0 valid identities found`:** run `security find-identity -p codesigning` without `-v`, which
  also lists identities that are not valid.
  - If the certificate is listed under "Matching identities", the intermediate certificate is
    missing (step 4 of [Setting Up](#setting-up)). Signing then fails with
    `unable to build chain to self-signed root`.
  - If it is not listed at all, the keychain has the certificate but not its private key.
- **No private key under the certificate:** the private key exists only on the Mac and user account
  that made the request, so downloading the certificate again does not restore it. Import a `.p12`
  file exported from that Mac, if one exists. Otherwise, delete the certificate from the keychain
  and make a new request and certificate. The old certificate is unusable without its key, so leave
  it to expire rather than revoking it. It still counts toward the team's limit of five, and
  [Developer Program Support](https://developer.apple.com/contact/) can help if that limit is
  reached.
- **No Developer ID Application option:** the membership is not active yet, or the signed-in user
  is not the Account Holder.
- **Exporting as `.p12` is unavailable:** the certificate was selected under Certificates rather
  than My Certificates, or its private key is missing.
- **Notarization fails with `HTTP status code: 403`:** an agreement needs to be accepted in the
  developer account.
- **Notarization fails with `HTTP status code: 401`:** the Apple Account and app-specific password
  do not match, or the account is not on the team that signed the app.
- **Notarization finishes with `status: Invalid`:**
  `xcrun notarytool log <SUBMISSION_ID>`, with the same credentials as the submission, explains why.
- **`The specified item could not be found in the keychain`:** no certificate matches
  `MACOS_SIGNING_IDENTITY` when packaging locally, or the certificate was not imported in the
  workflow.
- **`ambiguous`:** several certificates match `MACOS_SIGNING_IDENTITY`, such as an old and a new one
  with the same name. Use the SHA-1 hash listed by `security find-identity` instead.

## Windows

### Options

| Option | Cost | Publisher Shown | Limitations |
|---|---|---|---|
| [Azure Artifact Signing](https://learn.microsoft.com/azure/artifact-signing/) | About US$10 per month | The validated organization or individual | Organizations in the US, Canada, the EU, the UK, Australia, New Zealand, Japan, South Korea, Singapore, Switzerland, Norway, and Israel, or individuals in the US and Canada. |
| [Microsoft Store](https://blogs.windows.com/windowsdeveloper/2025/09/10/free-developer-registration-for-individual-developers-on-microsoft-store/) | Free for individuals | The developer | Signs only the copies installed from the Store. |
| [SignPath Foundation](https://signpath.org) | Free for open-source projects | SignPath Foundation | Excludes HARP (see below). |
| Certificate authority | Annual fee | The validated organization or individual | The private key has to be kept on a hardware token or cloud HSM, which is hard to use from a workflow. |

The workflow uses Artifact Signing (formerly Trusted Signing), which is the cheapest option that
signs the packages it publishes.

**There is no free way to sign those packages:**

- **The Microsoft Store** has been free for individual developers since September 2025, after an ID
  check, and signs apps packaged as MSIX itself. It also gives users one-click installation and
  automatic updates. However, only users who install from the Store benefit, and the workflow would
  have to build an MSIX package and submit each release for review. An MSIX app is installed into a
  protected folder, so a DAW that launches HARP by its path would have to use the app execution
  alias the Store creates instead.
- **SignPath Foundation** signs open-source projects for free, with a certificate issued to SignPath
  Foundation, a manual approval for every release, and a code signing policy on the project's home
  page. Its [terms](https://signpath.org/terms) exclude projects with any component under commercial
  dual licensing, which includes JUCE (AGPLv3 or commercial).

**Signing does not silence SmartScreen straight away.** It replaces the unknown publisher with the
publisher's name, but SmartScreen may keep warning about a new publisher until its downloads build
up reputation.

### Setting Up Artifact Signing

1. Create an [Azure](https://portal.azure.com) account with a pay-as-you-go subscription. For an
   individual certificate, the billing account must be of type Individual, with a legal name and
   address that match a government-issued ID.
2. Under Subscriptions → (the subscription) → Settings → Resource providers, select
   `Microsoft.CodeSigning` and choose **Register**.
3. Search the portal for **Artifact Signing Accounts** and choose **Create**. Create a new resource
   group, choose an account name and region (_e.g._ East US), and select the Basic pricing tier.
   Note the
   [endpoint of the region](https://learn.microsoft.com/azure/artifact-signing/quickstart#azure-regions-that-support-artifact-signing)
   (_e.g._ `https://eus.codesigning.azure.net/` for East US).
4. In the new account, open Access control (IAM) and assign yourself the **Artifact Signing Identity
   Verifier** role.
5. Under Identity validations, choose **New Identity** → **Public**, as either an organization or an
   individual, fill in the form, and follow the instructions sent by email. An organization takes 1
   to 20 business days to validate, and an individual verifies their ID with Microsoft
   Authenticator.
6. Once the validation is complete, go to Certificate profiles → **Create** → **Public Trust**,
   enter a profile name, and select the validated identity.
7. Create credentials for the workflow under Microsoft Entra ID → App registrations → **New
   registration**. Note the Application (client) ID and Directory (tenant) ID, then create a client
   secret under Certificates & secrets and copy its value.
8. In the signing account's Access control (IAM), assign the **Artifact Signing Certificate Profile
   Signer** role to the app registration.
9. Set these repository secrets:

   | Secret | Value |
   |---|---|
   | `AZURE_TENANT_ID` | Directory (tenant) ID from step 7 |
   | `AZURE_CLIENT_ID` | Application (client) ID from step 7 |
   | `AZURE_CLIENT_SECRET` | Client secret value from step 7 |

   And these repository variables, on the Variables tab next to Secrets:

   | Variable | Value |
   |---|---|
   | `AZURE_SIGNING_ENDPOINT` | Endpoint from step 3 |
   | `AZURE_SIGNING_ACCOUNT` | Account name from step 3 |
   | `AZURE_CERTIFICATE_PROFILE` | Profile name from step 6 |

10. Push a commit, download the Windows package from the workflow run, and check that `HARP.exe`
    lists the signature under Properties → Digital Signatures, or that
    `Get-AuthenticodeSignature HARP.exe` reports `Valid` in PowerShell.

The certificates Artifact Signing issues are valid for only a few days, which is why the workflow
timestamps each signature. A timestamped signature stays valid after its certificate expires.

### Handing Off Artifact Signing

- **Grant access rather than recreating resources.** A new maintainer can be given the Owner role on
  the resource group under Access control (IAM). Someone has to keep paying for the subscription, or
  transfer its billing.
- **The certificate names whoever passed identity validation.** Signing as a different individual or
  organization requires a new identity validation and certificate profile, after which
  `AZURE_CERTIFICATE_PROFILE` is updated.
- **Client secrets expire, after at most 24 months.** Create a new one under Certificates & secrets
  and replace `AZURE_CLIENT_SECRET` before the old one expires.

## Linux

Linux does not check signatures, so no setup is needed.

Instead, the system used to build the Linux packages limits where they run. A Linux binary requires
at least the versions of glibc and libstdc++ used to build it, so the workflow builds on the oldest
Ubuntu available, 22.04 (glibc 2.35, and libstdc++ from GCC 12). The packages therefore run on these
distributions:

- Ubuntu 22.04 or newer, and distributions based on it, such as Linux Mint 21 and Pop!_OS 22.04
- Debian 12 or newer, including 64-bit Raspberry Pi OS (with the `arm64` package)
- Fedora 36 or newer, RHEL 10 and its rebuilds, openSUSE Tumbleweed and Leap 16, and Arch Linux

They do not run on Ubuntu 20.04, Debian 11, RHEL 8 or 9, or openSUSE Leap 15. Reaching those would
mean building inside an older Linux container, and reaching every distribution would mean
publishing a Flatpak.

At runtime, HARP needs ALSA, FreeType, and X11, and loads libcurl for network requests, all of
which come with a standard desktop installation. On Wayland, it runs through XWayland, which GNOME
and KDE enable by default.
