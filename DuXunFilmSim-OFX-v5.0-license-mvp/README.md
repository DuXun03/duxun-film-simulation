# DuXun Film Simulation OFX v5.0 License MVP Beta

This package is the licensing MVP beta for DuXun Film Simulation v5.0. It is separate from the previously accepted no-licensing beta package `DuXunFilmSim-OFX-v5.0.zip`.

## Install

Close DaVinci Resolve, then run:

```bat
scripts\install.bat
```

The installer copies `build\DuXunFilmSim.ofx` to:

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

## Licensing Flow

Generate an offline activation request on the customer machine:

```bat
scripts\generate_activation_request.bat
```

Send `%PROGRAMDATA%\DuXun\FilmSim\activation-request.json` to the license operator. The operator signs a license outside this package. After receiving `license.json`, install it with:

```bat
scripts\install_license.bat C:\path\to\license.json
```

Verify the installed or supplied license:

```bat
scripts\verify_license_test.py
scripts\verify_license_test.py C:\path\to\license.json
```

## Trial And Buyout

Without a valid license, the plugin creates a local 24 hour trial state and renders with a lightweight watermark. A valid buyout `license.json` changes the Resolve License status to `License: buyout activated` and disables the watermark.

## Key Handling

The package contains only public-key verification code. It does not contain a private signing key. Staging keys are temporary QA keys used with `--public-key` and `--machine-hash` overrides for toolchain tests. The embedded MVP/production public key is what the plugin and default tools use for real signed licenses.

## Contents

- `build\DuXunFilmSim.ofx`
- `scripts\install.bat`
- `scripts\uninstall.bat`
- `scripts\generate_activation_request.bat/.py`
- `scripts\install_license.bat/.py`
- `scripts\verify_license_test.py`
- `scripts\duxun_license.py`
- licensing design and QA handoff documents
- `checksums\SHA256SUMS.txt`
