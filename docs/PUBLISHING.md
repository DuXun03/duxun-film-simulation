# Publishing

This repository is prepared for a free public GitHub release under the `DuXun03` account.

## Recommended Repository

- Repository name: `duxun-film-simulation`
- Visibility: public
- Description: `Free DaVinci Resolve OpenFX film simulation plugin with 54 film stocks, Chinese UI, film grain, halation, bloom, print response, gate weave, overscan, and local source builds.`
- Topics: `davinci-resolve`, `openfx`, `film-simulation`, `color-grading`, `cinestill`, `kodak`, `fuji`, `agfa`, `halation`, `film-grain`, `cpp`, `cuda`

## Push Commands

After creating an empty GitHub repository at `https://github.com/DuXun03/duxun-film-simulation`, run:

```bat
git remote add origin https://github.com/DuXun03/duxun-film-simulation.git
git push -u origin main
git push origin v5.0-free
```

If `origin` already exists, update it instead:

```bat
git remote set-url origin https://github.com/DuXun03/duxun-film-simulation.git
git push -u origin main
git push origin v5.0-free
```

## First Release

- Tag: `v5.0-free`
- Title: `DuXun Film Simulation v5.0 Free`

Release notes:

```markdown
This free public release focuses the repository on the OpenFX plugin itself.

Highlights:

- Free DaVinci Resolve OpenFX film simulation plugin
- 54 built-in film presets across Agfa, CineStill, Fuji, Ilford, and Kodak families
- One local OpenFX effect with Chinese UI labels
- Film grain, halation, bloom, print response, film damage, film breath, gate weave, overscan, and vignette controls
- No account gate, online service, or runtime restriction
- Source code, build scripts, install scripts, tests, and third-party notices included
```

## Verification Before Publishing

Run:

```bat
python -m unittest discover -s tests -q
build_plugin.bat
```

The latest local verification passed with 59 unit tests and a successful Windows build.
