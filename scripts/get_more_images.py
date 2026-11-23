#!/usr/bin/env python3
"""More test images, of kinds missing from the course set (point 4):

  med16/    real 16-bit grey medical images, as big-endian RAW files (one as
            little-endian): CT (signed; shifted so that the minimum is 0), MR,
            computed radiography. Source: github.com/pydicom/pydicom-data.
  color16/  real 16-bit colour images (PPM, maxval 65535): a 4K frame of the
            film Elephants Dream and three DCI 2K test frames.
            Source: github.com/uclouvain/openjpeg-data.
  large8/   8-bit colour photographs from the same source (640x480 to 2592x1944).

None of these was used to tune the codec. Each part has the benchmark layout
(imgs-pgm8/, imgs-ppm/, imgs-raw16/).

Needs: pip install pydicom tifffile numpy; git.
Usage: python3 scripts/get_more_images.py OUT_DIR
"""
import os
import shutil
import subprocess
import sys
import tempfile

import numpy as np
import pydicom
import tifffile

DICOM = ["693_UNCR.dcm", "MR2_UNCR.dcm", "MR-SIEMENS-DICOM-WithOverlays.dcm", "RG1_UNCR.dcm", "RG3_UNCR.dcm"]
TIFF16 = ["ElephantDream_4K.tif", "X_4_2K_24_185_CBR_WB_000.tif", "X_5_2K_24_235_CBR_STEM24_000.tif",
          "X_6_2K_24_FULL_CBR_CIRCLE_000.tif"]
PPM8 = ["Bretagne1.ppm", "Bretagne2.ppm", "Cevennes2.ppm"]


def sparse_clone(url, dst, paths):
    subprocess.run(["git", "clone", "-q", "--depth", "1", "--filter=blob:none", "--sparse", url, dst], check=True)
    subprocess.run(["git", "-C", dst, "sparse-checkout", "set", "--no-cone"] + ["/" + p for p in paths], check=True)


def main():
    out = sys.argv[1]
    for d in ("med16/imgs-raw16", "color16/imgs-ppm", "large8/imgs-ppm"):
        os.makedirs(os.path.join(out, d), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        pd = os.path.join(tmp, "pydicom-data")
        sparse_clone("https://github.com/pydicom/pydicom-data.git", pd, ["data_store/data/" + f for f in DICOM])
        for k, f in enumerate(DICOM):
            a = pydicom.dcmread(os.path.join(pd, "data_store/data", f)).pixel_array
            a = (a.astype(np.int64) - a.min()) if a.dtype.kind == "i" else a.astype(np.int64)
            h, w = a.shape
            name = os.path.splitext(f)[0]
            le = k == 1  # one little-endian file, to exercise that path
            path = os.path.join(out, "med16/imgs-raw16", f"{name}-u16{'le' if le else 'be'}-1x{h}x{w}.raw")
            a.astype("<u2" if le else ">u2").tofile(path)
        oj = os.path.join(tmp, "openjpeg-data")
        sparse_clone("https://github.com/uclouvain/openjpeg-data.git", oj,
                     ["input/nonregression/" + f for f in TIFF16 + PPM8])
        for f in TIFF16:
            a = tifffile.imread(os.path.join(oj, "input/nonregression", f))
            h, w = a.shape[:2]
            with open(os.path.join(out, "color16/imgs-ppm", os.path.splitext(f)[0] + ".ppm"), "wb") as fh:
                fh.write(b"P6\n%d %d\n65535\n" % (w, h) + a.astype(">u2").tobytes())
        for f in PPM8:
            shutil.copy(os.path.join(oj, "input/nonregression", f), os.path.join(out, "large8/imgs-ppm", f))
    print("images written to", out)


if __name__ == "__main__":
    main()
