from __future__ import annotations

import argparse
from pathlib import Path


ROWS = [
    {
        "group": "Fuji Superia",
        "presets": "Fuji Superia 100, 200, 400, 800, 1600, HG 1600",
        "baseline": "Identity",
        "scene": "Daylight skin, foliage, blue sky, and one shadow area",
        "inspect": "Enhanced color, greens/blues, consumer-negative grain scaling, no waxy skin",
    },
    {
        "group": "Agfa Vista",
        "presets": "Agfa Vista 100, 200, 400",
        "baseline": "Identity",
        "scene": "Daylight skin, neutral wall, mixed color objects, and fine detail",
        "inspect": "Moderate saturation, fine grain, broad exposure latitude, natural skin",
    },
    {
        "group": "CineStill 800T",
        "presets": "CineStill 800T",
        "baseline": "Identity",
        "scene": "Night tungsten/neon practicals with clipped point highlights",
        "inspect": "Cool tungsten balance, visible red-orange halation, controlled global haze",
    },
    {
        "group": "CineStill 50D",
        "presets": "CineStill 50D",
        "baseline": "Identity",
        "scene": "Daylight high-contrast exterior with specular highlights",
        "inspect": "Very fine grain, mild warm halation in highlights, daylight color stability",
    },
]


def render_plan() -> str:
    lines = [
        "# Resolve visual A/B checklist",
        "",
        "Safety boundary: do not automate activation, licensing, crack-related, or account screens.",
        "If Resolve opens to an activation screen, stop and ask the user to open a usable project.",
        "",
        "| Group | Presets | Baseline | Required scene | Inspect |",
        "| --- | --- | --- | --- | --- |",
    ]
    for row in ROWS:
        lines.append(
            f"| {row['group']} | {row['presets']} | {row['baseline']} | "
            f"{row['scene']} | {row['inspect']} |"
        )
    lines.extend(
        [
            "",
            "Capture rule: export an Identity frame and one matching frame per preset.",
            "Name frames as `<scene>__<preset>__YYYYMMDD.png`.",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", type=Path, help="Write the checklist to a markdown file.")
    args = parser.parse_args()
    text = render_plan()
    if args.write:
        args.write.parent.mkdir(parents=True, exist_ok=True)
        args.write.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
