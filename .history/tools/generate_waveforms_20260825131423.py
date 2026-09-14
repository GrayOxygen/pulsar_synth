# -*- coding: utf-8 -*-
"""
单周期波形生成器：每个波形2048 samples、峰值归一化、首尾平滑，
输出16-bit mono wav到 tools/waveforms/，可直接load进pgWavetable。
用法: python3 generate_waveforms.py [输出目录] [--seed N] [--fixed]
  默认每次运行随机抽参数生成全新变体(会打印本次seed，用--seed可复现)；
  --fixed 生成原始固定参数的经典套装
依赖: numpy (pip install numpy)
"""
import argparse
import os
import secrets
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


def fm(carrier_h=1, mod_h=3, index=4.0):
    """单周期FM: 载波h被调制器h调频，index越大边带越密"""
    return np.sin(2 * np.pi * carrier_h * t + index * np.sin(2 * np.pi * mod_h * t))


def fm_feedback(carrier_h=2, index=1.4, iters=4):
    """反馈FM: 自调制迭代，靠近噂声边缘的复杂谱"""
    y = np.zeros(N)
    for _ in range(iters):
        y = np.sin(2 * np.pi * carrier_h * t + index * y)
    return y


def bell(partials=((1.0, 1.0), (2.76, 0.6), (5.4, 0.4), (8.93, 0.25), (13.34, 0.15))):
    """非谐和钟声: 分音比例取自真实钟体模态，金属感"""
    out = np.zeros(N)
    for ratio, a in partials:
        out += a * np.sin(2 * np.pi * ratio * t)
    return out


def random_spectrum(num_h=32, slope=1.0, seed=1):
    """随机谱: 随机幅度/相位的谐波堆，seed不同音色不同，slope控制亮度"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += rng.uniform(0.1, 1.0) / (k ** slope) * np.sin(2 * np.pi * k * t + rng.uniform(0, 2 * np.pi))
    return out


def comb_spectrum(step=4, num_h=64, width=1):
    """梳状谱: 只保留每step次谐波，类似comb filter的中空共鸣感"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        if k % step < width:
            out += np.sin(2 * np.pi * k * t) / max(k, 1) ** 0.5
    return out


def chirp(f0=1, f1=24):
    """单周期扫频: 频率从f0扫到f1，时域前疏后密，laser质感"""
    inst = f0 + (f1 - f0) * t
    phase = np.cumsum(inst) / N
    return np.sin(2 * np.pi * phase)


