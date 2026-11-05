import os
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))

grain = ["grain.ivf", "grain10.ivf", "grain12.ivf", "grain444.ivf", "grain422.ivf", "svtgrain.ivf", "grain.mp4"]
oppoints = {"aom_svc.ivf": [0, 1, 2, 3], "aom_svct.ivf": [0, 1]}


def framemd5(path, extra):
    cmd = ["ffmpeg", "-hide_banner", "-loglevel", "error"] + extra + ["-c:v", "libdav1d", "-i", path, "-f", "framemd5", "-autoscale", "0", "-fps_mode", "passthrough", "-"]
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    return [line.split(",")[-1].strip() for line in out.splitlines() if line and not line.startswith("#")]


def main():
    lines = []
    for name in sorted(os.listdir(here)):
        if name.endswith(".py") or name.endswith(".txt"):
            continue
        path = os.path.join(here, name)
        if name.endswith(".av1b"):
            continue
        if name in oppoints:
            for op in oppoints[name]:
                lines.append(" ".join([name, "op=%d" % op] + framemd5(path, ["-oppoint", str(op)])))
            continue
        lines.append(" ".join([name, "-"] + framemd5(path, [])))
        if name in grain:
            lines.append(" ".join([name, "nograin"] + framemd5(path, ["-filmgrain", "0"])))
    with open(os.path.join(here, "manifest.txt"), "w") as f:
        f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
