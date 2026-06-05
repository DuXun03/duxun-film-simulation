"""
OFX Plugin Mock Host Test
Simulates DaVinci Resolve's OFX initialization to find where our plugin fails.
"""
import ctypes
import ctypes.wintypes
import sys

# Load DLL
DLL_PATH = r'C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx'
BUILD_PATH = r'C:\Users\LI\WorkBuddy\2026-05-28-19-53-09\film-sim-plugin\build\DuXunFilmSim.ofx'

for path in [DLL_PATH, BUILD_PATH]:
    try:
        dll = ctypes.WinDLL(path)
        test_path = path
        break
    except Exception:
        continue
else:
    print("Cannot find DLL!")
    sys.exit(1)

print(f"Testing: {test_path}")

# ===== OfxStatus values =====
kOfxStatOK = 0
kOfxStatFailed = 1
kOfxStatErrBadHandle = 9
kOfxStatErrMemory = 10
kOfxStatReplyDefault = 13
kOfxStatReplyYes = 14
kOfxStatReplyNo = 15

# ===== Callback functions for mock host =====

# We need to implement the functions that the plugin will call back into our mock host.
# The plugin calls gPropSuite->propSetString, gPropSuite->propSetDouble, etc.
# We need to track what properties are being set.

set_properties = {}  # (handle_ptr, prop_name) -> value
get_properties = {}

def make_property_suite():
    """Create a mock OfxPropertySuiteV1"""
    class PropertySuite(ctypes.Structure):
        _fields_ = [
            ('propSetPointer', ctypes.c_void_p),
            ('propGetPointer', ctypes.c_void_p),
            ('propSetString', ctypes.c_void_p),
            ('propGetString', ctypes.c_void_p),
            ('propSetDouble', ctypes.c_void_p),
            ('propGetDouble', ctypes.c_void_p),
            ('propSetInt', ctypes.c_void_p),
            ('propGetInt', ctypes.c_void_p),
        ]
    return PropertySuite()

def make_image_effect_suite():
    class ImageEffectSuite(ctypes.Structure):
        _fields_ = [
            ('clipDefine', ctypes.c_void_p),
            ('clipGetPropertySet', ctypes.c_void_p),
            ('clipGetHandle', ctypes.c_void_p),
            ('clipGetImage', ctypes.c_void_p),
            ('clipReleaseImage', ctypes.c_void_p),
            ('getPropertySet', ctypes.c_void_p),
            ('getParamSet', ctypes.c_void_p),
        ]
    return ImageEffectSuite()

def make_param_suite():
    class ParamSuite(ctypes.Structure):
        _fields_ = [
            ('paramDefine', ctypes.c_void_p),
            ('paramGetHandle', ctypes.c_void_p),
            ('paramGetValueAtTime', ctypes.c_void_p),
        ]
    return ParamSuite()

def make_memory_suite():
    class MemorySuite(ctypes.Structure):
        _fields_ = [
            ('memoryAlloc', ctypes.c_void_p),
            ('memoryFree', ctypes.c_void_p),
        ]
    return MemorySuite()

def make_multi_thread_suite():
    class MultiThreadSuite(ctypes.Structure):
        _fields_ = [
            ('mutexCreate', ctypes.c_void_p),
            ('mutexDestroy', ctypes.c_void_p),
            ('mutexLock', ctypes.c_void_p),
            ('mutexUnlock', ctypes.c_void_p),
        ]
    return MultiThreadSuite()

# Allocate the suites as ctypes objects (they must persist in memory)
prop_suite = make_property_suite()
effect_suite = make_image_effect_suite()
param_suite = make_param_suite()
memory_suite = make_memory_suite()
thread_suite = make_multi_thread_suite()

# Now build the OfxHost struct
# OfxHost { OfxPropertySetHandle host; const char* (*fetchSuite)(OfxPropertySetHandle, const char*, int); }
class OfxHost(ctypes.Structure):
    _fields_ = [
        ('host', ctypes.c_void_p),  # OfxPropertySetHandle
        ('fetchSuite', ctypes.c_void_p),  # function pointer
    ]

# Create the host object
host = OfxHost()

