# DuXun Film Simulation License MVP Beta Operator Runbook

Date: 2026-06-13

## Scope

This runbook is for manual beta license operations for:

- Package: `DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- Package SHA256: `995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX SHA256: `E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- Product id: `duxun-filmsim-ofx-v5`
- Product version: `5.0`
- Runtime key version: `v1`

Do not rebuild or repackage the License MVP zip during beta operations. If a blocking issue requires a new package, stop issuance and write a new release-readiness record with the reason, new hashes, and operator approval.

Production signing must use the `v1` private key that matches the public key embedded in this package. A dry-run, staging, or rehearsal key cannot activate real customer builds.

## Drill Versus Real Issuance

Support drills may use synthetic tester data and staging or dry-run signing material. Mark all drill files, ledger rows, and support notes with `DRILL_ONLY` or an equivalent label.

Real customer issuance must use:

- A real approved `customerId` and `orderId`.
- The production `v1` private key matching the embedded public key.
- A real offline operator ledger stored outside git.
- A verified `license.json` generated for the customer's actual `machineHash`.

Never mix drill and production materials. Do not send a dry-run or staging license to a real customer, even if local verification succeeds with a public-key override.

## Operator Materials

Keep these materials outside git, outside package folders, and outside customer support attachments:

- Production `v1` private key.
- Offline signing workspace.
- Issuance ledger.
- Customer request archive.
- Issued-license archive.

The repository may contain public verification code and operational documentation, but it must not contain private keys, customer ledgers, `_private` directories, or customer license archives.

## Receive An Activation Request

Customer sends:

```text
activation-request.json
```

Save the file unchanged in the offline request archive before editing, copying, or processing anything else.

Initial request checks:

- File is valid JSON.
- `productId` is `duxun-filmsim-ofx-v5`.
- `productVersion` is `5.0`.
- `requestType` is `offline_activation`.
- `machineHash` is a 64-character uppercase SHA256 hex string.
- `machineFingerprintVersion` is present.
- `requestId` is present and not already used for a completed issuance.
- `createdAt` is plausible for the current support case.
- The request came from the expected customer channel or order email.

If any check fails, do not sign. Mark the case `manual_review` or `rejected_malformed_request` in the ledger.

## Verify Customer And Order

Before signing, confirm:

- `customerId` maps to a beta tester or paid order record.
- `orderId` is paid, comped, or explicitly approved for beta.
- Order is not refunded, chargebacked, disputed, or previously denied.
- The requested product entitlement includes DuXun Film Simulation v5.0 License MVP.
- The request belongs to the expected tester and machine.
- Reissue count and previous machine history are acceptable.

Recommended decision states:

```text
approved
approved_beta_comp
manual_review
reissued_machine_replacement
rejected_malformed_request
rejected_unknown_order
rejected_refund_or_chargeback
rejected_suspicious_request
```

## Sign Offline

Run signing only from a clean release checkout or a dedicated offline operator workspace. Do not run the signer inside a folder that will be zipped for customers.

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

For beta buyout licenses, `expiresAt` should be `null` unless a separate time-limited beta policy has been approved and documented.

## Verify The License

Verify every `license.json` before sending it to the customer.

Production-package verification uses the embedded public key:

```bat
python scripts\verify_license_test.py C:\path\to\license.json
```

Expected output:

```text
ok: license valid
```

If verifying a rehearsal artifact made with a dry-run key, use the matching public-key override and the request machine hash. Do not send that license to a real customer.

```bat
python scripts\verify_license_test.py ^
  --public-key D:\path\to\dry-run-public-key-v1.pem ^
  --machine-hash MACHINE-HASH-FROM-REQUEST ^
  C:\path\to\license.json
```

If verification fails, do not edit the license manually. Delete the failed output from the send queue, record the failure, and re-run signing only after the request, key, and order data are checked.

## Return The License

Send only:

```text
license.json
```

Do not send:

- Private key files.
- Public-key override files.
- Signing commands with private paths.
- Issuance ledger rows.
- Internal notes about request scoring.
- Any `_private` archive material.
- Dry-run or staging licenses for real customer authorization.
- Screenshots that reveal private-key storage, operator ledgers, or signing workspaces.

Customer instruction summary:

```bat
scripts\install_license.bat C:\path\to\license.json
```

Then the customer should click `Reload License` in Resolve or restart Resolve. The expected status is:

```text
License: buyout activated
```

## Record Issuance

Record every approval, refusal, and reissue in the offline operator ledger.

Minimum ledger fields:

```text
issuedAt:
operator:
decisionState:
keyVersion:
packageName:
packageSha256:
ofxSha256:
customerId:
orderId:
requestId:
machineHash:
activationRequestSha256:
licenseJsonSha256:
licenseType:
expiresAt:
reissueCount:
replacementReason:
deliveryMethod:
supportCase:
notes:
```

Never record private key contents in the ledger. The ledger itself is internal operator material and must stay outside git and outside release packages.

## Failure Case Records

Record failures even when no license is issued. A refusal or manual-review entry should include:

```text
issuedAt:
operator:
decisionState:
keyVersion:
packageSha256:
customerId:
orderId:
requestId:
machineHash:
activationRequestSha256:
licenseJsonSha256:
licenseType:
expiresAt:
reissueCount:
supportCase:
notes:
```

Use `licenseJsonSha256: none` and `licenseType: none` when no license is sent. In `notes`, record the user-visible failure code or support reason, such as malformed request, machine mismatch, invalid signature, stale Resolve state, refund, chargeback, or suspicious reissue pattern.

Failure records must not include private-key paths, private-key contents, customer payment details, or full internal ledger exports.

## Machine Replacement

For a normal machine replacement:

1. Ask the customer to generate a new `activation-request.json` from the new machine.
2. Confirm the original `customerId`, `orderId`, and support identity.
3. Check the previous machine hash and reissue count.
4. Mark the previous machine as replaced in the ledger.
5. Sign a new buyout license for the new `machineHash`.
6. Verify the new license.
7. Send only the new `license.json`.
8. Record the new license hash and replacement reason.

Suggested MVP policy:

- First legitimate replacement: approve.
- Second replacement with a clear reason: manual review.
- Repeated replacements in a short period: hold until owner approval.
- Refund, chargeback, dispute, or suspicious mismatch: refuse.

MVP limitation: old offline licenses are not automatically revoked by v5.0. The ledger is the support source of truth.

## Resend

A resend is allowed when the customer lost the same `license.json` for the same machine and order.

Before resending:

- Confirm `customerId` and `orderId`.
- Confirm the target `machineHash` is unchanged.
- Recompute or compare the archived `license.json` SHA256.
- Record the resend timestamp and delivery method.

Do not generate a new license if the original archived license is available and the machine hash has not changed.

## Refuse Or Escalate

Do not sign when:

- Request JSON is malformed or unsupported.
- Product id or version does not match this package.
- Order is unpaid, refunded, chargebacked, or disputed.
- Customer cannot prove ownership of the order or beta invitation.
- Request appears copied from another customer or public channel.
- Reissue pattern suggests abuse.
- The correct production `v1` private key is unavailable.

Escalate to owner review when the evidence is unclear. Record the refusal or escalation in the ledger with no private-key details.

## Closing Checklist

Before closing a beta license case:

- `activation-request.json` archived unchanged.
- Order/customer check recorded.
- License signed with the production `v1` key that matches the embedded public key.
- `license.json` verified as valid.
- Customer received only `license.json`.
- Ledger row includes order, machine hash, request time, signing time, package hash, request hash, and license hash.
- No private key, signing workspace, `_private` path, or ledger material was copied into git or the customer package.
