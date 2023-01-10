import os
import xarray as xr
import wave
import matplotlib.pyplot as plt
import numpy as np

def create_wav(log_file, out_file, fs=16000, sample_width=2):
    """
    Creates a wav file for a netcdf log

    Args:
        log_file (_type_): input log file
        out_file (_type_): _description_
        fs (int, optional): _description_. Defaults to 16000.
        sample_width (int, optional): _description_. Defaults to 2.
    """
    ds = xr.open_dataset(log_file)

    f = wave.open(out_file, 'w')
    f.setnchannels(1)
    f.setsampwidth(sample_width)
    f.setframerate(fs)
    audio = ds['main.audio'][:, :, 0].values.flatten()

    audio = audio * 2 ** 15
    f.writeframes(audio.astype(np.int16).tobytes())
    f.close()

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description="Convert log to wav")
    parser.add_argument("log_file", help="Path to log file")
    parser.add_argument("out_file", help="Path to output wav file")
    parser.add_argument("--fs", type=int, default=16000, help="Sample rate")
    parser.add_argument("--sample_width", type=int, default=2, help="Sample width")
    opts = parser.parse_args()
    create_wav(opts.log_file, opts.out_file, opts.fs, opts.sample_width)