# The fetchSuite function needs to be a C callable that returns suite pointers
# We'll implement it to return our mock suites

SUITE_MAP = {
    b'OfxPropertySuite': ctypes.addressof(prop_suite),
    b'OfxImageEffectSuite': ctypes.addressof(effect_suite),
    b'OfxParameterSuite': ctypes.addressof(param_suite),
    b'OfxMemorySuite': ctypes.addressof(memory_suite),
    b'OfxMultiThreadSuite': ctypes.addressof(thread_suite),
}

@ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
def mock_fetch_suite(host_handle, suite_name, suite_version):
    name = suite_name
    if name in SUITE_MAP:
        print(f"  fetchSuite(\"{name.decode()}\", v{suite_version}) -> {hex(SUITE_MAP[name])}")
        return SUITE_MAP[name]
    else:
        print(f"  fetchSuite(\"{name.decode()}\", v{suite_version}) -> NULL (unknown suite)")
        return 0

host.fetchSuite = ctypes.cast(mock_fetch_suite, ctypes.c_void_p)
host.host = 0  # null property set for host

print("\n=== Step 1: OfxGetNumberOfPlugins ===")
dll.OfxGetNumberOfPlugins.restype = ctypes.c_int
num = dll.OfxGetNumberOfPlugins()
print(f"  Result: {num}")

print("\n=== Step 2: OfxGetPlugin ===")
dll.OfxGetPlugin.restype = ctypes.c_void_p
dll.OfxGetPlugin.argtypes = [ctypes.c_int]
plugin_ptr = dll.OfxGetPlugin(0)

class OfxPlugin(ctypes.Structure):
    _fields_ = [
        ('pluginApi', ctypes.c_char_p),
        ('apiVersion', ctypes.c_int),
        ('pluginIdentifier', ctypes.c_char_p),
        ('pluginVersionMajor', ctypes.c_int),
        ('pluginVersionMinor', ctypes.c_int),
        ('setHost', ctypes.c_void_p),
        ('mainEntry', ctypes.c_void_p),
    ]

p = OfxPlugin.from_address(plugin_ptr)
print(f"  pluginApi: {p.pluginApi.decode()}")
print(f"  apiVersion: {p.apiVersion}")
print(f"  pluginIdentifier: {p.pluginIdentifier.decode()}")
print(f"  setHost: {hex(p.setHost) if p.setHost else 'NULL'}")
print(f"  mainEntry: {hex(p.mainEntry) if p.mainEntry else 'NULL'}")

print("\n=== Step 3: setHost (via plugin struct) ===")
# The setHost in OfxPlugin struct takes (OfxHost*)
SET_HOST_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
set_host_func = SET_HOST_TYPE(p.setHost)
try:
    set_host_func(ctypes.addressof(host))
    print("  setHost() called successfully (no crash)")
except Exception as e:
    print(f"  setHost() CRASHED: {e}")
    import traceback
    traceback.print_exc()
    print("\n>>> ROOT CAUSE: setHost crashes! This is why Resolve can't load the plugin.")
    sys.exit(1)

print("\n=== Step 4: mainEntry(describe) ===")
MAIN_ENTRY_TYPE = ctypes.CFUNCTYPE(
    ctypes.c_int,  # return
    ctypes.c_char_p,  # action
    ctypes.c_void_p,  # handle
    ctypes.c_void_p,  # inArgs
    ctypes.c_void_p,  # outArgs
)
main_entry = MAIN_ENTRY_TYPE(p.mainEntry)

# The plugin needs a non-null handle for describe, but our mock suites are empty
# so it will try to call propSetString on null function pointers and crash.
# Let's wrap it in SEH
import subprocess

