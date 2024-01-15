import sys, os
import cv2
import numpy as np


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
        NAME = os.path.basename(filename).replace(".", "_").upper()
        h, w, d = img.shape
        assert d == 3, f"Image must be BGR, got dims = {d}"

        with open(f"{filename}.h", "w") as f:
            print("#pragma once", file=f)
            print("#include <stdint.h>", file=f)
            print(f'extern "C" const unsigned {NAME}_W;', file=f)
            print(f'extern "C" const unsigned {NAME}_H;', file=f)
            print(f'extern "C" const uint32_t {NAME}_DATA[];', file=f)

        with open(f"{filename}.c", "w") as f:
            print("#include <stdint.h>", file=f)
            print(f"const unsigned {NAME}_W = {w};", file=f)
            print(f"const unsigned {NAME}_H = {h};", file=f)
            print(f'const uint32_t {NAME}_DATA[{h * w}] = {"{"}', file=f)
            if img.shape[2] == 4:
                img = cv2.cvtColor(img, cv2.COLOR_BGRA2BGR)
            for [b, g, r] in np.reshape(img, (-1, 3)):
                pixel = f"0xFF{r:02x}{g:02x}{b:02x}" if (b or g or r) else "0"
                print(pixel + ",", file=f, end="")
            print("};", file=f)
