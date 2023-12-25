import sys, os
import cv2
import numpy as np


def xxd(img, namespace="IMAGE"):
    yield "#pragma once"
    yield "#include <stdint.h>"
    yield f'namespace {namespace} {"{"}'
    yield f"const uint8_t width = {w};"
    yield f"const uint8_t height = {h};"
    img = np.reshape(img, (-1, 3))
    yield "const uint32_t img[] = {"
    N = 256
    for i in range(0, len(img), N):
        pixels = [
            f"0x{b:02x}{g:02x}{r:02x}00" if b or g or r else "0"
            for [b, g, r] in img[i : i + N]
        ]
        yield ",".join(pixels) + ","
    yield "}"


if __name__ == "__main__":
    filename = sys.argv[-1]
    if not os.path.exists(filename):
        print("File not found", file=sys.stderr)
        exit(1)
    elif os.path.realpath(filename) == os.path.realpath(__file__):
        print("Cannot convert myself", file=sys.stderr)
        exit(1)
    else:
        img = cv2.imread(filename, cv2.IMREAD_COLOR)
        namespace = os.path.basename(filename).replace(".", "_").upper()
        h, w, d = img.shape
        assert d == 3, f"Image must be BGR, got dims = {d}"
        with open(f"{filename}.h", "w") as f:
            print("#pragma once", file=f)
            print("#include <stdint.h>", file=f)
            print(f'namespace {namespace} {"{"}', file=f)
            print(f"const unsigned width = {w};", file=f)
            print(f"const unsigned height = {h};", file=f)
            print(f"extern const uint32_t data[];", file=f)
            print("}", file=f)
        with open(f"{filename}.cpp", "w") as f:
            print("#include <stdint.h>", file=f)
            print(f'namespace {namespace} {"{"}', file=f)
            print(f'const uint32_t data[] = {"{"}', file=f)
            data = np.reshape(img, (-1, 3))
            print(data.shape)
            N = 256
            for i in range(0, len(data), N):
                pixels = [
                    f"0x{b:02x}{g:02x}{r:02x}FF" if (b or g or r) else "0"
                    for [b, g, r] in data[i : i + N]
                ]
                print(",".join(pixels) + ",", file=f)
            print("};", file=f)
            print("}", file=f)
