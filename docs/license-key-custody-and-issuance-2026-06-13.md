# DuXun Film Simulation v5.0 License Key Custody And Issuance

Date: 2026-06-13

## Scope

This document defines the production private-key custody model and the manual offline license issuance process for DuXun Film Simulation v5.0 License MVP.

In scope:

- ECDSA P-256 private key storage and access control.
- Public/private key version management.
- Manual buyout license issuance from `activation-request.json`.
- Manual reissue, refusal, and revocation policy.
- A production-mvp dry run using a dedicated test key.

Out of scope:

- Online activation server.
- Runtime network checks.
- Automatic revocation enforcement.
- Changes to visual defaults, film matrices, HDR, OpenCL, Metal, GPU expansion, or LUT runtime loading.
- Modifying or replacing `DuXunFilmSim-OFX-v5.0.zip` or `DuXunFilmSim-OFX-v5.0-license-mvp.zip`.

## Release Anchors

- License MVP package: `DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- Package SHA256: `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX SHA256: `E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- Current signing scheme: ECDSA P-256, SHA-256, raw R||S signature encoded as base64.
- Current product id: `duxun-filmsim-ofx-v5`
- Current product version: `5.0`
- Current runtime key version: `v1`

The distributed plugin and user-side tools contain only public-key verification material. Private signing keys must never be copied into the repository, package directory, zip archive, customer machine, or support email attachment.

## Private Key Custody

### Storage Location

Production private keys must live outside the git repository and outside release-package directories.

Recommended local MVP location for the operator workstation:

```text
D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp\offline-private-key-v1.pem
```

Recommended structure:

```text
_private\duxun-filmsim-v5-production-mvp\
  operator-custody\
    offline-private-key-v1.pem
    public-key-v1.pem
  issuance-ledger\
    license-issuance-ledger-YYYY.md
  backups\
    offline-private-key-v1-YYYYMMDD.encrypted
```

The `_private` directory is intentionally outside the repository root used for `film-sim-plugin`. It must remain out of git.

### Backup Strategy

- Keep one primary encrypted copy on the operator workstation.
- Keep one offline encrypted backup on removable media stored separately from the workstation.
- Keep one sealed recovery copy controlled by the business owner.
- Verify each backup after creation by deriving the public key and comparing it with the recorded public-key fingerprint.
- Do not store plaintext private keys in cloud sync folders, chat uploads, issue trackers, email, or release artifacts.
- Rotate backup media and re-check readability at least before each public release.

Minimum backup record:

```text
keyVersion: v1
createdAt: YYYY-MM-DD
publicKeySha256: <sha256 of public-key-v1.pem>
backupId: <physical media label or vault item id>
verifiedBy: <operator>
```

Do not publish the private key contents. The private-key file hash may be kept in an offline operator ledger, but it should not be required in repo docs or customer-facing material.

### Access Control

Only authorized license operators may read the private key.

For the MVP, authorized roles are:

- Owner: may create, rotate, back up, and revoke keys.
- License operator: may sign licenses and update the issuance ledger.
- Reviewer: may inspect public-key fingerprints, issuance logs, and package hashes, but may not read the private key.

Recommended Windows controls:

```bat
icacls D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp /inheritance:r
icacls D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp /grant:r <operator-user>:(OI)(CI)F
```

The exact account names depend on the operator machine. Run `icacls` only after confirming the current user can still read the directory and the recovery copy is available.

### Operator Rules

- Sign licenses only from a clean working tree or a tagged release checkout.
- Use only the private key matching the public key embedded in the target package.
- Verify every generated `license.json` before sending it to the customer.
- Record every issuance decision, including refusals and reissues.
- Never paste private key material into a terminal transcript, document, chat, ticket, or screenshot.
- Never run the signer from inside a package directory that will be zipped for customers.

### Loss Or Compromise Consequences

If the private key is lost:

- Existing customer licenses remain valid.
- New licenses matching the embedded v1 public key cannot be issued.
- Reissues for existing v1 customers cannot be completed unless a backup exists.
- Recovery requires releasing a new build with a new public key and `keyVersion`.

If the private key is compromised:

- Stop issuing v1 licenses immediately.
- Mark v1 as compromised in the operator ledger.
- Generate a new keypair and release a new build with `keyVersion=v2`.
- Because the MVP has no online revocation, already-issued v1 licenses may continue to validate in v1 builds.
- Use support records and future builds to migrate legitimate customers to v2.

## Key Version Management

The MVP currently uses `keyVersion=v1`.

For each key version, maintain:

- Private key custody path.
- Public key PEM path.
- Public key SHA256.
- Embedded public key constants or source location.
- Package names and hashes shipped with that public key.
- First and last issuance dates.
- Retirement or compromise status.

Current source locations:

- Python tools: `scripts/duxun_license.py`, `PUBLIC_KEY_V1_X`, `PUBLIC_KEY_V1_Y`, `KEY_VERSION`.
- OFX runtime: `ofx/DuXunFilm/DuXunLicense.h`.

Key rotation rules:

- Planned rotation creates `keyVersion=v2` and a new package/build.
- Do not silently replace v1 private keys while keeping `keyVersion=v1`.
- If multiple active key versions are needed, update runtime and tools to verify each supported key version explicitly.
- Keep old private keys only as long as reissue support for old builds is required.
- A new package hash must be recorded for every package that embeds a new public key.

## Production Issuance Flow

### 1. Receive Activation Request

Customer submits:

```text
activation-request.json
```

Required fields:

- `productId`
- `productVersion`
- `requestType`
- `createdAt`
- `machineHash`
- `machineFingerprintVersion`
- `requestId`

Initial checks:

- `productId` is `duxun-filmsim-ofx-v5`.
- `productVersion` is `5.0`.
- `requestType` is `offline_activation`.
- `machineHash` is a 64-character uppercase SHA256 hex string.
- `createdAt` is plausible for the support case.
- Request file is saved unchanged in the issuance archive.

### 2. Verify Customer And Order

Before signing, operator verifies:

- `customerId` matches the support or commerce record.
- `orderId` is paid, not refunded, not chargebacked, and not already denied.
- Product entitlement includes DuXun Film Simulation v5.0 License MVP.
- Request is for the expected customer and machine.
- Reissue count and replacement history are acceptable.

Suggested decision states:

```text
approved
rejected_refund_or_chargeback
rejected_unknown_order
rejected_suspicious_request
reissued_machine_replacement
manual_review
```

### 3. Sign Buyout License Offline

Use the private key matching the embedded public key for the target release.

Command template:

```bat
python scripts\license_sign_tool.py ^
  --request C:\path\to\activation-request.json ^
  --private-key D:\secure\offline-private-key-v1.pem ^
  --license-type buyout ^
  --customer-id CUSTOMER-ID ^
  --order-id ORDER-ID ^
  -o C:\path\to\license.json
