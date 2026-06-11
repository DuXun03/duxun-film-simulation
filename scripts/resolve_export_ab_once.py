from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path


SCRIPTING_EXAMPLES = Path(
    r"C:\ProgramData\Blackmagic Design\DaVinci Resolve\Support\Developer\Scripting\Examples"
)
sys.path.insert(0, str(SCRIPTING_EXAMPLES))

from python_get_resolve import GetResolve  # type: ignore  # noqa: E402


FPS_NOMINAL = 30


def frame_to_tc(frame: int) -> str:
    hh = frame // (FPS_NOMINAL * 3600)
    frame %= FPS_NOMINAL * 3600
    mm = frame // (FPS_NOMINAL * 60)
    frame %= FPS_NOMINAL * 60
    ss = frame // FPS_NOMINAL
    ff = frame % FPS_NOMINAL
    return f"{hh:02d}:{mm:02d}:{ss:02d}:{ff:02d}"


def item_for_frame(timeline, frame: int):
    for track_index in range(1, timeline.GetTrackCount("video") + 1):
        for item in timeline.GetItemListInTrack("video", track_index):
            if int(item.GetStart()) <= frame < int(item.GetEnd()):
                return item
    return None


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--scene", required=True)
    parser.add_argument("--preset", required=True)
    parser.add_argument("--date", required=True)
    parser.add_argument("--source-frame", type=int, required=True)
    parser.add_argument("--target-frame", type=int, required=True)
    parser.add_argument("--node-index", type=int, default=2)
    args = parser.parse_args()

    resolve = GetResolve()
    project = resolve.GetProjectManager().GetCurrentProject()
    if not project:
        raise RuntimeError("No Resolve project is open")
    timeline = project.GetCurrentTimeline()
    if not timeline:
        raise RuntimeError("No current timeline is open")

    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    source = item_for_frame(timeline, args.source_frame)
    target = item_for_frame(timeline, args.target_frame)
    if source is None or target is None:
        raise RuntimeError(
            f"Could not find source/target items: source={source is not None}, target={target is not None}"
        )

    if target.GetUniqueId() != source.GetUniqueId() and not source.CopyGrades([target]):
        raise RuntimeError("CopyGrades failed")

    target_tc = frame_to_tc(args.target_frame)
    if not timeline.SetCurrentTimecode(target_tc):
        raise RuntimeError(f"SetCurrentTimecode failed: {target_tc}")
    time.sleep(0.6)

    current = timeline.GetCurrentVideoItem()
    graph = current.GetNodeGraph()
    if graph.GetNumNodes() < args.node_index:
        raise RuntimeError(f"Expected node {args.node_index}, got {graph.GetNumNodes()}")

    identity = out_dir / f"{args.scene}__identity__{args.date}.png"
    effect = out_dir / f"{args.scene}__{args.preset}__{args.date}.png"

    if not graph.SetNodeEnabled(args.node_index, False):
        raise RuntimeError("Disabling target node failed")
    time.sleep(0.5)
    identity_ok = project.ExportCurrentFrameAsStill(str(identity))

    if not graph.SetNodeEnabled(args.node_index, True):
        raise RuntimeError("Enabling target node failed")
    time.sleep(0.7)
    effect_ok = project.ExportCurrentFrameAsStill(str(effect))

    source_tc = frame_to_tc(args.source_frame)
    timeline.SetCurrentTimecode(source_tc)

    result = {
        "scene": args.scene,
        "preset": args.preset,
        "sourceTimecode": source_tc,
        "targetTimecode": target_tc,
        "identityOk": bool(identity_ok),
        "effectOk": bool(effect_ok),
        "identity": str(identity),
        "effect": str(effect),
        "identityExists": identity.exists(),
        "effectExists": effect.exists(),
    }
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if identity_ok and effect_ok and identity.exists() and effect.exists() else 1


if __name__ == "__main__":
    raise SystemExit(main())
