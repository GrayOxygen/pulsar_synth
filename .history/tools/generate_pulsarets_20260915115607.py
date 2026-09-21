# -*- coding: utf-8 -*-
"""
nuPG风格复杂pulsaret生成器：每次运行随机构建一批合成管线，
每个波形 = 随机家族生成器 + 随机多段包络 + 随机后处理链(0~3个算子)，
复杂度远高于固定配方——没有两次运行会产生相同的bank。
输出: 9600 samples、16-bit mono wav，可直接load进pgWavetable。
用法: python3 generate_pulsarets.py [输出目录] [--seed N] [--count M]
  --seed  复现某一整套bank(每次运行会打印本次seed)
  --count 生成数量(默认64)
依赖: numpy (pip install numpy)
"""
import argparse
import os
import secrets
import wave

import numpy as np

N = 9600
SR = 9600  # wav头采样率，对单周期波形只是元数据
TWO_PI = 2.0 * np.pi
t = np.linspace(0.0, 1.0, N, endpoint=False)


# ============================ 基础工具 ============================

def normalize(x):
    peak = np.max(np.abs(x))
    return x / peak if peak > 0 else x


def smooth_ends(x, fade=32):
    """首尾极短淡入淡出，保证wrap无click"""
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    x[:fade] *= w
    x[-fade:] *= w[::-1]
    return x


def breakpoint_env(rng, zero_ends=True):
    """随机多段包络：3~8个断点、余弦插值、随机曲率——nuPG式任意包络形状"""
    n = int(rng.integers(3, 9))
    xs = np.sort(np.concatenate(([0.0, 1.0], rng.uniform(0.03, 0.97, n - 2))))
    ys = rng.uniform(0.0, 1.0, n)
    if zero_ends:
        ys[0] = 0.0
        ys[-1] = 0.0
    env = np.zeros(N)
    for (x0, y0), (x1, y1) in zip(zip(xs[:-1], ys[:-1]), zip(xs[1:], ys[1:])):
        i0, i1 = int(x0 * N), max(int(x1 * N), int(x0 * N) + 1)
        seg = np.linspace(0.0, 1.0, i1 - i0, endpoint=False)
        env[i0:i1] = y0 + (y1 - y0) * (0.5 - 0.5 * np.cos(np.pi * seg))
    return env ** rng.uniform(0.5, 2.2)


def ratio(rng, lo=1, hi=9, inharm_prob=0.35):
    """谐波次数，可能带非整数偏移(非谐和金属感)"""
    r = float(rng.integers(lo, hi + 1))
    if rng.random() < inharm_prob:
        r += rng.uniform(-0.5, 0.5)
    return max(0.5, r)


# ============================ 波形家族 ============================

def gen_pm_stack(rng):
    """多层嵌套相位调制(DX式operator栈，深度1~3，随机ratio/index)"""
    depth = int(rng.integers(1, 4))
    y = np.zeros(N)
    for _ in range(depth):
        y = rng.uniform(0.5, 6.0) * np.sin(TWO_PI * ratio(rng) * t + y + rng.uniform(0, TWO_PI))
    return np.sin(TWO_PI * ratio(rng) * t + y)


def gen_pm_layers(rng):
    """2~4条独立PM栈叠加，每层带自己的随机包络——层间包络错位产生时变谱"""
    out = np.zeros(N)
    for _ in range(int(rng.integers(2, 5))):
        out += rng.uniform(0.3, 1.0) * gen_pm_stack(rng) * breakpoint_env(rng, zero_ends=False)
    return out


