import os
import xarray as xr
import wave
import matplotlib.pyplot as plt
import numpy as np

if __name__ == '__main__':
    log_file = r"C:\git\dsplib\blob_support\script\log_09_24_2022-12_29_27\192.168.50.26.nc"
    out_file = os.path.join(os.path.dirname(log_file), 'capture.wav')
    ds = xr.open_dataset(log_file)

    print(ds['main.rcv_bytes'][:, :, 0].values.flatten().mean())

    f = wave.open(out_file, 'w')
    f.setnchannels(1)
    f.setsampwidth(2)
    f.setframerate(16000)
    audio = ds['main.audio'][:, :128, 0].values.flatten()
    ch1 = audio[0::2]
    ch2 = audio[1::2]

    f.writeframes(audio.astype(np.int16).tobytes())
    f.close()
    breakpoint()


    