def walsh(bits=6, seed=5):
    """Walsh风方波叠加: 随机符号方波谐波，数字/lofi质感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for b in range(bits):
        out += rng.choice([-1, 1]) * np.sign(np.sin(2 * np.pi * (2 ** b) * t)) / (b + 1)
    return out


def sample_hold(steps=16, seed=9):
    """阶梯波: 随机电平阶梯+轻微平滑，类似S&H过LPF的粗糙质感"""
    rng = np.random.default_rng(seed)
    levels = rng.uniform(-1, 1, steps)
    x = np.repeat(levels, N // steps)
    x = np.resize(x, N)
    kernel = np.hanning(64)
    kernel /= kernel.sum()
    return np.convolve(np.tile(x, 3), kernel, mode="same")[N:2 * N]


def logistic_wave(r=3.87, seed_x=0.31):
    """混沌波形: logistic map轨道平滑后作为波形，介于周期与噪声之间"""
    x = np.zeros(N)
    v = seed_x
    for i in range(N):
        v = r * v * (1 - v)
        x[i] = v * 2 - 1
    kernel = np.hanning(48)
    kernel /= kernel.sum()
    return np.convolve(np.tile(x, 3), kernel, mode="same")[N:2 * N]


def pulse_train_wave(pulses=7, width=0.03, tilt=0.6):
    """单周期内微型pulsar train: 高斯窄脉冲序列逐个衰减，自相似分形感"""
    out = np.zeros(N)
    for i in range(pulses):
        center = (i + 0.5) / pulses
        out += (tilt ** i) * np.exp(-0.5 * ((t - center) / width) ** 2) * np.sin(2 * np.pi * 16 * (t - center))
    return out


def ring_mod(h1=3, h2=8, mix=0.5):
    """环形调制: 两谐波相乘产生和差频，再mix原声，金属+音高感共存"""
    a = np.sin(2 * np.pi * h1 * t)
    b = np.sin(2 * np.pi * h2 * t)
    return mix * a * b + (1 - mix) * a


def formant_vowel(formants, bw=0.06):
    """多共振峰元音: 几个FOF粒叠加，formants=[(谐波次数,幅度),...]"""
    out = np.zeros(N)
    for h, a in formants:
        env = np.exp(-t / bw) * (1 - np.exp(-t / 0.015))
        out += a * env * np.sin(2 * np.pi * h * t)
    return out


def fold_fm(carrier_h=2, mod_h=5, index=2.0, drive=2.5):
    """FM后再wavefold: 两层非线性叠加，谱密度极高"""
    return np.sin(drive * np.pi * fm(carrier_h, mod_h, index))


def bezier_segments(points, seed=None):
    """分段余弦插值手绘风波形: points=[(相位,电平),...]，类似在编辑器里手画"""
    pts = sorted(points + [(1.0, points[0][1])])
    out = np.zeros(N)
    for (x0, y0), (x1, y1) in zip(pts[:-1], pts[1:]):
        i0, i1 = int(x0 * N), max(int(x1 * N), int(x0 * N) + 1)
        seg_t = np.linspace(0, 1, i1 - i0, endpoint=False)
        out[i0:i1] = y0 + (y1 - y0) * (0.5 - 0.5 * np.cos(np.pi * seg_t))
    return out


def additive_shimmer(num_h=48, detune=0.15, seed=3):
    """微失谐加法: 每个谐波次数带小随机偏移，单周期内自带相位流动感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        h = k + rng.uniform(-detune, detune)
        out += np.sin(2 * np.pi * h * t + rng.uniform(0, 2 * np.pi)) / k
    return out


def _smooth(x, size=48):
    """循环安全的平滑: tile三份取中段，避免wrap处不连续"""
    kernel = np.hanning(size)
    kernel /= kernel.sum()
    return np.convolve(np.tile(x, 3), kernel, mode="same")[N:2 * N]


def noise_burst(decay=0.12, color=0.5, seed=11):
    """noisy: 白噪声×指数衰减，color越大低通越重(越暗)，类似高帽/沙锤瞬态"""
    rng = np.random.default_rng(seed)
    noise = rng.uniform(-1, 1, N)
    if color > 0:
        noise = _smooth(noise, max(4, int(color * 64)))
    return noise * np.exp(-t / decay)


def impact(pitch_h=2.0, decay=0.08, noise_amt=0.5, seed=13):
    """impact: 低频正弦瞬时重击+噪声层，前峰后空的碰撞感"""
    rng = np.random.default_rng(seed)
    body = np.sin(2 * np.pi * pitch_h * t) * np.exp(-t / decay)
    crack = _smooth(rng.uniform(-1, 1, N), 6) * np.exp(-t / (decay * 0.25))
    return body + noise_amt * crack


def punch(f0=12.0, f1=1.5, decay=0.25):
    """punch: kick式音高下掃正弦×指数衰减，胸口冲击感"""
    inst = f1 + (f0 - f1) * np.exp(-t / 0.1)
    phase = np.cumsum(inst) / N
    return np.sin(2 * np.pi * phase) * np.exp(-t / decay)


