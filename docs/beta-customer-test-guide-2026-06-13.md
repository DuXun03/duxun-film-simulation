# DuXun Film Simulation License MVP Beta Customer Test Guide

Date: 2026-06-13

Thank you for testing the DuXun Film Simulation License MVP beta. This guide covers the install and activation flow for ordinary beta testers.

## What You Received

You should have:

```text
DuXunFilmSim-OFX-v5.0-license-mvp.zip
```

After extracting it, the folder should include:

```text
README.md
QUICKSTART.md
build\DuXunFilmSim.ofx
scripts\install.bat
scripts\uninstall.bat
scripts\generate_activation_request.bat
scripts\install_license.bat
```

Do not rename files inside the extracted folder before installing.

## Install The OFX Plug-In

1. Close DaVinci Resolve.
2. Extract the zip to a normal local folder, such as Desktop or Downloads.
3. Open the extracted folder.
4. Run:

```bat
scripts\install.bat
```

On Windows, the installer may need Administrator permission because OFX plug-ins are installed under:

```text
C:\Program Files\Common Files\OFX\Plugins
```

The install script should print matching build and installed SHA256 hashes. If it reports a mismatch or permission error, send us a screenshot of the full window.

## Open Resolve And Confirm Trial

1. Start DaVinci Resolve.
2. Open a project and timeline.
3. Go to the Effects Library.
4. Find `DuXun -> DuXun Film Simulation`.
5. Add the effect to a clip.
6. Open the effect controls and expand the `License` section.

Before activation, the plug-in should show a trial status similar to:

```text
License: 24h trial active, watermark enabled
```

You should also see a visible watermark in the viewer. This is expected before the beta license is installed.

## Generate Your Activation Request

In the plug-in controls, click:

```text
Generate Activation Request
```

The plug-in writes:

```text
C:\ProgramData\DuXun\FilmSim\activation-request.json
```

Send this `activation-request.json` file to us through the support channel we gave you. Please do not edit the file.

## Install Your license.json

We will send back:

```text
license.json
```

Save it to a local folder, then run this from the extracted beta package folder:

```bat
scripts\install_license.bat C:\path\to\license.json
```

Replace `C:\path\to\license.json` with the actual location of the file you saved.

If the script succeeds, it installs the license to:

```text
C:\ProgramData\DuXun\FilmSim\license.json
```

## Reload License In Resolve

Return to DaVinci Resolve.

In the `License` section, click:

```text
Reload License
```

If Resolve does not update, restart Resolve and open the project again.

Expected activated status:

```text
License: buyout activated
```

The watermark should disappear after activation.

## What To Test After Activation

Please try:

- Fuji Superia 100, 400, and 800 on daylight and mixed-light clips.
- Agfa Vista 100, 200, and 400 on skin tones and exterior shots.
- CineStill 800T on night, neon, practical lights, or tungsten scenes.
- A few black-and-white presets if your project has suitable footage.
- Playback responsiveness while adjusting strength, grain, halation, and glow.

Please compare the plug-in enabled and bypassed so your feedback is specific.

## Common Problems

### The plug-in does not appear in Resolve

- Make sure Resolve was closed during install.
- Run `scripts\install.bat` again as Administrator.
- Restart Resolve.
- If it still does not appear, send your Resolve version and the install script output.

### The activation request file is missing

- Confirm the effect is added to a clip.
- Click `Generate Activation Request` again.
- Check:

```text
C:\ProgramData\DuXun\FilmSim
```

- If the folder is missing, send us a screenshot of the `License` section.

### install_license.bat says the license is invalid

- Make sure you are installing the exact `license.json` we sent back.
- Do not rename it to `license.json.txt`.
- Do not open and resave it in a text editor.
- If you generated a new activation request after we sent the license, send us the new request.

### Resolve still shows trial after installing the license

- Click `Reload License`.
- Restart Resolve.
- Confirm this file exists:

```text
C:\ProgramData\DuXun\FilmSim\license.json
```

- If it still shows trial, send us:

```text
C:\ProgramData\DuXun\FilmSim\logs\license.log
```

### Watermark is still visible

- Confirm the status says `License: buyout activated`.
- Toggle the effect off and on.
- Restart Resolve.
- Send a screenshot of the viewer and the `License` section if it remains.

## What To Send Back

Please send the completed beta feedback template, plus any screenshots or short screen recordings that make the issue easier to understand.
