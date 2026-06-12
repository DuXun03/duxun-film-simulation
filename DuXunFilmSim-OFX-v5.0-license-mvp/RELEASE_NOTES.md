# Release Notes - DuXun Film Simulation OFX v5.0 License MVP Beta

This is a license MVP beta package. It is not the original accepted no-licensing beta package `DuXunFilmSim-OFX-v5.0.zip`, and that original archive is intentionally left untouched.

## Added

- Local signed-license runtime for DuXun Film Simulation v5.0.
- 24 hour local trial state with watermark output.
- Offline activation request generation.
- Buyout license support with ECDSA P-256 signature verification.
- Resolve License parameter group with status, Generate Activation Request, and Reload License controls.
- CLI tools for activation request generation, license installation, and license verification.

## Licensing Workflow

1. Install the OFX beta with `scripts\install.bat`.
2. In Resolve, use the License group or run `scripts\generate_activation_request.bat` to create `activation-request.json`.
3. Send the request to the license operator.
4. The operator signs a `license.json` outside this package.
5. Install the returned license with `scripts\install_license.bat C:\path\to\license.json`.
6. Click Reload License in Resolve or restart Resolve.

## Private Key Policy

No private key is included in this package. The signing private key must stay in operator-controlled storage and must not be distributed, committed to git, or copied into customer packages.

## Staging Versus Production/MVP Keys

Staging keys are temporary QA keypairs generated for smoke tests. The verification and install tools support `--public-key` and `--machine-hash` overrides so staging licenses can be tested without replacing the embedded MVP public key. Normal package use relies on the embedded public key and licenses signed by the matching private key.

## Boundaries

- No visual default or matrix changes are intended in this package.
- HDR, OpenCL, Metal, new GPU expansion, and LUT runtime loading remain out of scope.
- This beta does not include online activation, an activation server, or a full GUI license manager.
