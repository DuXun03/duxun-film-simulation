# DuXun Film Simulation License MVP Beta Support Drill Result

Date: 2026-06-13

## Scope

This document records the public, git-safe summary of one Beta support drill rehearsal for the DuXun Film Simulation v5.0 License MVP.

Private drill artifacts were kept outside git. This summary intentionally does not include private keys, `_private` paths, real customer data, real ledger data, `activation-request.json`, or `license.json` contents.

All dry-run/staging materials used in this drill are marked as not valid for real customer authorization.

## Drill Identity

- Drill run: `run-2026-06-13-001`
- Synthetic tester id: `CUST-BETA-DRILL-001`
- Synthetic order id: `ORDER-BETA-DRILL-20260613-001`
- Synthetic support case: `CASE-BETA-DRILL-20260613-001`
- Archive code: `duxun-filmsim-v5-beta-drill-2026-06-13/run-2026-06-13-001`
- Authorization note: `DRILL_ONLY_NOT_FOR_REAL_CUSTOMER_AUTHORIZATION`

## Release Anchors

- Package: `DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- Package SHA256: `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- Expected OFX SHA256: `E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- Product id: `duxun-filmsim-ofx-v5`
- Product version: `5.0`

The release zip was not modified or repackaged during the drill.

## Baseline Results

- `git status --short --branch`: clean `main` at drill start.
- `python -m unittest discover -s tests -q`: `Ran 68 tests`, `OK`.
- `Get-FileHash -Algorithm SHA256 DuXunFilmSim-OFX-v5.0-license-mvp.zip`: matched the expected package hash.
- `tar -tf DuXunFilmSim-OFX-v5.0-license-mvp.zip | rg -i "license_sign_tool|private|\.pem|\.key|secret|ledger"`: no matches.
- Repository private-key block scan: no matches.

## Normal First Activation

Result: PASS.

Evidence summary:

- Synthetic activation request was generated and archived outside git.
- Dry-run signing material was used only for rehearsal.
- Rehearsal buyout `license.json` was signed outside git.
- License verification returned `ok: license valid`.
- License install was simulated to an external archive path.
- Installed-license SHA256 matched the signed-license SHA256.
- Dry-run private key was not retained in the final drill archive.

This path confirms the operator handoff can support:

```text
activation request received -> request validated -> license signed -> license verified -> license installed -> buyout activated
```

## Trial-Stuck Triage

Result: PASS.

Scenario rehearsed:

```text
install_license reports success, but Resolve still shows trial or watermark.
```

Evidence summary:

- Archived license verified against the expected synthetic machine hash.
- Wrong-machine verification was rehearsed with a synthetic mismatch.
- Support note records the triage path: click `Reload License`, restart Resolve, confirm ProgramData `license.json`, request License section screenshot, request command output, and inspect `license.log` if present.
- Manual-review recording path was rehearsed for unresolved or mismatch cases.

Conclusion: the customer guide and operator runbook now give enough evidence requests to distinguish stale Resolve state from machine/license mismatch.

## Resend And Machine Replacement

Result: PASS.

Resend decision:

- Same synthetic customer, same order, same machine hash.
- Decision rehearsed as resend archived license only.
- No new license signing required.
- Reissue count remains `0`.

Machine replacement decision:

- New synthetic replacement request was created outside git.
- Replacement machine hash was distinct from the original synthetic request.
- First replacement decision was rehearsed as approved.
- Replacement license was signed and verified with dry-run material.
- Reissue count moves to `1`.

Suspicious replacement handling:

- Decision path remains `manual_review` or `rejected_suspicious_request`.
- No signing until owner review.
- No internal ledger rows or private signing details are sent to the tester.

## Archive Contents

External archive code:

```text
duxun-filmsim-v5-beta-drill-2026-06-13/run-2026-06-13-001
```

Archive category summary:

- Synthetic activation request.
- Rehearsal license files.
- Verification output.
- Synthetic feedback.
- Drill ledger copy.
- Support notes.
- Dry-run public verification material.

The archive is outside git. This repository does not include private keys, actual request files, actual license files, real customer information, or real ledger rows from the drill.

## Pass/Fail Conclusion

PASS.

The support drill covered the required three paths:

- Normal first activation.
- License installed but Resolve still shows trial.
- Resend and machine replacement decision flow.

No support-drill blocker remains before the first controlled real-user beta send.

## Remaining Guardrails

These are release guardrails, not blockers for the first controlled beta send:

- Use only the production `v1` private key that matches the embedded public key for real customer authorization.
- Never use the dry-run/staging key for real customer licenses.
- Keep real customer data, real ledgers, private keys, signing workspaces, `activation-request.json`, and `license.json` out of git.
- Keep the License MVP zip frozen unless a blocking issue forces a new documented readiness record.
- This is still a controlled beta send, not a formal public release.