```

For buyout licenses, `expiresAt` is intentionally `null`.

### 4. Verify Signed License

Default production verification uses the embedded public key in `scripts\duxun_license.py`:

```bat
python scripts\verify_license_test.py C:\path\to\license.json
```

When rehearsing with a production-mvp test key, verify with the matching public-key override:

```bat
python scripts\verify_license_test.py ^
  --public-key D:\path\to\production-mvp-public-key-v1.pem ^
  --machine-hash <machineHash from request> ^
  C:\path\to\license.json
```

The expected output is:

```text
ok: license valid
```

### 5. Return License

Send only `license.json` to the customer.

Customer-side install:

```bat
scripts\install_license.bat C:\path\to\license.json
```

Customer then clicks `Reload License` in Resolve or restarts Resolve. The expected status is:

```text
License: buyout activated
```

### 6. Record Issuance Log

The operator ledger must record:

- Issuance timestamp.
- Operator.
- Decision state.
- `keyVersion`.
- Package name and package SHA256.
- `customerId`.
- `orderId`.
- `requestId`.
- `machineHash`.
- `activation-request.json` SHA256.
- `license.json` SHA256.
- Reissue count or replacement reason.
- Delivery method.

Do not record private key contents in the ledger.

## Machine Replacement And Reissue

Reissue is manual in the MVP.

Process:

1. Customer submits a new `activation-request.json`.
2. Operator confirms the original `orderId` and customer identity.
3. Operator checks replacement reason and prior reissue count.
4. Operator marks the previous machine as replaced in the ledger.
5. Operator signs a new buyout license for the new `machineHash`.
6. Operator sends the new `license.json` and records its SHA256.

Suggested MVP policy:

- First legitimate machine replacement: approve.
- Repeated replacements in a short window: manual review.
- Refund, chargeback, reseller dispute, or suspicious mismatch: refuse.

MVP limitation: old licenses are not automatically revoked in the offline runtime. The ledger is the source of truth for support decisions.

## Revocation And Refusal

The MVP does not enforce online revocation.

Manual refusal still matters. The operator should refuse signing when:

- Order is unpaid, refunded, or chargebacked.
- Customer cannot prove ownership of the order.
- Activation request is malformed or for an unsupported product.
- Request appears to be copied from a public forum or unrelated machine.
- Reissue pattern indicates abuse.

Manual revocation record:

```text
revokedAt:
customerId:
orderId:
machineHash:
reason:
operator:
notes:
```

Future online activation or updater builds can consume this ledger, but v5.0 MVP does not call a server.

## Production-MVP Dry Run

Purpose: rehearse the production signing ceremony with a dedicated `production-mvp` test key. This key is not the temporary staging key used in package smoke tests. It also does not replace the embedded public key in the already-built License MVP package.

All dry-run artifacts were written outside the git repository:

```text
D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\
```

Dry-run paths:

```text
operator-custody\production-mvp-private-key-v1.pem
operator-custody\production-mvp-public-key-v1.pem
customer-request\activation-request.json
issued-license\license.json
customer-install\license.json
```

Commands run from the repository root:

```bat
python scripts\license_sign_tool.py ^
  --generate-private-key D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\operator-custody\production-mvp-private-key-v1.pem ^
  --public-key-output D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\operator-custody\production-mvp-public-key-v1.pem