# Use a subprocess to test for crash
test_code = f"""
import ctypes, ctypes.wintypes, sys

dll = ctypes.WinDLL(r'{test_path}')
dll.OfxGetPlugin.restype = ctypes.c_void_p
dll.OfxGetPlugin.argtypes = [ctypes.c_int]
plugin_ptr = dll.OfxGetPlugin(0)

class OfxPlugin(ctypes.Structure):
    _fields_ = [
        ('pluginApi', ctypes.c_char_p),
        ('apiVersion', ctypes.c_int),
        ('pluginIdentifier', ctypes.c_char_p),
        ('pluginVersionMajor', ctypes.c_int),
        ('pluginVersionMinor', ctypes.c_int),
        ('setHost', ctypes.c_void_p),
        ('mainEntry', ctypes.c_void_p),
    ]
p = OfxPlugin.from_address(plugin_ptr)

# Call setHost with real mock
class OfxHost(ctypes.Structure):
    _fields_ = [
        ('host', ctypes.c_void_p),
        ('fetchSuite', ctypes.c_void_p),
    ]

class PropertySuite(ctypes.Structure):
    _fields_ = [('propSetPointer',ctypes.c_void_p),('propGetPointer',ctypes.c_void_p),('propSetString',ctypes.c_void_p),('propGetString',ctypes.c_void_p),('propSetDouble',ctypes.c_void_p),('propGetDouble',ctypes.c_void_p),('propSetInt',ctypes.c_void_p),('propGetInt',ctypes.c_void_p)]
class EffectSuite(ctypes.Structure):
    _fields_ = [('clipDefine',ctypes.c_void_p),('clipGetPropertySet',ctypes.c_void_p),('clipGetHandle',ctypes.c_void_p),('clipGetImage',ctypes.c_void_p),('clipReleaseImage',ctypes.c_void_p),('getPropertySet',ctypes.c_void_p),('getParamSet',ctypes.c_void_p)]
class ParamSuite(ctypes.Structure):
    _fields_ = [('paramDefine',ctypes.c_void_p),('paramGetHandle',ctypes.c_void_p),('paramGetValueAtTime',ctypes.c_void_p)]
class MemorySuite(ctypes.Structure):
    _fields_ = [('memoryAlloc',ctypes.c_void_p),('memoryFree',ctypes.c_void_p)]
class ThreadSuite(ctypes.Structure):
    _fields_ = [('mutexCreate',ctypes.c_void_p),('mutexDestroy',ctypes.c_void_p),('mutexLock',ctypes.c_void_p),('mutexUnlock',ctypes.c_void_p)]

prop_suite = PropertySuite()
effect_suite = EffectSuite()
param_suite = ParamSuite()
mem_suite = MemorySuite()
thr_suite = ThreadSuite()

SUITE_MAP = {{
    b'OfxPropertySuite': ctypes.addressof(prop_suite),
    b'OfxImageEffectSuite': ctypes.addressof(effect_suite),
    b'OfxParameterSuite': ctypes.addressof(param_suite),
    b'OfxMemorySuite': ctypes.addressof(mem_suite),
    b'OfxMultiThreadSuite': ctypes.addressof(thr_suite),
}}

@ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int)
def fetch_suite(h, name, ver):
    return SUITE_MAP.get(name, 0)

host = OfxHost()
host.fetchSuite = ctypes.cast(fetch_suite, ctypes.c_void_p)
host.host = 0

SET_HOST_TYPE = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
set_host_func = SET_HOST_TYPE(p.setHost)
set_host_func(ctypes.addressof(host))

# Now call describe
MAIN_ENTRY_TYPE = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
main_entry = MAIN_ENTRY_TYPE(p.mainEntry)

# Use 0x1 as a fake handle
result = main_entry(b'OfxImageEffectActionDescribe', 0x1, 0, 0)
print(f'describe returned: {{result}}')
sys.stdout.flush()
"""

result = subprocess.run(
    [sys.executable, '-c', test_code],
    capture_output=True, text=True, timeout=5
)
print(f"  stdout: {result.stdout.strip()}")
if result.stderr.strip():
    print(f"  stderr: {result.stderr.strip()}")
if result.returncode != 0:
    print(f"  >>> CRASH! Exit code: {result.returncode}")
    print("  >>> ROOT CAUSE: describe action crashes!")
else:
    ret = int(result.stdout.strip().split(':')[-1].strip())
    if ret != 0:
        print(f"  >>> describe returned error code {ret}")
    else:
        print(f"  >>> describe succeeded!")

print("\n=== Diagnostics complete ===")
