#!/usr/bin/env python3
"""Security tests for Arctis Control.

Builds the app with AddressSanitizer + UBSan against a fake headsetcontrol and a fake pactl
and feeds it hostile input: malformed, huge, endless, hanging or crashing headsetcontrol output,
HTML in names and error messages, absurd equalizer data, malicious state.json files (huge,
deeply nested, wrong types, out-of-range values, FIFO, directory, dangling symlink) and
malicious PipeWire card data (garbage, huge, hanging, HTML and option-like names).

Each case must exit cleanly, without sanitizer reports and without any network request
(an HTTP server listens for <img src="http://..."> leaks).

Usage:  python3 tests/security/run.py      (from the repository root; takes a few minutes)
"""
import http.server, json, os, re, shutil, subprocess, sys, tempfile, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
WORK = tempfile.mkdtemp(prefix="arctis-sec-")
FAKE = os.path.join(WORK, "fake-headsetcontrol")
FAKE_PACTL = os.path.join(WORK, "fake-pactl")
BUILD = os.path.join(WORK, "build")
shutil.copy(os.path.join(HERE, "fake-headsetcontrol"), FAKE)
shutil.copy(os.path.join(HERE, "fake-pactl"), FAKE_PACTL)

print("Building with ASan/UBSan…")
subprocess.run(["cmake", "-S", ROOT, "-B", BUILD, "-DCMAKE_BUILD_TYPE=Debug", f"-DARCTIS_HEADSETCONTROL={FAKE}", f"-DARCTIS_PACTL={FAKE_PACTL}",
                "-DCMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"],
               check=True, stdout=subprocess.DEVNULL)
subprocess.run(["cmake", "--build", BUILD, "-j", str(os.cpu_count())], check=True, stdout=subprocess.DEVNULL)
APP = os.path.join(BUILD, "arctis-control")

hits = []
class Trap(http.server.BaseHTTPRequestHandler):
    def do_GET(self): hits.append(self.path); self.send_response(404); self.end_headers()
    def log_message(self, *a): pass
server = http.server.HTTPServer(("127.0.0.1", 18777), Trap)
threading.Thread(target=server.serve_forever, daemon=True).start()

failures = 0
def run(name, scenario, args, cfg, expect_exit=0, pactl="ok"):
    global failures
    open(os.path.join(WORK, "scenario"), "w").write(scenario + "\n")
    open(os.path.join(WORK, "pactl-scenario"), "w").write(pactl + "\n")
    env = dict(os.environ, XDG_CONFIG_HOME=cfg, LANG="en_US.UTF-8", QT_QPA_PLATFORM="offscreen",
               ASAN_OPTIONS="detect_leaks=0")
    t0 = time.time()
    try:
        p = subprocess.run([APP] + args, env=env, capture_output=True, text=True, timeout=60)
        code, out = p.returncode, p.stdout + p.stderr
    except subprocess.TimeoutExpired:
        code, out = "hang", ""
    reports = len(re.findall(r"ERROR: AddressSanitizer|runtime error:", out))
    ok = code == expect_exit and reports == 0
    failures += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'}  {name:24} exit={code} sanitizer_reports={reports} {time.time() - t0:.1f}s")

def fresh_cfg(name, prepare=None):
    cfg = os.path.join(WORK, "cfg-" + name)
    os.makedirs(os.path.join(cfg, "arctis-control"))
    if prepare:
        prepare(os.path.join(cfg, "arctis-control", "state.json"))
    return cfg

shot = ["--screenshot", os.path.join(WORK, "shot.png")]
for sc in ["garbage", "empty_fail", "huge", "endless", "hang", "crash", "html_names", "many_devices",
           "weird_eq", "bad_types", "deep", "regex_bomb"]:
    run("headsetcontrol:" + sc, sc, shot, fresh_cfg(sc))

run("self-test ok", "ok_test", ["--test-device", "--self-test"], fresh_cfg("selftest-ok"))