```

```bat
python scripts\generate_activation_request.py ^
  -o D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\customer-request\activation-request.json
```

```bat
python scripts\license_sign_tool.py ^
  --request D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\customer-request\activation-request.json ^
  --private-key D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\operator-custody\production-mvp-private-key-v1.pem ^
  --license-type buyout ^
  --customer-id DX-PROD-MVP-DRYRUN-001 ^
  --order-id ORDER-PROD-MVP-DRYRUN-20260613-001 ^
  -o D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\issued-license\license.json
```

```bat
python scripts\verify_license_test.py ^
  --public-key D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\operator-custody\production-mvp-public-key-v1.pem ^
  --machine-hash 6C5ADA7C9FB0EA00B4FA57E16BE296F8D584146C28EAC8938C8AA65A00A15A88 ^
  D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\issued-license\license.json
```

```bat
python scripts\install_license.py ^
  --public-key D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\operator-custody\production-mvp-public-key-v1.pem ^
  --machine-hash 6C5ADA7C9FB0EA00B4FA57E16BE296F8D584146C28EAC8938C8AA65A00A15A88 ^
  -o D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\customer-install\license.json ^
  D:\Codex\调色插件开发\_private\duxun-filmsim-v5-production-mvp-dry-run-2026-06-13\issued-license\license.json
```

Observed output:

```text
Activation request written: ...\customer-request\activation-request.json
Machine hash: 6C5ADA7C9FB0EA00B4FA57E16BE296F8D584146C28EAC8938C8AA65A00A15A88
Signed license written: ...\issued-license\license.json
ok: license valid
License installed: ...\customer-install\license.json
```

Dry-run hashes:

```text
production-mvp-public-key-v1.pem SHA256:
1B6DEB7656EC31CB347A7F540F2E5D1E13A51B03BFC09A9D465379F7E31FD1FF

activation-request.json SHA256:
33CFF41A7AAED4EEB574F28E6721AA0D173A3AC34C638C306E02A82CC8CD2D07

issued license.json SHA256:
79C22909CF4017FE2F419C4CB61F601789FC9EE183FF7E2CB0B467CEE669667A

installed license.json SHA256:
79C22909CF4017FE2F419C4CB61F601789FC9EE183FF7E2CB0B467CEE669667A
```

Dry-run conclusion:

- Dedicated production-mvp test key was generated outside the repo.
- Buyout signing, verification, and install path worked.
- Installed license hash matched signed license hash.
- Private key stayed outside git and outside package artifacts.
- This dry-run key validates the ceremony only; customer licenses for the shipped License MVP package must be signed by the private key matching the embedded `keyVersion=v1` public key.

## Distribution Boundary

Customer package may include:

- OFX binary.
- Public-key verification code embedded in the OFX and user-side tools.
- `generate_activation_request`.
- `install_license`.
- `verify_license_test`.
- User-facing README and release notes.

Customer package must not include:

- `license_sign_tool.py`.
- Private key PEM files.
- Key-generation commands.
- Operator ledger.
- Custody notes.
- Production issuance scripts that expose private-key paths.

## Security Checks

Required before release or repackage:

```bat
git ls-files | rg -i "\.(pem|key)$|private|secret"
rg -n "BEGIN .*PRIV[A]TE KEY|PRIV[A]TE KEY-----" .
tar -tf DuXunFilmSim-OFX-v5.0-license-mvp.zip | rg -i "license_sign_tool|private|\.pem|\.key|secret"
Get-FileHash -Algorithm SHA256 DuXunFilmSim-OFX-v5.0-license-mvp.zip
python -m unittest discover -s tests -q
```

Expected result:

- No tracked `.pem` or `.key` private-key files.
- No PEM private-key blocks in repository files.
- Package zip does not contain `license_sign_tool.py`, private key files, or operator custody material.
- License MVP package SHA256 remains `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E` unless a deliberate new package is created.
- Unit tests pass.