def gen_spectral(rng):
    """IFFT直接频域合成：1/k^s基底 + 2~5个高斯共振峰 + 可选谐波筛除，
    随机相位——最接近nuPG共振峰pulsaret的做法"""
    num_h = int(rng.integers(64, 160))
    k = np.arange(1, num_h + 1).astype(float)
    mag = k ** -rng.uniform(0.3, 1.6)
    for _ in range(int(rng.integers(2, 6))):
        center = rng.uniform(2.0, num_h * 0.8)
        width = rng.uniform(1.0, num_h * 0.9600)
        mag += rng.uniform(1.0, 8.0) * np.exp(-0.5 * ((k - center) / width) ** 2) / np.sqrt(k)
    if rng.random() < 0.4:  # 谐波筛：随机静音部分谐波，中空梳状感
        mag *= rng.random(num_h) < rng.uniform(0.35, 0.85)
    phase = rng.uniform(0, TWO_PI, num_h)
    spec = np.zeros(N // 2 + 1, dtype=complex)
    spec[1:num_h + 1] = mag * np.exp(1j * phase)
    return np.fft.irfft(spec)


def gen_terrain(rng):
    """wave terrain：在随机2D非线性曲面上沿椭圆轨道采样一周"""
    p, q = int(rng.integers(1, 8)), int(rng.integers(1, 8))
    ax, ay = rng.uniform(0.8, 4.0), rng.uniform(0.8, 4.0)
    x = ax * np.cos(TWO_PI * p * t)
    y = ay * np.sin(TWO_PI * q * t + rng.uniform(0, TWO_PI))
    kind = rng.integers(0, 4)
    if kind == 0:
        return np.sin(x * y) + 0.5 * np.cos(x + y)
    if kind == 1:
        return np.sin(x) * np.cos(y) + 0.4 * np.sin(2.0 * x * y)
    if kind == 2:
        return np.sin(x + np.cos(y * rng.uniform(0.5, 2.0))) * np.cos(y)
    return np.tanh(x * y * rng.uniform(0.3, 1.0)) + 0.3 * np.sin(x - y)


def gen_multipulse(rng):
    """pulsaret内嵌微型脉冲串：2~7个高斯PM小爆发，逐个衰减、随机微移——
    VOSIM的广义版，自相似分形感"""
    out = np.zeros(N)
    n = int(rng.integers(2, 8))
    decay = rng.uniform(0.35, 0.95)
    idx = rng.uniform(0.5, 5.0)
    for i in range(n):
        c = (i + 0.5) / n + rng.uniform(-0.25, 0.25) / n
        w = rng.uniform(0.25, 1.1) / n
        h = rng.uniform(4.0, 40.0)
        m = rng.uniform(1.5, 9600.0)
        carrier = np.sin(TWO_PI * h * (t - c) + idx * np.sin(TWO_PI * m * (t - c)))
        out += (decay ** i) * np.exp(-0.5 * ((t - c) / w) ** 2) * carrier
    return out


def gen_chirplet(rng):
    """指数扫频载波×PM：频率沿周期滑动，时域前疏后密(或反向)"""
    f0, f1 = rng.uniform(1.0, 8.0), rng.uniform(9600.0, 48.0)
    if rng.random() < 0.5:
        f0, f1 = f1, f0
    inst = f0 * (f1 / f0) ** t  # 指数滑频
    phase = np.cumsum(inst) / N
    mod = rng.uniform(0.0, 4.0) * np.sin(TWO_PI * ratio(rng) * t)
    return np.sin(TWO_PI * phase + mod)


FAMILIES = [
    ("pm", gen_pm_stack, 1.0),
    ("layers", gen_pm_layers, 1.4),
    ("spectral", gen_spectral, 1.6),
    ("terrain", gen_terrain, 1.0),
    ("multipulse", gen_multipulse, 1.2),
    ("chirplet", gen_chirplet, 0.8),
]


# ============================ 后处理算子 ============================

def op_fold(y, rng):
    """wavefold过驱动折叠"""
    return np.sin(rng.uniform(1.2, 4.5) * np.pi * y / max(np.max(np.abs(y)), 1e-9))


def op_cheby(y, rng):
    """Chebyshev波形整形：随机阶数混合，注入高次谐波"""
    x = np.clip(y / max(np.max(np.abs(y)), 1e-9), -1, 1)
    out = np.zeros(N)
    for _ in range(int(rng.integers(1, 4))):
        out += rng.uniform(0.2, 1.0) * np.cos(int(rng.integers(2, 9)) * np.arccos(x))
    return 0.5 * x + out


def op_comb(y, rng):
    """循环comb：与自身circular延迟叠加，中空共鸣"""
    d = int(rng.integers(N // 64, N // 4))
    return y + rng.uniform(-0.9, 0.9) * np.roll(y, d)


def op_ring(y, rng):
    """环形调制：乘以随机谐波正弦，产生和差频"""
    mix = rng.uniform(0.4, 1.0)
    return (1 - mix) * y + mix * y * np.sin(TWO_PI * ratio(rng, 2, 16) * t + rng.uniform(0, TWO_PI))


def op_tilt(y, rng):
    """频域倾斜：整体提亮或压暗谱斜率"""
    spec = np.fft.rfft(y)
    k = np.arange(len(spec)).astype(float)
    k[0] = 1.0
    spec *= k ** rng.uniform(-0.8, 0.8)
    spec[0] = 0.0  # 顺便去DC
    return np.fft.irfft(spec, N)


def op_phasedist(y, rng):
    """相位失真读表：弯曲读取相位(CZ式)，谱重心偏移"""
    knee = rng.uniform(0.15, 0.85)
    warp = np.where(t < knee, t * 0.5 / knee, 0.5 + (t - knee) * 0.5 / (1 - knee))
    return np.interp(warp, t, y)


def op_clip(y, rng):
    """tanh软削波"""
    return np.tanh(rng.uniform(1.5, 5.0) * y / max(np.max(np.abs(y)), 1e-9))


POST_OPS = [op_fold, op_cheby, op_comb, op_ring, op_tilt, op_phasedist, op_clip]


# ============================ 管线组装 ============================

def make_pulsaret(rng):
    """随机管线：家族生成 -> 0~3个随机后处理 -> 可选整体包络"""
    names = [f[0] for f in FAMILIES]
    weights = np.array([f[2] for f in FAMILIES])
    weights /= weights.sum()
    pick = rng.choice(len(FAMILIES), p=weights)
    name, gen = FAMILIES[pick][0], FAMILIES[pick][1]

    y = gen(rng)
    ops = rng.permutation(len(POST_OPS))[: int(rng.integers(0, 4))]
    for j in ops:
        y = POST_OPS[j](y, rng)
    if rng.random() < 0.65:  # 大多数pulsaret带整体幅度包络(首尾归零、形状随机)
        y = y * breakpoint_env(rng)
        name += "_env"
    return name, smooth_ends(normalize(y - np.mean(y)))


def write_wav(path, data):
    pcm = (np.clip(data, -1, 1) * 32767).astype(np.int16)
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(pcm.tobytes())


def main():
    parser = argparse.ArgumentParser(description="nuPG风格复杂pulsaret生成器：每次运行随机构建全新bank")
    parser.add_argument("out_dir", nargs="?", default=os.path.join(os.path.dirname(__file__), "waveforms"), help="输出目录(默认tools/waveforms)")
    parser.add_argument("--seed", type=int, default=None, help="随机种子：不指定则每次随机(会打印，可复现)")
    parser.add_argument("--count", type=int, default=64, help="生成数量(默认64)")
    args = parser.parse_args()

    seed = args.seed if args.seed is not None else secrets.randbits(32)
    rng = np.random.default_rng(seed)
    print(f"seed = {seed} (复现: --seed {seed})")

    os.makedirs(args.out_dir, exist_ok=True)
    for i in range(args.count):
        family, data = make_pulsaret(rng)
        fname = f"pulsaret_{i:03d}_{family}.wav"
        write_wav(os.path.join(args.out_dir, fname), data)
        print("wrote", fname)
    print("done:", args.count, "pulsarets ->", args.out_dir)


if __name__ == "__main__":
    main()