# Headset microphone through PipeWire (fake pactl). The real Nova 5 profiles with the mic
# off: enabling it must pick the same output mode with input.
pactl_log = os.path.join(WORK, "pactl-calls.log")
open(pactl_log, "w").close()
run("pactl: enable mic", "ok_test", ["--test-device", "--self-test"], fresh_cfg("mic-ok"))
sets = [l for l in open(pactl_log).read().splitlines() if "set-card-profile" in l]
expected = "set-card-profile alsa_card.usb-Fake_Headset-00 output:analog-stereo+input:mono-fallback"
ok = len(sets) == 1 and sets[0].endswith(expected)
failures += 0 if ok else 1
print(f"{'PASS' if ok else 'FAIL'}  pactl: picks right profile   ({sets})")
# Old HeadsetControl without "-d": writes only with a single listed device, never with two.
calls_log = os.path.join(WORK, "calls.log")
mark = len(open(calls_log).read().splitlines())
run("old headsetcontrol, 1 device", "old_one", ["--test-device", "--self-test"], fresh_cfg("old-one"))
mark2 = len(open(calls_log).read().splitlines())
run("old headsetcontrol, 2 devices", "old_two", ["--test-device", "--self-test"], fresh_cfg("old-two"), expect_exit=14)
new_calls = open(calls_log).read().splitlines()
WRITE = r" -(s|l|i|v|r|p|e|n) | --(microphone|volume|bt|noise)"
old_one_writes = [c for c in new_calls[mark:mark2] if re.search(WRITE, c)]
old_two_writes = [c for c in new_calls[mark2:] if re.search(WRITE, c)]
ok = len(old_one_writes) == 14 and not any(" -d " in c for c in old_one_writes) and not old_two_writes
failures += 0 if ok else 1
print(f"{'PASS' if ok else 'FAIL'}  old version targeting       ({len(old_one_writes)} writes with 1 device, {len(old_two_writes)} with 2)")
run("pactl: set fails", "ok_test", ["--test-device", "--self-test"], fresh_cfg("mic-fail"), expect_exit=1, pactl="set_fail")
for sc in ["garbage", "huge", "hang", "html", "dash_names"]:
    open(pactl_log, "w").close()
    run("pactl:" + sc, "ok_test", ["--test-device", "--self-test"], fresh_cfg("mic-" + sc), pactl=sc)
    if any("set-card-profile" in l for l in open(pactl_log)):
        failures += 1
        print(f"FAIL  pactl:{sc} must not change any profile")
run("self-test html errors", "actions_html", ["--test-device", "--self-test"], fresh_cfg("selftest-err"), expect_exit=14)

dev = "1038:a00c"
states = {
    "garbage": lambda p: open(p, "wb").write(os.urandom(300000)),
    "huge": lambda p: open(p, "w").write('{"devices":{"%s":{"eq_custom":[%s]}}}' % (dev, ",".join(["1"] * 3_000_000))),
    "deep": lambda p: open(p, "w").write('{"devices":' + "[" * 200000 + "]" * 200000 + "}"),
    "wrong_types": lambda p: json.dump({"devices": {dev: {"sidetone": "x", "eq_preset": [1], "eq_custom": "y"}}}, open(p, "w")),
    "out_of_range": lambda p: json.dump({"devices": {dev: {"sidetone": 10**12, "inactive_time": -5, "eq_custom": [1e308] * 10}}}, open(p, "w")),
    "10k_devices": lambda p: json.dump({"devices": {f"{i:04x}:{i:04x}": {"sidetone": 1} for i in range(10000)}}, open(p, "w")),
    "bad_ids": lambda p: json.dump({"devices": {"../../etc": {}, "x" * 100000: {}, "": {}}}, open(p, "w")),
    "fifo": lambda p: os.mkfifo(p),
    "directory": lambda p: os.mkdir(p),
    "dangling_symlink": lambda p: os.symlink("/nonexistent/file", p),
}
for name, prepare in states.items():
    run("state.json:" + name, "ok", shot, fresh_cfg("state-" + name, prepare))

# Every write command must target exactly one device with -d.
calls = open(os.path.join(WORK, "calls.log")).read().splitlines()
writes = [c for c in calls if re.search(r" -(s|l|i|v|r|p|e|n) | --(microphone|volume|bt|noise)", c)
          and not c.startswith("old_")]
untargeted = [c for c in writes if " -d 0x" not in c]
failures += 1 if untargeted else 0
print(f"{'PASS' if not untargeted else 'FAIL'}  every write has -d          ({len(writes)} writes checked)")
failures += 1 if hits else 0
print(f"{'PASS' if not hits else 'FAIL'}  no network requests          ({len(hits)} received)")

server.shutdown()
shutil.rmtree(WORK, ignore_errors=True)
print(f"\n{'All tests passed' if failures == 0 else f'{failures} test(s) failed'}")
sys.exit(1 if failures else 0)
