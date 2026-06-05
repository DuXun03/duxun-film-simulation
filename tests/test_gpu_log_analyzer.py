from pathlib import Path
import contextlib
import importlib.util
import io
import json
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "scripts" / "analyze_gpu_log.py"


def load_analyzer():
    spec = importlib.util.spec_from_file_location("analyze_gpu_log", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class GpuLogAnalyzerTests(unittest.TestCase):
    def test_summarizes_timed_gpu_log_by_size_effects_and_fallbacks(self):
        analyzer = load_analyzer()
        lines = [
            "phase=request reason=received width=3840 height=2160 pixels=8294400 sizeClass=full_4k elapsedMs=-1.000 requested=1 available=1 cuda=1 opencl=0 doHalation=1 doBloom=1 doDamage=0 doSpatial=1 requestIndex=1 successCount=0 fallbackCount=0",
            "phase=cuda_success reason=ok width=3840 height=2160 pixels=8294400 sizeClass=full_4k elapsedMs=12.000 setupMs=9.000 kernelMs=3.000 coldStart=1 requested=1 available=1 cuda=1 opencl=0 doHalation=1 doBloom=1 doDamage=0 doSpatial=1 requestIndex=1 successCount=1 fallbackCount=0",
            "phase=cuda_success reason=ok width=432 height=243 pixels=104976 sizeClass=preview elapsedMs=1.500 setupMs=0.250 kernelMs=1.250 coldStart=0 requested=1 available=1 cuda=1 opencl=0 doHalation=0 doBloom=0 doDamage=0 doSpatial=0 requestIndex=2 successCount=2 fallbackCount=0",
            "phase=cuda_fallback reason=launch_failed width=138 height=69 pixels=9522 sizeClass=thumbnail elapsedMs=0.750 setupMs=0.500 kernelMs=0.250 coldStart=0 requested=1 available=1 cuda=1 opencl=0 doHalation=1 doBloom=0 doDamage=1 doSpatial=0 requestIndex=3 successCount=2 fallbackCount=1",
            "phase=cuda_success reason=ok width=3840 height=2160 requested=1 available=1 cuda=1 opencl=0 doHalation=0 doBloom=0 doDamage=0 doSpatial=0",
        ]

        attempts = analyzer.parse_log_lines(lines)
        summary = analyzer.summarize_attempts(attempts)
        report = analyzer.format_summary(summary)

        self.assertEqual(len(attempts), 5)
        self.assertEqual(summary["total"]["success"], 3)
        self.assertEqual(summary["total"]["fallback"], 1)
        self.assertEqual(summary["total"]["fallbackRate"], 0.25)
        self.assertEqual(summary["fallbackReasons"], {"launch_failed": 1})
        self.assertEqual(summary["bySize"]["full_4k"]["success"]["count"], 2)
        self.assertEqual(summary["bySize"]["preview"]["success"]["avgMs"], 1.5)
        self.assertEqual(summary["bySize"]["full_4k"]["success"]["avgSetupMs"], 9.0)
        self.assertEqual(summary["bySize"]["full_4k"]["success"]["avgKernelMs"], 3.0)
        self.assertEqual(summary["bySize"]["full_4k"]["success"]["coldStartCount"], 1)
        self.assertEqual(summary["byEffects"]["halation+bloom+spatial"]["success"]["maxMs"], 12.0)
        self.assertIn("GPU Render Summary", report)
        self.assertIn("full_4k", report)
        self.assertIn("fallbackRate=25.0%", report)
        self.assertIn("setupAvgMs=9.000", report)
        self.assertIn("kernelAvgMs=3.000", report)
        self.assertIn("coldStart=1", report)
        self.assertIn("launch_failed", report)

    def test_cli_can_analyze_only_lines_after_row_count_snapshot(self):
        analyzer = load_analyzer()
        lines = [
            "phase=cuda_success reason=ok width=3840 height=2160 elapsedMs=10.000 setupMs=8.000 kernelMs=2.000 coldStart=1 requested=1 available=1 cuda=1 opencl=0 doHalation=1 doBloom=1 doDamage=0 doSpatial=1",
            "phase=cuda_success reason=ok width=432 height=243 elapsedMs=1.000 setupMs=0.100 kernelMs=0.900 coldStart=0 requested=1 available=1 cuda=1 opencl=0 doHalation=0 doBloom=0 doDamage=0 doSpatial=0",
            "phase=cuda_success reason=ok width=3840 height=2160 elapsedMs=4.000 setupMs=0.250 kernelMs=3.750 coldStart=0 requested=1 available=1 cuda=1 opencl=0 doHalation=1 doBloom=1 doDamage=1 doSpatial=1",
            "phase=cuda_fallback reason=runtime_not_loaded width=3840 height=2160 elapsedMs=0.500 setupMs=0.500 kernelMs=-1.000 coldStart=0 requested=1 available=1 cuda=1 opencl=0 doHalation=0 doBloom=0 doDamage=0 doSpatial=0",
        ]
        with tempfile.TemporaryDirectory() as temp_dir:
            log_path = Path(temp_dir) / "DuXunFilmSim-gpu.log"
            log_path.write_text("\n".join(lines), encoding="utf-8")
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                code = analyzer.main([str(log_path), "--since-line", "2", "--json"])

        summary = json.loads(output.getvalue())
        self.assertEqual(code, 0)
        self.assertEqual(summary["total"]["completed"], 2)
        self.assertEqual(summary["total"]["success"], 1)
        self.assertEqual(summary["total"]["fallback"], 1)
        self.assertEqual(summary["fallbackReasons"], {"runtime_not_loaded": 1})
        self.assertEqual(summary["bySize"]["full_4k"]["success"]["avgKernelMs"], 3.75)


if __name__ == "__main__":
    unittest.main()
