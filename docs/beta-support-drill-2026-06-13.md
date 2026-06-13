# DuXun Film Simulation License MVP Beta Support Drill

Date: 2026-06-13

## Scope

This drill rehearses the full beta support loop before sending the License MVP package to real testers:

```text
user install -> activation request -> operator signing -> activation -> feedback -> archive
```

This is a rehearsal only. Use synthetic tester data and staging or dry-run signing materials. Dry-run licenses must be clearly labeled and must not be sent to real customers. Real customer authorization requires the production `v1` private key matching the public key embedded in `DuXunFilmSim-OFX-v5.0-license-mvp.zip`.

Do not modify visual algorithms, License MVP runtime code, frozen parameters, or the release zip during this drill.

## Release Anchors

- Package: `DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- Package SHA256: `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX SHA256: `E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- Customer guide: `docs/beta-customer-test-guide-2026-06-13.md`
- Operator runbook: `docs/beta-operator-runbook-2026-06-13.md`
- Feedback template: `docs/beta-feedback-template-2026-06-13.md`
- Ledger template: `docs/beta-operator-ledger-template-2026-06-13.md`

## Synthetic Drill Identity

Use this fake tester record for the rehearsal:

```text
testerName: Beta Drill Tester
customerId: CUST-BETA-DRILL-001
orderId: ORDER-BETA-DRILL-20260613-001
supportCase: CASE-BETA-DRILL-20260613-001
contactChannel: internal drill inbox
machineLabel: DRILL-WIN-PRIMARY
```

Do not replace this with real customer information in git. Actual operator ledgers must stay outside the repository.

## Expected State Flow

```text
package_sent
installed
trial_active_watermark_visible
activation_request_received
request_validated
license_signed_dry_run_or_staging
license_verified
license_sent
license_installed
buyout_activated_watermark_removed
feedback_received
case_archived
```

Every drill path should end with either `case_archived` or a documented failure state plus next action.

## Preflight

Run from the repository root:

```bat
git status --short --branch
python -m unittest discover -s tests -q
powershell -NoProfile -Command "Get-FileHash -Algorithm SHA256 DuXunFilmSim-OFX-v5.0-license-mvp.zip"
```

Audit the package contents before the drill:

```powershell
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead((Resolve-Path 'DuXunFilmSim-OFX-v5.0-license-mvp.zip'))
try {
  $zip.Entries |
    Where-Object { $_.FullName -match '(?i)(license_sign_tool|signing|private|_private|\.pem$|\.key$|secret|ledger)' } |
    Select-Object -ExpandProperty FullName
} finally {
  $zip.Dispose()
}
```

Expected package-audit result: no output.

## Drill Path 1: Normal First Activation

Purpose: verify the happy path is clear for both tester and operator.

Tester steps:

1. Extract `DuXunFilmSim-OFX-v5.0-license-mvp.zip`.
2. Run `scripts\install.bat`.
3. Open Resolve and add `DuXun -> DuXun Film Simulation` to a clip.
4. Confirm trial status and visible watermark.
5. Click `Generate Activation Request`.
6. Send `C:\ProgramData\DuXun\FilmSim\activation-request.json` to the operator.

Operator steps:

1. Save the request unchanged in the drill archive outside git.
2. Confirm `productId=duxun-filmsim-ofx-v5`, `productVersion=5.0`, and `requestType=offline_activation`.
3. Record the request SHA256 in the drill ledger copy.
4. Sign a rehearsal license with staging or dry-run material, or perform a no-send production-key ceremony if owner-approved.
5. Verify the generated `license.json`.
6. Record the license SHA256.
7. Send only `license.json` back to the tester.

Tester activation steps:

1. Run `scripts\install_license.bat C:\path\to\license.json`.
2. Click `Reload License` in Resolve.
3. Confirm status is `License: buyout activated`.
4. Confirm the watermark is gone.
5. Complete the feedback template.

Expected final state:

```text
decisionState: approved_beta_comp
licenseType: buyout
expiresAt: null
reissueCount: 0
caseStatus: case_archived
```

## Drill Path 2: License Installed But Resolve Still Shows Trial

Purpose: rehearse support triage when customer-side install appears successful but Resolve does not activate.

Initial condition:

```text
scripts\install_license.bat reports success, but Resolve still shows trial or watermark.
```

Tester checks:

1. Click `Reload License`.
2. Restart Resolve.
3. Confirm `C:\ProgramData\DuXun\FilmSim\license.json` exists.
4. Capture a screenshot of the Resolve `License` section.
5. Capture a screenshot of the install-license command output.
6. Send `C:\ProgramData\DuXun\FilmSim\logs\license.log` if present.

Operator triage:

1. Confirm the customer installed the newest `license.json`, not the original `activation-request.json`.
2. Confirm the request id and machine hash in the ledger match the generated license.
3. Ask whether a new activation request was generated after signing.
4. Verify the archived `license.json` against the expected machine hash.
5. If the machine hash changed, move to the machine replacement path.
6. If the license verifies but Resolve is stale, ask the tester to restart Resolve and re-check.
7. If `license.log` reports signature, machine, or expiry failure, record the exact failure code.

Record a failure ledger row or case note with:

```text
decisionState: manual_review
supportCase: CASE-BETA-DRILL-20260613-TRIAL-STUCK
notes: install succeeded but Resolve stayed trial; include log status and next action
```

Expected final states:

- `buyout_activated_watermark_removed` if reload or restart fixes it.
- `manual_review` if logs show mismatch or an unclear runtime failure.
- `reissued_machine_replacement` if the tester generated a valid request on a different machine.

## Drill Path 3: Machine Replacement Or Resend

Purpose: rehearse the operator decision between resending an existing license and signing a replacement license.

Scenario A: resend same license.

Tester report:

```text
I lost license.json, same computer, same Resolve install.
```

Operator decision:

1. Confirm customer identity and `orderId`.
2. Confirm the support case maps to the same `machineHash`.
3. Locate the archived `license.json` outside git.
4. Recompute its SHA256.
5. Resend only that same `license.json`.
6. Record resend timestamp and delivery method.

Expected ledger note:

```text
decisionState: approved
reissueCount: 0
notes: resend archived license for unchanged machine hash
```

Scenario B: replacement machine.

Tester report:

```text
I changed computers or reinstalled Windows and need activation again.
```

Operator decision:

1. Ask for a new `activation-request.json` from the new machine.
2. Confirm customer identity and original `orderId`.
3. Compare new `machineHash` with the previous ledger row.
4. Check replacement reason and prior replacement count.
5. If acceptable, sign a new license for the new machine hash.
6. Verify the new license.
7. Send only the new `license.json`.
8. Mark the previous machine as replaced in ledger notes.

Expected ledger note:

```text
decisionState: reissued_machine_replacement
reissueCount: 1
notes: approved first replacement; previous machine marked replaced
```

Scenario C: suspicious replacement.

Operator decision:

1. Do not sign immediately.
2. Record `manual_review` or `rejected_suspicious_request`.
3. Escalate to owner review with order history, request id, machine hashes, and support notes.
4. Do not send internal ledger rows or private signing details to the tester.

## Archive Checklist

For every drill case, archive outside git:

- Original `activation-request.json`.
- Request SHA256.
- Generated `license.json` if one was created.
- License SHA256.
- Verification output.
- Support screenshots or log excerpts.
- Completed feedback template.
- Ledger row or drill ledger copy.

Do not archive private keys, raw private-key terminal output, or real customer data in this repository.

## Pass Criteria

The drill is ready to mark complete when:

- Tester instructions are clear enough to produce `activation-request.json`.
- Operator can validate request fields and order identity using the runbook.
- Operator can sign or simulate signing without exposing private material.
- License verification is recorded before sending.
- Tester can install `license.json` and confirm buyout status.
- Trial-stuck troubleshooting has a clear support path.
- Replacement versus resend decision is documented.
- Feedback template is completed and archived.
- No private key, real customer data, real ledger, or `_private` material enters git.