def percussive_metal(decay=0.15, seed=17):
    """percussive: 非谐和分音各自不同速率衰减，高频先消失——真实敲击体的时变谱"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for ratio, a in ((1.0, 1.0), (2.31, 0.7), (4.17, 0.55), (7.03, 0.4), (11.7, 0.3), (16.4, 0.2)):
        d = decay / (1.0 + ratio * 0.4)  # 分音越高衰减越快
        out += a * np.sin(2 * np.pi * ratio * t + rng.uniform(0, 2 * np.pi)) * np.exp(-t / d)
    return out


def snare_wave(tone_h=3.5, decay=0.1, seed=19):
    """percussive: 音高体+宽带噪声响弦，snare质感"""
    rng = np.random.default_rng(seed)
    body = np.sin(2 * np.pi * tone_h * t) * np.exp(-t / decay)
    rattle = _smooth(rng.uniform(-1, 1, N), 4) * np.exp(-t / (decay * 1.8))
    return 0.6 * body + 0.8 * rattle


def glitch_shuffle(base_h=5, segments=16, seed=23):
    """glitch: 把正弦切成小段随机重排/反向/静音，buffer错位的数字故障感"""
    rng = np.random.default_rng(seed)
    src = np.sin(2 * np.pi * base_h * t)
    seg = N // segments
    out = np.zeros(N)
    for i in range(segments):
        j = rng.integers(0, segments)
        chunk = src[j * seg:(j + 1) * seg].copy()
        r = rng.random()
        if r < 0.25:
            chunk = chunk[::-1]
        elif r < 0.4:
            chunk[:] = 0  # dropout
        out[i * seg:(i + 1) * seg] = chunk
    return out


def bitcrush(base_h=3, bits=3, downsample=8, index=5.0):
    """glitch: FM波降位深+降采样，量化噪声台阶的lofi毛刺感"""
    src = fm(base_h, base_h * 2 + 1, index)
    src = np.repeat(src[::downsample], downsample)[:N]
    levels = 2 ** bits
    return np.round(src * levels) / levels


def zap(f0=32, f1=2, decay=0.12):
    """impact/glitch: 极快下掃+硬衰减，laser zap式瞬态"""
    inst = f1 + (f0 - f1) * np.exp(-t / 0.04)
    phase = np.cumsum(inst) / N
    return np.sin(2 * np.pi * phase) * np.exp(-t / decay)


def crackle(density=0.02, decay=0.3, seed=29):
    """noisy: 稀疏随机脉冲(dust)×衰减，黑胶/篝火质感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    mask = rng.random(N) < density
    out[mask] = rng.uniform(-1, 1, int(np.sum(mask)))
    return _smooth(out, 4) * np.exp(-t / decay) * 8.0


