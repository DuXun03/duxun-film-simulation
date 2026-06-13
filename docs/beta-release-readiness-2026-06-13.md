# DuXun Film Simulation License MVP Beta Release Readiness

Date: 2026-06-13

## Release Candidate

- Package: `DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- Package SHA256: `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX SHA256: `E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- Product id: `duxun-filmsim-ofx-v5`
- Product version: `5.0`
- Runtime key version: `v1`
- Status: beta handoff ready, not a formal public release.

Do not modify or repackage this zip unless a blocking issue is found and recorded with a new readiness document and new hashes.

## Beta Handoff Documents

- Operator runbook: `docs/beta-operator-runbook-2026-06-13.md`
- Customer test guide: `docs/beta-customer-test-guide-2026-06-13.md`
- Feedback template: `docs/beta-feedback-template-2026-06-13.md`
- License custody reference: `docs/license-key-custody-and-issuance-2026-06-13.md`
- Licensing QA record: `docs/licensing-mvp-qa-2026-06-12.md`

## QA Conclusion

Current verified baseline:

- Unit tests pass with `python -m unittest discover -s tests -q`.
- Package SHA256 matches the recorded release candidate hash.
- License MVP package audit found no `license_sign_tool`, `private`, `.pem`, `.key`, or `secret` entries.
- Tracked source scan found no private-key blocks.
- Resolve smoke from the License MVP QA showed trial, activation request, license install, `Reload License`, buyout status, and watermark removal.
- Visual baseline remains frozen for Fuji Superia, Agfa Vista, and CineStill 800T.

The package is suitable for controlled beta testers who can follow manual offline activation instructions and provide structured feedback.

## Allowed Beta Scope

Allowed:

- Send the frozen License MVP zip to selected beta testers.
- Ask testers to install the OFX package, generate `activation-request.json`, and return it.
- Manually issue buyout `license.json` files after order or beta approval.
- Collect feedback with the beta template.
- Compare basic picture quality across presets, footage types, and GPUs.
- Record activation, reissue, resend, and refusal decisions in the offline operator ledger.

## Known Limitations

- Offline activation only. There is no license server.
- No automatic revocation in v5.0.
- Machine replacement is manual.
- Watermark is an MVP misuse reminder, not strong DRM.
- Resolve UI smoke is manual, not an automated UI regression suite.
- ProgramData permissions rely on the current Windows environment and installer behavior.
- No GUI installer.
- No HDR, OpenCL, Metal, new GPU expansion, or LUT runtime loading in this package.
- CineStill 50D remains first-round daylight exterior pass only.

## Do Not Do

- Do not change visual algorithms, frozen parameters, or the licensing runtime for this beta handoff.
- Do not rebuild, overwrite, or repackage `DuXunFilmSim-OFX-v5.0-license-mvp.zip`.
- Do not put private keys, signing workspaces, operator ledgers, `_private` paths, or customer archives into git.
- Do not include `license_sign_tool.py`, private keys, `.pem`, `.key`, or internal operator materials in customer packages.
- Do not use a dry-run or staging key for real customer licenses.
- Do not describe this as a formal release.

Production signing must use the `v1` private key that matches the public key embedded in this package. A dry-run key cannot be used for real customer authorization.

## Next Beta Entry

For each beta tester:

1. Send the frozen zip and `docs/beta-customer-test-guide-2026-06-13.md`.
2. Receive `activation-request.json`.
3. Follow `docs/beta-operator-runbook-2026-06-13.md`.
4. Send only `license.json`.
5. Ask the tester to complete `docs/beta-feedback-template-2026-06-13.md`.
6. Record outcome, machine hash, signing time, and feedback status in the offline ledger.

For the next readiness update, create a new dated document rather than editing package hashes in place.
