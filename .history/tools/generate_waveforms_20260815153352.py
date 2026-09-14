# -*- coding: utf-8 -*-
"""
NuPG风格单周期波形生成器：每个波形2048 samples、峰值归一化、首尾平滑，
输出16-bit mono wav到 tools/waveforms/，可直接load进pgWavetable。
用法: python3 generate_waveforms.py [输出目录]
依赖: numpy (pip install numpy)
"""
import os
import sys
import wave

import numpy as np

N = 2048
SR = 96000  # wav头采样率，对单周期波形只是元数据
t = np.linspace(0.0, 1.0, N, endpoint=False)  # 归一化相位 0~1


def normalize(x):
    peak = np.max(np.abs(x))
    return x / peak if peak > 0 else x


def sine(harmonics):
    """加法合成: [(次数, 幅度, 相位), ...]"""
    out = np.zeros(N)
    for h, a, ph in harmonics:
        out += a * np.sin(2 * np.pi * h * t + ph)
    return out


def band_limited(kind, num_h=64):
    """带限saw/square/triangle: 谐波求和避免数字混叠"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        if kind == "saw":
            out += ((-1) ** (k + 1)) * np.sin(2 * np.pi * k * t) / k
        elif kind == "square" and k % 2 == 1:
            out += np.sin(2 * np.pi * k * t) / k
        elif kind == "triangle" and k % 2 == 1:
            out += ((-1) ** ((k - 1) // 2)) * np.sin(2 * np.pi * k * t) / (k * k)
    return out


def gauss_pulse(width=0.15, carrier_h=1):
    """高斯包络×正弦载波: pulsar合成经典pulsaret，width越小频谱越宽"""
    env = np.exp(-0.5 * ((t - 0.5) / width) ** 2)
    return env * np.sin(2 * np.pi * carrier_h * t)


def fof(formant_h=8, bw=0.12):
    """FOF粒: 指数衰减包络×载波，模拟单个声道共振峰的冲激响应"""
    env = np.exp(-t / bw) * (1 - np.exp(-t / 0.02))  # 快攻击慢衰减
    return env * np.sin(2 * np.pi * formant_h * t)


def vosim(pulses=5, decay=0.75, carrier_h=12):
    """VOSIM(Kaegi): 一串逐个衰减的sin^2脉冲，声音像元音"""
    out = np.zeros(N)
    seg = N // pulses
    for i in range(pulses):
        seg_t = np.linspace(0, 1, seg, endpoint=False)
        out[i * seg:(i + 1) * seg] = (decay ** i) * np.sin(np.pi * seg_t * carrier_h / pulses) ** 2 * np.sin(2 * np.pi * seg_t)
    return out


def sinc_wave(lobes=8):
    """sinc函数: 频谱近似矩形，声音亮而空"""
    x = (t - 0.5) * 2 * lobes * np.pi
    x[x == 0] = 1e-12
    return np.sin(x) / x


def expodec(carrier_h=6, decay=0.2):
    """expodec: 指数衰减×正弦，Roads《Microsound》里的经典粒形"""
    return np.exp(-t / decay) * np.sin(2 * np.pi * carrier_h * t)


def rexpodec(carrier_h=6, decay=0.2):
    """反向expodec: 指数上升，反着的attack感"""
    return expodec(carrier_h, decay)[::-1].copy()


def chebyshev(order):
    """Chebyshev多项式T_n(cos): 纯第n次谐波+波形失真质感"""
    x = np.cos(2 * np.pi * t)
    return np.cos(order * np.arccos(np.clip(x, -1, 1)))


def phase_distort(amount=0.7):
    """相位失真(Casio CZ式): 弯曲读表相位得到类共振峰亮度"""
    knee = 0.5 * (1 - amount)
    phase = np.where(t < knee, t * 0.5 / max(knee, 1e-9), 0.5 + (t - knee) * 0.5 / max(1 - knee, 1e-9))
    return np.sin(2 * np.pi * phase)


def fold(drive=3.0):
    """wavefold: 正弦过驱动折叠，西海岸质感"""
    return np.sin(drive * np.pi * np.sin(2 * np.pi * t))


def smooth_ends(x, fade=32):
    """首尾极短淡入淡出，保证wrap无click"""
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    x[:fade] *= w
    x[-fade:] *= w[::-1]
    return x


WAVEFORMS = {
    # 基础
    "sine": sine([(1, 1, 0)]),
    "saw_bl": band_limited("saw"),
    "square_bl": band_limited("square"),
    "triangle_bl": band_limited("triangle"),
    # pulsar经典pulsaret
    "gauss_narrow": gauss_pulse(0.08, 3),
    "gauss_wide": gauss_pulse(0.22, 1),
    "sinc8": sinc_wave(8),
    "expodec": expodec(6, 0.2),
    "rexpodec": rexpodec(6, 0.2),
    # 共振峰/语音
    "fof_low": fof(5, 0.18),
    "fof_high": fof(13, 0.08),
    "vosim_a": vosim(5, 0.75, 12),
    "vosim_o": vosim(3, 0.6, 7),
    # 加法/失真
    "organ": sine([(1, 1, 0), (2, 0.5, 0), (3, 0.33, 0), (4, 0.25, 0), (6, 0.15, 0), (8, 0.1, 0)]),
    "odd_stack": sine([(1, 1, 0), (3, 0.6, 0.5), (5, 0.4, 1.1), (7, 0.25, 0.3), (9, 0.15, 2.0)]),
    "cheby3": chebyshev(3),
    "cheby5": chebyshev(5),
    "phasedist": phase_distort(0.7),
    "fold3": fold(3.0),
    "fold5": fold(5.0),
}


def write_wav(path, data):
    pcm = (np.clip(data, -1, 1) * 32767).astype(np.int16)
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(pcm.tobytes())


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "waveforms")
    os.makedirs(out_dir, exist_ok=True)
    for name, data in WAVEFORMS.items():
        write_wav(os.path.join(out_dir, name + ".wav"), smooth_ends(normalize(data.copy())))
        print("wrote", name + ".wav")
    print("done:", len(WAVEFORMS), "waveforms ->", out_dir)


if __name__ == "__main__":
    main()