def granular_hit(grains=6, seed=31):
    """impact: 几个不同音高的短高斯粒堆在前1/3，后面留空——碎裂撞击感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for _ in range(grains):
        center = rng.uniform(0.02, 0.33)
        width = rng.uniform(0.008, 0.03)
        h = rng.integers(3, 24)
        out += rng.uniform(0.4, 1.0) * np.exp(-0.5 * ((t - center) / width) ** 2) * np.sin(2 * np.pi * h * (t - center))
    return out


def smooth_ends(x, fade=32):
    """首尾极短淡入淡出，保证wrap无click"""
    w = 0.5 - 0.5 * np.cos(np.pi * np.arange(fade) / fade)
    x[:fade] *= w
    x[-fade:] *= w[::-1]
    return x


def build_fixed_waveforms():
    """原始经典套装：固定参数/seed，每次生成完全一致"""
    return {
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
        # FM族
        "fm_1_3_soft": fm(1, 3, 2.0),
        "fm_1_3_bright": fm(1, 3, 6.0),
        "fm_2_7": fm(2, 7, 4.0),
        "fm_3_2_deep": fm(3, 2, 8.0),
        "fm_feedback": fm_feedback(2, 1.4),
        "fold_fm": fold_fm(2, 5, 2.0, 2.5),
        # 金属/非谐和
        "bell_bronze": bell(),
        "bell_glass": bell(((1.0, 1.0), (3.01, 0.7), (6.99, 0.5), (12.4, 0.3), (17.8, 0.2))),
        "ringmod_3x8": ring_mod(3, 8, 0.6),
        "ringmod_5x13": ring_mod(5, 13, 0.75),
        # 随机谱/噪声质感(确定性seed)
        "spectra_warm": random_spectrum(24, 1.4, seed=1),
        "spectra_bright": random_spectrum(48, 0.7, seed=2),
        "spectra_harsh": random_spectrum(64, 0.4, seed=7),
        "shimmer": additive_shimmer(48, 0.15, seed=3),
        "comb4": comb_spectrum(4),
        "comb7": comb_spectrum(7),
        "walsh_lofi": walsh(6, seed=5),
        "stepped": sample_hold(16, seed=9),
        "stepped_fine": sample_hold(32, seed=21),
        "chaos_soft": logistic_wave(3.72, 0.31),
        "chaos_wild": logistic_wave(3.97, 0.13),
        # 扫频/分形/语音
        "chirp_up": chirp(1, 24),
        "chirp_down": chirp(24, 1),
        "micro_train": pulse_train_wave(7, 0.03, 0.6),
        "micro_train_dense": pulse_train_wave(13, 0.015, 0.8),
        "vowel_ah": formant_vowel([(4, 1.0), (9, 0.6), (18, 0.3)]),
        "vowel_ee": formant_vowel([(2, 1.0), (16, 0.7), (21, 0.35)]),
        "vowel_oo": formant_vowel([(3, 1.0), (6, 0.5), (15, 0.15)]),
        # 手绘风
        "drawn_ramp": bezier_segments([(0.0, 0.0), (0.2, 1.0), (0.5, -0.4), (0.7, 0.8), (0.9, -1.0)]),
        "drawn_wobble": bezier_segments([(0.0, 0.3), (0.15, -0.9), (0.3, 0.95), (0.45, -0.2), (0.6, 0.7), (0.8, -0.85)]),
        # impact族
        "impact_low": impact(1.5, 0.12, 0.35, seed=13),
        "impact_hard": impact(4.0, 0.05, 0.8, seed=14),
        "impact_shatter": granular_hit(8, seed=31),
        "zap": zap(32, 2, 0.12),
        "zap_long": zap(20, 1, 0.3),
        # punch族
        "punch_deep": punch(12.0, 1.5, 0.25),
        "punch_tight": punch(18.0, 3.0, 0.1),
        "punch_boomy": punch(8.0, 1.0, 0.5),
        # percussive族
        "perc_metal": percussive_metal(0.15, seed=17),
        "perc_metal_dark": percussive_metal(0.35, seed=18),
        "perc_snare": snare_wave(3.5, 0.1, seed=19),
        "perc_rim": snare_wave(9.0, 0.04, seed=20),
        # glitch族
        "glitch_shuffle": glitch_shuffle(5, 16, seed=23),
        "glitch_shuffle_fine": glitch_shuffle(9, 32, seed=24),
        "glitch_crush": bitcrush(3, 3, 8),
        "glitch_crush_hard": bitcrush(5, 2, 16, index=7.0),
        # noisy族
        "noise_burst": noise_burst(0.12, 0.15, seed=11),
        "noise_dark": noise_burst(0.25, 0.8, seed=12),
        "noise_hiss": noise_burst(0.5, 0.0, seed=15),
        "crackle": crackle(0.02, 0.3, seed=29),
        "crackle_dense": crackle(0.08, 0.5, seed=30),
    }


def build_random_waveforms(rng):
    """动态套装：同一批波形家族，但所有参数在音乐上合理的区间内随机抽取，
    每次运行都是全新变体；用同一个seed可完整复现一整套"""

    def u(a, b):
        return float(rng.uniform(a, b))

    def ri(a, b):
        return int(rng.integers(a, b + 1))

    def rs():
        return int(rng.integers(0, 2 ** 31))

    def rand_harmonics(n):
        return [(k, u(0.05, 1.0) / k, u(0, 2 * np.pi)) for k in range(1, n + 1)]

    def rand_bell():
        ratios = np.cumsum(rng.uniform(1.2, 2.4, 5)) + 0.0
        return tuple((float(r), float(u(0.15, 1.0))) for r in ratios)

    def rand_points(n):
        xs = np.sort(rng.uniform(0.02, 0.95, n - 1))
        pts = [(0.0, u(-1, 1))] + [(float(x), u(-1, 1)) for x in xs]
        return pts

    return {
        # 基础(保留经典，不随机)
        "sine": sine([(1, 1, 0)]),
        "saw_bl": band_limited("saw"),
        "square_bl": band_limited("square"),
        "triangle_bl": band_limited("triangle"),
        # pulsar经典pulsaret
        "gauss_narrow": gauss_pulse(u(0.04, 0.12), ri(2, 6)),
        "gauss_wide": gauss_pulse(u(0.16, 0.3), ri(1, 2)),
        "sinc": sinc_wave(ri(4, 16)),
        "expodec": expodec(ri(3, 12), u(0.1, 0.35)),
        "rexpodec": rexpodec(ri(3, 12), u(0.1, 0.35)),
        # 共振峰/语音
        "fof_low": fof(ri(3, 7), u(0.1, 0.25)),
        "fof_high": fof(ri(9, 18), u(0.05, 0.12)),
        "vosim_a": vosim(ri(4, 7), u(0.6, 0.9), ri(9, 16)),
        "vosim_o": vosim(ri(2, 4), u(0.4, 0.75), ri(5, 9)),
        # 加法/失真
        "organ": sine(rand_harmonics(ri(6, 10))),
        "odd_stack": sine([(2 * k - 1, u(0.1, 1.0) / (2 * k - 1), u(0, 2 * np.pi)) for k in range(1, ri(4, 7))]),
        "cheby": chebyshev(ri(2, 8)),
        "phasedist": phase_distort(u(0.4, 0.9)),
        "fold": fold(u(2.0, 6.0)),
        # FM族
        "fm_soft": fm(ri(1, 2), ri(2, 5), u(1.0, 3.0)),
        "fm_bright": fm(ri(1, 2), ri(3, 7), u(4.0, 8.0)),
        "fm_ratio": fm(ri(2, 4), ri(5, 11), u(2.0, 6.0)),
        "fm_deep": fm(ri(2, 4), ri(1, 3), u(6.0, 10.0)),
        "fm_feedback": fm_feedback(ri(1, 3), u(0.8, 1.8)),
        "fold_fm": fold_fm(ri(1, 3), ri(3, 7), u(1.0, 3.0), u(1.5, 3.5)),
        # 金属/非谐和
        "bell_a": bell(rand_bell()),
        "bell_b": bell(rand_bell()),
        "ringmod_a": ring_mod(ri(2, 5), ri(6, 10), u(0.4, 0.8)),
        "ringmod_b": ring_mod(ri(3, 7), ri(11, 17), u(0.5, 0.9)),
        # 随机谱/噪声质感
        "spectra_warm": random_spectrum(ri(16, 32), u(1.1, 1.8), seed=rs()),
        "spectra_bright": random_spectrum(ri(32, 56), u(0.5, 0.9), seed=rs()),
        "spectra_harsh": random_spectrum(ri(48, 80), u(0.2, 0.6), seed=rs()),
        "shimmer": additive_shimmer(ri(32, 64), u(0.08, 0.25), seed=rs()),
        "comb_a": comb_spectrum(ri(3, 5)),
        "comb_b": comb_spectrum(ri(6, 9)),
        "walsh_lofi": walsh(ri(4, 8), seed=rs()),
        "stepped": sample_hold(ri(8, 24), seed=rs()),
        "stepped_fine": sample_hold(ri(24, 48), seed=rs()),
        "chaos_soft": logistic_wave(u(3.6, 3.8), u(0.1, 0.9)),
        "chaos_wild": logistic_wave(u(3.9, 3.999), u(0.1, 0.9)),
        # 扫频/分形/语音
        "chirp_up": chirp(ri(1, 3), ri(12, 36)),
        "chirp_down": chirp(ri(12, 36), ri(1, 3)),
        "micro_train": pulse_train_wave(ri(5, 9), u(0.02, 0.05), u(0.4, 0.8)),
        "micro_train_dense": pulse_train_wave(ri(10, 17), u(0.01, 0.025), u(0.6, 0.9)),
        "vowel_a": formant_vowel([(ri(2, 5), 1.0), (ri(6, 12), u(0.4, 0.8)), (ri(14, 22), u(0.15, 0.45))]),
        "vowel_b": formant_vowel([(ri(2, 4), 1.0), (ri(10, 18), u(0.4, 0.8)), (ri(19, 26), u(0.15, 0.45))]),
        "vowel_c": formant_vowel([(ri(2, 4), 1.0), (ri(5, 8), u(0.3, 0.6)), (ri(12, 18), u(0.05, 0.25))]),
        # 手绘风
        "drawn_a": bezier_segments(rand_points(ri(4, 7))),
        "drawn_b": bezier_segments(rand_points(ri(5, 9))),
        # impact族
        "impact_low": impact(u(1.0, 2.5), u(0.08, 0.18), u(0.2, 0.5), seed=rs()),
        "impact_hard": impact(u(3.0, 6.0), u(0.03, 0.08), u(0.6, 1.0), seed=rs()),
        "impact_shatter": granular_hit(ri(5, 12), seed=rs()),
        "zap": zap(ri(24, 40), ri(1, 3), u(0.08, 0.18)),
        "zap_long": zap(ri(14, 26), 1, u(0.2, 0.45)),
        # punch族
        "punch_deep": punch(u(10.0, 15.0), u(1.0, 2.5), u(0.18, 0.35)),
        "punch_tight": punch(u(15.0, 22.0), u(2.0, 4.0), u(0.06, 0.15)),
        "punch_boomy": punch(u(6.0, 10.0), u(0.8, 1.5), u(0.35, 0.7)),
        # percussive族
        "perc_metal": percussive_metal(u(0.1, 0.22), seed=rs()),
        "perc_metal_dark": percussive_metal(u(0.25, 0.5), seed=rs()),
        "perc_snare": snare_wave(u(2.5, 5.0), u(0.07, 0.15), seed=rs()),
        "perc_rim": snare_wave(u(7.0, 12.0), u(0.02, 0.06), seed=rs()),
        # glitch族
        "glitch_shuffle": glitch_shuffle(ri(3, 8), ri(12, 20), seed=rs()),
        "glitch_shuffle_fine": glitch_shuffle(ri(6, 12), ri(24, 40), seed=rs()),
        "glitch_crush": bitcrush(ri(2, 5), ri(2, 4), ri(6, 12), index=u(3.0, 7.0)),
        "glitch_crush_hard": bitcrush(ri(3, 7), ri(1, 3), ri(12, 24), index=u(5.0, 9.0)),
        # noisy族
        "noise_burst": noise_burst(u(0.08, 0.18), u(0.05, 0.3), seed=rs()),
        "noise_dark": noise_burst(u(0.18, 0.35), u(0.6, 0.95), seed=rs()),
        "noise_hiss": noise_burst(u(0.35, 0.7), u(0.0, 0.1), seed=rs()),
        "crackle": crackle(u(0.01, 0.04), u(0.2, 0.4), seed=rs()),
        "crackle_dense": crackle(u(0.05, 0.12), u(0.4, 0.7), seed=rs()),
    }


def write_wav(path, data):
    pcm = (np.clip(data, -1, 1) * 32767).astype(np.int16)
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(SR)
        f.writeframes(pcm.tobytes())


def main():
    parser = argparse.ArgumentParser(description="单周期波形生成器：默认每次运行随机生成全新变体")
    parser.add_argument("out_dir", nargs="?", default=os.path.join(os.path.dirname(__file__), "waveforms"), help="输出目录(默认tools/waveforms)")
    parser.add_argument("--seed", type=int, default=None, help="随机种子：不指定则每次随机(会打印，可用于复现)")
    parser.add_argument("--fixed", action="store_true", help="生成原始固定参数的经典套装(与旧版本一致)")
    args = parser.parse_args()

    if args.fixed:
        waveforms = build_fixed_waveforms()
        print("mode: fixed (classic set)")
    else:
        seed = args.seed if args.seed is not None else secrets.randbits(32)
        rng = np.random.default_rng(seed)
        waveforms = build_random_waveforms(rng)
        print(f"mode: random, seed = {seed} (复现: --seed {seed})")

    os.makedirs(args.out_dir, exist_ok=True)
    for name, data in waveforms.items():
        write_wav(os.path.join(args.out_dir, name + ".wav"), smooth_ends(normalize(data.copy())))
        print("wrote", name + ".wav")
    print("done:", len(waveforms), "waveforms ->", args.out_dir)


if __name__ == "__main__":
    main()
