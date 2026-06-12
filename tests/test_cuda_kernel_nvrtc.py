from pathlib import Path
import ctypes
import os
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "ofx" / "DuXunFilm" / "DuXunFilmSim.cpp"


def extract_cuda_source(source: str) -> str:
    start = source.find("static const char* kCudaKernelSource")
    assert start != -1, "missing embedded CUDA kernel source"
    end = source.find(";\n\n#if DUXUN_ENABLE_CUDA_RENDER", start)
    assert end != -1, "missing end of embedded CUDA kernel source"
    chunks = re.findall(r'R"CUDA\((.*?)\)CUDA"', source[start:end], re.S)
    assert chunks, "missing embedded CUDA kernel source"
    return "".join(chunks)


def find_nvrtc():
    candidates = [
        Path(r"C:\Program Files\Blackmagic Design\DaVinci Resolve\nvrtc64_120_0.dll"),
        Path(r"C:\Program Files\Blackmagic Design\DaVinci Resolve\nvrtc64_80.dll"),
        Path(r"C:\Windows\System32\nvrtc64_120_0.dll"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


class CudaKernelNvrtcTests(unittest.TestCase):
    def test_embedded_cuda_source_declares_p1_glow_kernels(self):
        source = SOURCE.read_text(encoding="utf-8")
        kernel_source = extract_cuda_source(source)
        for kernel_name in [
            "cudaHighlightMaskKernel",
            "cudaDownsampleKernel",
            "cudaBlurHorizontalKernel",
            "cudaBlurVerticalKernel",
            "cudaCompositeGlowKernel",
        ]:
            self.assertRegex(
                kernel_source,
                re.compile(rf'extern\s+"C"\s+__global__\s+void\s+{kernel_name}\s*\('),
            )
        for cpu_only_token in ["GlowBuffer", "buildGlowBuffer", "std::", "malloc", "free("]:
            self.assertNotIn(cpu_only_token, kernel_source)

    def test_embedded_cuda_halation_is_red_orange_not_white_rgb_blur(self):
        source = SOURCE.read_text(encoding="utf-8")
        kernel_source = extract_cuda_source(source)
        for token in [
            "float redLeak = fmaxf(pr, lum * 1.12f) * w",
            "mask[i + 1] = redLeak * 0.30f",
            "mask[i + 2] = redLeak * 0.025f",
            "float ringBias = 1.15f - 0.75f * duxunCudaSmoothstep(0.72f, 1.10f, baseLum)",
            "float haloTint = duxunClamp((veil * ringBias * (0.48f + 0.38f * warm))",
            "float redB = r * (0.018f + 0.045f * (1.0f - warm))",
            "float fHalationBlueComp",
            "float fHalationImpact",
        ]:
            self.assertIn(token, kernel_source)
        self.assertNotIn("mask[i + 0] = pr * w;\n    mask[i + 1] = pg * w;\n    mask[i + 2] = pb * w;", kernel_source)

    def test_embedded_cuda_kernel_compiles_with_resolve_nvrtc(self):
        source = SOURCE.read_text(encoding="utf-8")
        kernel_source = extract_cuda_source(source).encode("utf-8")

        nvrtc_path = find_nvrtc()
        if nvrtc_path is None:
            self.skipTest("NVRTC DLL not available on this machine")

        old_path = os.environ.get("PATH", "")
        os.environ["PATH"] = str(nvrtc_path.parent) + os.pathsep + old_path
        dll_dir = os.add_dll_directory(str(nvrtc_path.parent)) if hasattr(os, "add_dll_directory") else None
        nvrtc = ctypes.CDLL(str(nvrtc_path))
        program = ctypes.c_void_p()
        nvrtc.nvrtcCreateProgram.argtypes = [
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_int,
            ctypes.c_void_p,
            ctypes.c_void_p,
        ]
        nvrtc.nvrtcCreateProgram.restype = ctypes.c_int
        nvrtc.nvrtcCompileProgram.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_char_p)]
        nvrtc.nvrtcCompileProgram.restype = ctypes.c_int
        nvrtc.nvrtcGetProgramLogSize.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_size_t)]
        nvrtc.nvrtcGetProgramLogSize.restype = ctypes.c_int
        nvrtc.nvrtcGetProgramLog.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        nvrtc.nvrtcGetProgramLog.restype = ctypes.c_int
        nvrtc.nvrtcDestroyProgram.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
        nvrtc.nvrtcDestroyProgram.restype = ctypes.c_int

        create_status = nvrtc.nvrtcCreateProgram(
            ctypes.byref(program),
            kernel_source,
            b"DuXunFilmSimCudaKernel.cu",
            0,
            None,
            None,
        )
        self.assertEqual(create_status, 0)

        options = (ctypes.c_char_p * 2)(b"--std=c++11", b"--gpu-architecture=compute_52")
        compile_status = nvrtc.nvrtcCompileProgram(program, 2, options)

        log_size = ctypes.c_size_t()
        nvrtc.nvrtcGetProgramLogSize(program, ctypes.byref(log_size))
        log = ctypes.create_string_buffer(log_size.value if log_size.value else 1)
        nvrtc.nvrtcGetProgramLog(program, log)
        nvrtc.nvrtcDestroyProgram(ctypes.byref(program))
        if dll_dir is not None:
            dll_dir.close()
        os.environ["PATH"] = old_path

        self.assertEqual(compile_status, 0, log.value.decode("utf-8", errors="replace"))


if __name__ == "__main__":
    unittest.main()
