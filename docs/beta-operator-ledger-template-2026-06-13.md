# DuXun Film Simulation License MVP Beta Operator Ledger Template

Date: 2026-06-13

## Scope

This is an empty ledger template for beta operations and support drills. It is safe to keep in git because it contains no real customer data, no real request data, no real license hashes, and no private-key material.

Actual operator ledgers must stay outside the repository and outside customer packages. If they are stored under a local `_private` custody area, confirm that `_private` remains outside git and outside any release-package staging folder.

## Rules

- Do not commit filled ledger rows with real customer data.
- Do not commit real `activation-request.json` or `license.json` archives.
- Do not record private key contents, private key paths, or screenshots showing private material.
- Use synthetic values only when documenting a drill in git.
- Keep production signing and custody details in the internal operator location.

## Recommended Decision States

```text
approved
approved_beta_comp
manual_review
reissued_machine_replacement
rejected_malformed_request
rejected_unknown_order
rejected_refund_or_chargeback
rejected_suspicious_request
resend_archived_license
```

## Ledger Row Template

Copy this block into the real offline ledger and fill it there, not in git.

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

## Field Definitions

```text
issuedAt:
  ISO 8601 timestamp for the approval, rejection, resend, or reissue decision.

operator:
  Internal operator id or initials. Do not use personal data beyond the internal operator convention.

decisionState:
  One of the decision states above, or a documented internal extension.

keyVersion:
  Public/private key version used for the target package, such as v1.

packageSha256:
  SHA256 of the package the tester received.

customerId:
  Internal customer or beta tester id. Use synthetic ids in drills committed to git.

orderId:
  Commerce order id, beta comp id, or approved test order id.

requestId:
  Request id from activation-request.json.

machineHash:
  Machine hash from activation-request.json.

activationRequestSha256:
  SHA256 of the exact request file received from the tester.

licenseJsonSha256:
  SHA256 of the exact license.json sent to the tester. Leave blank for refusals.

licenseType:
  buyout, trial_extension, none, or another documented entitlement type. License MVP beta normally uses buyout.

expiresAt:
  Expiration timestamp or null. Buyout beta licenses normally use null.

reissueCount:
  0 for first issue, increment for replacement-machine license issuance. Resending the same archived license should not increment this count.

supportCase:
  Support ticket, inbox thread, or drill case id.

notes:
  Short operational note. Include failure code, replacement reason, or resend reason. Do not include private key material.
```

## Synthetic Drill Example

This row is fake and must not be treated as a real customer record.

```text
issuedAt: 2026-06-13T00:00:00Z
operator: OP-DRILL
decisionState: approved_beta_comp
keyVersion: v1-dry-run
packageSha256: 995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E
customerId: CUST-BETA-DRILL-001
orderId: ORDER-BETA-DRILL-20260613-001
requestId: REQ-BETA-DRILL-001
machineHash: DRILL_ONLY_NOT_A_REAL_MACHINE_HASH
activationRequestSha256: DRILL_ONLY_NOT_A_REAL_REQUEST_HASH
licenseJsonSha256: DRILL_ONLY_NOT_A_REAL_LICENSE_HASH
licenseType: buyout
expiresAt: null
reissueCount: 0
supportCase: CASE-BETA-DRILL-20260613-001
notes: synthetic drill row; dry-run material only; not valid for real customer authorization
```
