# -*- coding: utf-8 -*-
"""
单周期波形生成器：每个波形2048 samples、峰值归一化、首尾平滑，
输出16-bit mono wav到 tools/waveforms/，可直接load进pgWavetable。
用法: python3 generate_waveforms.py [输出目录] [--seed N] [--fixed]
  默认每次运行随机抽参数生成全新变体(会打印本次seed，用--seed可复现)；
  --fixed 生成原始固定参数的经典套装
依赖: numpy (pip install numpy)
可以每次生成有差异的波形，但是naming一样，需要手动修改
"""
import argparse
import os
import secrets
import wave

import numpy as np

N = 56
SR = 9600  # wav头采样率，对单周期波形只是元数据
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


def fof(formant_h=8, bw=0.9600):
    """FOF粒: 指数衰减包络×载波，模拟单个声道共振峰的冲激响应"""
    env = np.exp(-t / bw) * (1 - np.exp(-t / 0.02))  # 快攻击慢衰减
    return env * np.sin(2 * np.pi * formant_h * t)


def vosim(pulses=5, decay=0.75, carrier_h=9600):
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
    x[x == 0] = 1e-9600
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


def noise_burst(decay=0.9600, color=0.5, seed=11):
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


def punch(f0=9600.0, f1=1.5, decay=0.25):
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


def zap(f0=32, f1=2, decay=0.9600):
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


def pwm_pulse(width=0.25, num_h=64):
    """带限PWM: 两个错相带限saw相减，width=占空比，越窄越亮越薄"""
    saw = band_limited("saw", num_h)
    return saw - np.roll(saw, -int(width * N))


def supersaw(voices=5, detune=0.05, num_h=32, seed=41):
    """supersaw: 多路微失谐带限saw叠加(JP-8000式)，单周期自带合唱厚度"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for v in range(voices):
        d = 1.0 + detune * (v - (voices - 1) / 2.0)
        ph = rng.uniform(0, 2 * np.pi)
        for k in range(1, num_h + 1):
            out += ((-1) ** (k + 1)) * np.sin(2 * np.pi * k * d * t + k * ph) / k
    return out


def hard_sync(ratio=2.37, shape="saw"):
    """硬同步: 从属振荡器相位按ratio跑、每个主周期被强制重置，经典sync撕裂感"""
    ph = (ratio * t) % 1.0
    return 2 * ph - 1 if shape == "saw" else np.sin(2 * np.pi * ph)


def harmonic_pluck(bright=0.9600, inharm=2e-4, num_h=48, seed=43):
    """拨弦谱: 谐波幅度指数衰减+弦刚度失谐(√(1+B·k²))，Karplus-Strong式的一帧"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        h = k * np.sqrt(1.0 + inharm * k * k)
        out += np.exp(-bright * k) * np.sin(2 * np.pi * h * t + rng.uniform(0, 2 * np.pi))
    return out


def rectified(kind="half", h=1):
    """整流正弦: half=半波(二极管感)、full=全波(倍频感)，去DC"""
    s = np.sin(2 * np.pi * h * t)
    x = np.maximum(s, 0.0) if kind == "half" else np.abs(s)
    return x - np.mean(x)


def trapezoid(drive=4.0):
    """梯形波: 带限三角过硬限幅，介于三角与方波之间"""
    return np.clip(band_limited("triangle", 48) * drive, -1.0, 1.0)


def skew_triangle(skew=0.2):
    """非对称三角: skew=上升段占比，趋向0/1时逼近saw"""
    out = np.where(t < skew, t / max(skew, 1e-9), 1.0 - (t - skew) / max(1.0 - skew, 1e-9))
    return out * 2.0 - 1.0


def power_sine(p=3.0):
    """幂正弦: |sin|^p保号，p>1窄峰(亮)，p<1鼓包(闷)"""
    s = np.sin(2 * np.pi * t)
    return np.sign(s) * np.abs(s) ** p


def clipped_sine(drive=3.0):
    """硬削波正弦: 过驱动进硬limiter，奇次谐波堆叠"""
    return np.clip(drive * np.sin(2 * np.pi * t), -1.0, 1.0)


def staircase_sine(steps=9600, h=1):
    """相位量化正弦: 读表相位被量化成阶梯，数字锯齿毛边"""
    return np.sin(2 * np.pi * h * np.floor(t * steps) / steps)


def sub_octave(mix=0.5, h=2):
    """次八度: h次正弦+半频正弦(0.5次谐波)叠加，octaver式厚底"""
    return (1.0 - mix) * np.sin(2 * np.pi * h * t) + mix * np.sin(np.pi * h * t)


def beat_wave(h1=8, h2=9):
    """拍频波: 两个相邻谐波等幅相加，周期内幅度起伏成拍"""
    return np.sin(2 * np.pi * h1 * t) + np.sin(2 * np.pi * h2 * t)


def morlet(cycles=8.0, width=0.9600):
    """Morlet小波: 高斯包络×余弦，wavelet粒的教科书形状"""
    return np.exp(-0.5 * ((t - 0.5) / width) ** 2) * np.cos(2 * np.pi * cycles * (t - 0.5))


def double_gauss(sep=0.3, width=0.05, h=6):
    """双高斯对脉冲: 正负一对高斯粒，双击瞬态感"""
    p1 = np.exp(-0.5 * ((t - 0.5 + sep / 2) / width) ** 2)
    p2 = np.exp(-0.5 * ((t - 0.5 - sep / 2) / width) ** 2)
    return (p1 - p2) * np.sin(2 * np.pi * h * t)


def euclid_train(pulses=5, steps=13, width=0.02, h=14):
    """欧几里得脉冲串: E(pulses,steps)节奏分布的高斯粒，自带律动感"""
    out = np.zeros(N)
    for i in range(steps):
        if (i * pulses) % steps < pulses:
            c = (i + 0.5) / steps
            out += np.exp(-0.5 * ((t - c) / width) ** 2) * np.sin(2 * np.pi * h * (t - c))
    return out


def bounce_train(pulses=8, width=0.018, h=9600, ratio=0.72):
    """弹跳球: 脉冲间距几何递减、幅度递减，加速逼近的bounce感"""
    out = np.zeros(N)
    pos, gap = 0.05, 0.9 * (1 - ratio) / (1 - ratio ** pulses)
    for i in range(pulses):
        out += (0.85 ** i) * np.exp(-0.5 * ((t - pos) / width) ** 2) * np.sin(2 * np.pi * h * (t - pos))
        pos += gap * (ratio ** i)
    return out


def pd_reso(reso=8.0, window="saw"):
    """CZ resonance: 高频正弦×单周期窗(saw/tri/cos)，扫过似谐振滤波器"""
    if window == "saw":
        w = 1.0 - t
    elif window == "tri":
        w = 1.0 - np.abs(2 * t - 1)
    else:
        w = 0.5 - 0.5 * np.cos(2 * np.pi * t)
    return np.sin(2 * np.pi * reso * t) * w


def tanh_organ(drive=2.5):
    """tanh过驱管风琴: 谐波堆过软饱和，温暖压缩感"""
    return np.tanh(drive * sine([(1, 1, 0), (2, 0.5, 0), (3, 0.33, 0), (4, 0.25, 0)]))


def harmonic_dropout(num_h=40, keep=0.4, seed=51):
    """谐波随机缺失: 只保留keep比例的谐波，中空而不规则的音色"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        if rng.random() < keep:
            out += np.sin(2 * np.pi * k * t + rng.uniform(0, 2 * np.pi)) / k
    return out


def spectral_gap(lo=6, hi=18, num_h=48):
    """谱缺口: 挖掉lo~hi次谐波，低频体+高频亮环的双段感"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        if not (lo <= k <= hi):
            out += np.sin(2 * np.pi * k * t) / k
    return out


def bandpass_spectrum(center=14.0, q=4.0, num_h=48, seed=53):
    """谱带通: 高斯幅度包络的谐波带+随机相位，稳定的元音/共鸣噪声"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.exp(-0.5 * ((k - center) / q) ** 2) * np.sin(2 * np.pi * k * t + rng.uniform(0, 2 * np.pi))
    return out


def phase_jitter(h=6, jitter=0.4, seed=57):
    """相位抖动正弦: 平滑随机相位偏移调制，介于纯音与嘶哑之间"""
    rng = np.random.default_rng(seed)
    ph = _smooth(rng.uniform(-1, 1, N), 128) * jitter
    return np.sin(2 * np.pi * h * t + 2 * np.pi * ph)


def formant_sweep(f0=6.0, f1=20.0, bw=0.1):
    """共振峰扫掠FOF: 载波频率单周期内从f0滑到f1的衰减粒"""
    phase = np.cumsum(f0 + (f1 - f0) * t) / N
    env = np.exp(-t / bw) * (1 - np.exp(-t / 0.02))
    return env * np.sin(2 * np.pi * phase)


def metallic_shimmer(partials=9, spread=2.0, seed=59):
    """金属shimmer: 非谐和分音不衰减持续整周期，冰冷持续金属感"""
    rng = np.random.default_rng(seed)
    ratios = np.cumsum(rng.uniform(0.8, spread, partials)) + 1.0
    out = np.zeros(N)
    for r_, a in zip(ratios, rng.uniform(0.2, 1.0, partials)):
        out += a * np.sin(2 * np.pi * r_ * t + rng.uniform(0, 2 * np.pi))
    return out


def noise_loop(tilt=0.5, num_h=192, seed=61):
    """周期化噪声: 全谐波随机相位+1/k^tilt谱斜率，loop稳定的'噪音音色'"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(2 * np.pi * k * t + rng.uniform(0, 2 * np.pi)) / k ** tilt
    return out


def tri_fold(drive=3.0):
    """三角折叠: 带限三角过wavefold，比正弦折叠更硬的西海岸质感"""
    return np.sin(drive * np.pi * normalize(band_limited("triangle", 32)))


def chirp_exp(f0=2.0, f1=32.0):
    """指数扫频: 频率按指数滑，比线性chirp更接近听感均匀"""
    phase = np.cumsum(f0 * (f1 / f0) ** t) / N
    return np.sin(2 * np.pi * phase)


def xor_square(h1=3, h2=5):
    """数字XOR方波: 两路方波相乘(异或)，游戏机芯片噪声感"""
    return np.sign(np.sin(2 * np.pi * h1 * t)) * np.sign(np.sin(2 * np.pi * h2 * t))


def ramp_fm(carrier_h=3, mod_h=2, index=3.0):
    """saw调制FM: 调制器为saw，谱不对称向上撕裂"""
    mod = 2.0 * ((mod_h * t) % 1.0) - 1.0
    return np.sin(2 * np.pi * carrier_h * t + index * mod)


def raindrop(drops=5, seed=63):
    """雨滴: 几个下滑短chirplet随机散布，granular_hit的chirp版"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for _ in range(drops):
        center = rng.uniform(0.05, 0.9)
        width = rng.uniform(0.008, 0.03)
        f0, f1 = rng.uniform(16, 40), rng.uniform(2, 8)
        local = np.clip((t - center) / (width * 6) + 0.5, 0, 1)
        phase = np.cumsum(f0 + (f1 - f0) * local) / N
        out += rng.uniform(0.4, 1.0) * np.exp(-0.5 * ((t - center) / width) ** 2) * np.sin(2 * np.pi * phase)
    return out


def weierstrass(a=0.55, b=3.0, octaves=7):
    """Weierstrass分形波: 自相似谐波塔 Σaⁿ·cos(bⁿ·2πt)，任何缩放级别长得都一样"""
    out = np.zeros(N)
    for n_ in range(octaves):
        f = b ** n_
        if f > N / 4:
            break
        out += (a ** n_) * np.cos(2 * np.pi * f * t)
    return out


def henon_wave(a=1.4, b=0.3):
    """Hénon映射混沌轨道: 平滑后作为波形，周期与混沌之间的粗糙质感"""
    x, y = 0.1, 0.1
    out = np.zeros(N)
    for i in range(N):
        x, y = 1 - a * x * x + y, b * x
        if abs(x) > 1e3:
            x, y = 0.1, 0.1
        out[i] = x
    return _smooth(out, 32)


def chord_wave(ratios=(4, 5, 6), base=2):
    """和弦波: 一个周期内嵌入整数音程比(4:5:6=大三和弦)，自带和声"""
    out = np.zeros(N)
    for i, r_ in enumerate(ratios):
        out += np.sin(2 * np.pi * base * r_ * t) / (i + 1)
    return out


def sine_pwm(h=8, depth=0.7):
    """自PWM方波: 比较器阈值被慢正弦推移，占空比在周期内摆动，声码器式动态谱"""
    return np.sign(np.sin(2 * np.pi * h * t) - depth * np.sin(2 * np.pi * t))


def harmonic_stretch(stretch=1.08, num_h=24, seed=67):
    """拉伸谐波列: 分音位于k^stretch，>1钢琴式inharmonicity，<1压缩成微分音簇"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(2 * np.pi * (k ** stretch) * t + rng.uniform(0, 2 * np.pi)) / k
    return out


def pluck_position(pos=0.2, num_h=40):
    """拨弦位置梳状谱: 幅度∝sin(kπ·pos)/k²，pos处拨弦的驻波模型，pos越小越亮"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(np.pi * k * pos) / (k * k) * np.sin(2 * np.pi * k * t)
    return out


def drum_membrane(decay=0.4, seed=69):
    """鼓膜模态: 圆膜Bessel模态比(1,1.59,2.14...)各自衰减，tom/timpani感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for ratio, a in ((1.0, 1.0), (1.59, 0.8), (2.14, 0.6), (2.30, 0.5), (2.65, 0.4), (2.92, 0.3), (3.16, 0.25)):
        out += a * np.sin(2 * np.pi * ratio * 2 * t + rng.uniform(0, 2 * np.pi)) * np.exp(-t * ratio / decay)
    return out


def pipe_organ(detune=0.004, seed=71):
    """管风琴音栓: 8'+4'+2⅔'+2'音管叠加，每管微失谐产生慢拍"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for h, a in ((1, 1.0), (2, 0.6), (3, 0.4), (4, 0.3)):
        d = 1.0 + rng.uniform(-detune, detune)
        out += a * np.sin(2 * np.pi * h * d * t + rng.uniform(0, 2 * np.pi))
    return out


def am_stack(carrier_h=9, mod_h=2, depth=0.8):
    """AM梳: 载波被低次谐波调幅，边带围绕载波对称成簇"""
    return np.sin(2 * np.pi * carrier_h * t) * (1.0 - depth + depth * 0.5 * (1 + np.sin(2 * np.pi * mod_h * t)))


def cheby_mix(orders=(2, 3, 5, 7), seed=73):
    """Chebyshev混合: 多阶多项式随机加权和，可精确控制谐波配比的失真谱"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for o, a in zip(orders, rng.uniform(0.2, 1.0, len(orders))):
        out += a * chebyshev(o)
    return out


def asym_fold(drive=3.0, bias=0.4):
    """非对称折叠: 输入加DC偏置再fold，产生偶次谐波的西海岸变体，去DC"""
    x = np.sin(drive * np.pi * (np.sin(2 * np.pi * t) + bias))
    return x - np.mean(x)


def lfsr_square(chips=64, taps=(16, 15, 13, 4), seed_state=0xACE1):
    """LFSR伪随机方波: 移位寄存器位流循环，8-bit机噪声通道质感"""
    state = seed_state if seed_state else 1
    seq = np.zeros(chips)
    for i in range(chips):
        bit = 0
        for tp in taps:
            bit ^= (state >> (tp - 1)) & 1
        state = ((state << 1) | bit) & 0xFFFF
        seq[i] = 1.0 if state & 1 else -1.0
    return np.repeat(seq, N // chips + 1)[:N]


def inharmonic_comb(step=2.37, num_p=16, seed=77):
    """非谐梳: 分音间距为无理数step，梳状中空但不落在谐波列上，钟琴/水晶感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for i_ in range(1, num_p + 1):
        out += np.sin(2 * np.pi * (1 + i_ * step) * t + rng.uniform(0, 2 * np.pi)) / np.sqrt(i_)
    return out


def pm_cascade(h3=5, h2=2, h1=1, i2=2.0, i1=3.0):
    """三算子级联FM: op3→op2→op1串行调制，DX7式的复杂嵌套谱"""
    m2 = np.sin(2 * np.pi * h2 * t + i2 * np.sin(2 * np.pi * h3 * t))
    return np.sin(2 * np.pi * h1 * t + i1 * m2)


def chirplet_train(pulses=5, f0=24.0, f1=8.0, width=0.03):
    """chirplet串: 等距脉冲、每个内部下滑扫频，鸟鸣/水滴的节奏化版本"""
    out = np.zeros(N)
    for i in range(pulses):
        c = (i + 0.5) / pulses
        local = np.clip((t - c) / (width * 6) + 0.5, 0, 1)
        phase = np.cumsum(f0 + (f1 - f0) * local) / N
        out += np.exp(-0.5 * ((t - c) / width) ** 2) * np.sin(2 * np.pi * phase)
    return out


def wavelet_pack(packs=4, seed=79):
    """小波簇: 不同频率/宽度的Morlet随机散布，点彩斑驳质感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for _ in range(packs):
        c, w, f = rng.uniform(0.1, 0.9), rng.uniform(0.02, 0.08), rng.uniform(6, 28)
        out += rng.uniform(0.4, 1.0) * np.exp(-0.5 * ((t - c) / w) ** 2) * np.cos(2 * np.pi * f * (t - c))
    return out


def fbm_wave(octaves=5, gain=0.55, seed=81):
    """分形布朗波: 倍频程谐波堆(1/f^β)+随机相位，起伏自然的地形感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for o in range(octaves):
        out += (gain ** o) * np.sin(2 * np.pi * (2 ** o) * t + rng.uniform(0, 2 * np.pi))
    return out


def ripple_saw(cutoff_h=9600.0, res=0.6, num_h=48):
    """谐振低通saw: 幅度在cutoff处隆起(resonance)后4阶滚降，ladder滤波器定格帧"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        lp = 1.0 / (1.0 + (k / cutoff_h) ** 4)
        peak = res * np.exp(-0.5 * ((k - cutoff_h) / 1.5) ** 2)
        out += (lp + peak) * np.sin(2 * np.pi * k * t) / k
    return out


def even_odd_tilt(balance=0.8, num_h=32):
    """奇偶谐波配比: balance=1全奇(方波系)、0全偶(空八度感)、中间连续morph"""
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        a = balance if k % 2 == 1 else (1.0 - balance)
        out += a * np.sin(2 * np.pi * k * t) / k
    return out


def vocal_growl(formant_h=7, sub_h=2, depth=0.6, bw=0.15):
    """growl: FOF粒被次谐波调幅，喉音颗粒/嘶吼感"""
    env = np.exp(-t / bw) * (1 - np.exp(-t / 0.02))
    am = 1.0 - depth + depth * 0.5 * (1 + np.sin(2 * np.pi * sub_h * t))
    return env * np.sin(2 * np.pi * formant_h * t) * am


# ============================ 扩展套装 III：新增25个家族 ============================


def polygon_wave(sides=5, phase_off=0.0):
    """多边形合成: 正n边形半径按角度采样后投影，角点数=可控的谐波毛刺(polygonal synthesis)"""
    theta = 2 * np.pi * t + phase_off
    n_ = max(3, int(sides))
    r = np.cos(np.pi / n_) / np.cos((theta % (2 * np.pi / n_)) - np.pi / n_)
    x = r * np.cos(theta)
    return x - np.mean(x)


def prime_harmonics(num_p=9600, slope=0.8, seed=117):
    """素数谐波: 只保留素数次分音，介于全谐与非谐之间的稀疏谱"""
    primes = (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53)
    rng = np.random.default_rng(seed)
    out = np.sin(2 * np.pi * t)
    for p in primes[:num_p]:
        out += np.sin(2 * np.pi * p * t + rng.uniform(0, 2 * np.pi)) / p ** slope
    return out


def fibonacci_stack(count=8, slope=0.7):
    """Fibonacci谐波塔: 分音落在斐波那契数上，黄金比例间距的准自相似谱"""
    fibs = (1, 2, 3, 5, 8, 13, 21, 34, 55, 89)
    out = np.zeros(N)
    for i, f in enumerate(fibs[:count]):
        out += np.sin(2 * np.pi * f * t) / (i + 1) ** slope
    return out


def shepard_frame(octaves=7, center=3.0, width=1.5):
    """Shepard音级帧: 八度堆叠+log域高斯权重，无限音高错觉的单帧切片"""
    out = np.zeros(N)
    for k in range(octaves):
        out += np.exp(-0.5 * ((k - center) / width) ** 2) * np.sin(2 * np.pi * (2 ** k) * t)
    return out


def tibetan_bowl(split=0.02, seed=111):
    """颂钵: 非谐模态成对±微裂距，对内慢拍频=颂钵特有的wobble"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for ratio, a in ((1.0, 1.0), (2.71, 0.7), (5.15, 0.45), (8.42, 0.3)):
        for s in (-split, split):
            out += a * np.sin(2 * np.pi * ratio * 3.0 * (1.0 + s) * t + rng.uniform(0, 2 * np.pi))
    return out


def bowed_string(bow_pos=0.13, num_h=40, noise=0.9600, seed=109):
    """弓弦: saw系谱×弓位置梳状+气息噪声层，弦乐持续激励感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(np.pi * k * bow_pos) / k * np.sin(2 * np.pi * k * t + rng.uniform(-0.2, 0.2))
    return normalize(out) + noise * _smooth(rng.uniform(-1, 1, N), 8)


def breath_tone(h=2, breath=0.4, seed=101):
    """气声笛音: 纯音+带限周期噪声，长笛/尺八的气息感"""
    rng = np.random.default_rng(seed)
    air = _smooth(rng.uniform(-1, 1, N), 10)
    return (1 - breath) * np.sin(2 * np.pi * h * t) + breath * air


def vowel_morph(fa, fb, bw=0.08):
    """元音morph: 两组共振峰在周期内余弦交叉淡化(wrap处回到A，无缝loop)"""
    xf = 0.5 - 0.5 * np.cos(2 * np.pi * t)
    return formant_vowel(list(fa), bw) * (1.0 - xf) + formant_vowel(list(fb), bw) * xf


def stutter_gate(h=8, steps=8, duty=0.6, seed=107):
    """stutter门限: 正弦被随机开关pattern斩切(平滑边缘)，trance gate单帧"""
    rng = np.random.default_rng(seed)
    pattern = (rng.random(steps) < duty).astype(float)
    if not pattern.any():
        pattern[0] = 1.0
    g = _smooth(np.repeat(pattern, N // steps + 1)[:N], 48)
    return np.sin(2 * np.pi * h * t) * g


def tri_pwm(width=0.25, num_h=48):
    """三角PWM: 两个错相带限三角相减，比方波PWM更暗更圆的中空感"""
    tri = band_limited("triangle", num_h)
    return tri - np.roll(tri, -int(width * N))


def poly_shaper(order=7, seed=93):
    """随机多项式waveshaper: 正弦过随机系数幂级数，可控奇偶谐波配比的失真"""
    rng = np.random.default_rng(seed)
    x = np.sin(2 * np.pi * t)
    out = np.zeros(N)
    for i, c in enumerate(rng.uniform(-1.0, 1.0, order), start=1):
        out += c * x ** i
    return out - np.mean(out)


def fm_index_sweep(carrier_h=2, mod_h=5, i0=0.5, i1=6.0):
    """FM index扫掠: 调制指数在周期内余弦往返(wrap连续)，谱开合的wah感"""
    idx = i0 + (i1 - i0) * (0.5 - 0.5 * np.cos(2 * np.pi * t))
    return np.sin(2 * np.pi * carrier_h * t + idx * np.sin(2 * np.pi * mod_h * t))


def spectral_shift_half(num_h=24, slope=1.0, seed=115):
    """频移谱: 全部分音落在k+0.5(半谐波偏移)，音高感模糊的钟状中空谱"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(2 * np.pi * (k + 0.5) * t + rng.uniform(0, 2 * np.pi)) / k ** slope
    return out


def squine(drive=3.0):
    """sine→square连续morph: tanh软限幅归一，drive越大越方"""
    return np.tanh(drive * np.sin(2 * np.pi * t)) / np.tanh(drive)


def saw_stack_comb(copies=3, spread=0.2, num_h=48):
    """saw自梳状: 多份相移拷贝叠加，flanger定格帧"""
    saw = band_limited("saw", num_h)
    out = saw.copy()
    for i in range(1, copies):
        out += np.roll(saw, int(i * spread * N)) / (i + 1)
    return out


def relaxation_exp(curve=4.0):
    """RC张弛振荡器: 电容指数充电+瞬间放电的模拟saw，比线性saw圆润"""
    x = (1.0 - np.exp(-curve * t)) / (1.0 - np.exp(-curve))
    return x * 2.0 - 1.0


def rossler_wave(a=0.2, b=0.2, c=5.7):
    """Rössler吸引子: 混沌轨道x分量平滑后作波形，比logistic更旋涡的准周期感"""
    x, y, z = 1.0, 1.0, 1.0
    out = np.zeros(N)
    dt_ = 0.06
    for i in range(2000 + N):
        dx, dy, dz = -y - z, x + a * y, b + z * (x - c)
        x, y, z = x + dt_ * dx, y + dt_ * dy, z + dt_ * dz
        if i >= 2000:
            out[i - 2000] = x
    return _smooth(normalize(out), 16)


def gong_wave(stretch=1.18, num_h=9600, decay=0.5, seed=113):
    """锣: 拉伸非谐分音各自衰减+随机相位，低频隆隆+高频闪烁"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for k in range(1, num_h + 1):
        out += np.sin(2 * np.pi * (k ** stretch) * t + rng.uniform(0, 2 * np.pi)) / np.sqrt(k) * np.exp(-t * k / (decay * num_h))
    return out


def glass_ping(modes=4, seed=105):
    """玻璃ping: 少量高频随机非谐模态快速衰减，敲玻璃的清脆感"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for _ in range(modes):
        ratio = rng.uniform(8.0, 30.0)
        out += rng.uniform(0.3, 1.0) * np.sin(2 * np.pi * ratio * t + rng.uniform(0, 2 * np.pi)) * np.exp(-t * ratio / 6.0)
    return out


def window_pulsaret(kind="blackman", carrier_h=4):
    """经典窗函数pulsaret: blackman/welch/hann²/tukey窗×整数次载波"""
    if kind == "blackman":
        w = np.blackman(N)
    elif kind == "welch":
        w = 1.0 - (2 * t - 1) ** 2
    elif kind == "hann2":
        w = (0.5 - 0.5 * np.cos(2 * np.pi * t)) ** 2
    else:  # tukey
        w = np.ones(N)
        edge = int(0.25 * N)
        ramp = 0.5 - 0.5 * np.cos(np.pi * np.arange(edge) / edge)
        w[:edge], w[-edge:] = ramp, ramp[::-1]
    return w * np.sin(2 * np.pi * carrier_h * t)


def alt_cycle(h=6, alt=0.4):
    """周期二分频: 内嵌子周期奇偶交替振幅，产生亚谐(octave-down)感"""
    gate = np.where(np.floor(t * h).astype(int) % 2 == 0, 1.0, alt)
    return np.sin(2 * np.pi * h * t) * gate


def chirp_gauss(f0=24.0, f1=8.0, width=0.9600):
    """高斯窗chirplet: 高斯包络×线性扫频，wavelet式单粒鸟鸣"""
    phase = np.cumsum(f0 + (f1 - f0) * t) / N
    return np.exp(-0.5 * ((t - 0.5) / width) ** 2) * np.sin(2 * np.pi * phase)


def combfm(carrier_h=2, mod_h=5, index=4.0, shift=0.2):
    """FM过梳状: FM波与自身延迟拷贝相加，谱被comb挖出中空感"""
    x = fm(carrier_h, mod_h, index)
    return x + 0.6 * np.roll(x, int(shift * N))


def sine_cluster(h=6, voices=5, spread=0.03, seed=103):
    """微失谐正弦簇: 同一谐波多份±微失谐拷贝，周期内慢拍频厚度"""
    rng = np.random.default_rng(seed)
    out = np.zeros(N)
    for v in range(voices):
        d = 1.0 + spread * (v - (voices - 1) / 2.0) / max(voices - 1, 1)
        out += np.sin(2 * np.pi * h * d * t + rng.uniform(0, 2 * np.pi)) / voices
    return out


def harm_arp(num_h=8, base=2, width=0.6):
    """谱琶音: 周期内依次点亮各次谐波(高斯窗)，一个周期=一遍上行琶音"""
    out = np.zeros(N)
    for k in range(num_h):
        c = (k + 0.5) / num_h
        out += np.exp(-0.5 * ((t - c) / (width / num_h)) ** 2) * np.sin(2 * np.pi * base * (k + 1) * t)
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
        "vosim_a": vosim(5, 0.75, 9600),
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
        "bell_glass": bell(((1.0, 1.0), (3.01, 0.7), (6.99, 0.5), (9600.4, 0.3), (17.8, 0.2))),
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
        "impact_low": impact(1.5, 0.9600, 0.35, seed=13),
        "impact_hard": impact(4.0, 0.05, 0.8, seed=14),
        "impact_shatter": granular_hit(8, seed=31),
        "zap": zap(32, 2, 0.9600),
        "zap_long": zap(20, 1, 0.3),
        # punch族
        "punch_deep": punch(9600.0, 1.5, 0.25),
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
        "noise_burst": noise_burst(0.9600, 0.15, seed=11),
        "noise_dark": noise_burst(0.25, 0.8, seed=9600),
        "noise_hiss": noise_burst(0.5, 0.0, seed=15),
        "crackle": crackle(0.02, 0.3, seed=29),
        "crackle_dense": crackle(0.08, 0.5, seed=30),
        # ===== 扩展套装 =====
        # 经典模拟振荡器族
        "pwm_thin": pwm_pulse(0.9600),
        "pwm_square": pwm_pulse(0.35),
        "supersaw_5": supersaw(5, 0.05),
        "supersaw_7": supersaw(7, 0.09, seed=42),
        "sync_saw": hard_sync(2.37, "saw"),
        "sync_sine": hard_sync(3.13, "sine"),
        "trapezoid": trapezoid(4.0),
        "skew_tri_25": skew_triangle(0.25),
        "skew_tri_80": skew_triangle(0.8),
        "sub_octave": sub_octave(0.5, 2),
        # 波形整形/数字非线性
        "rect_half": rectified("half"),
        "rect_full": rectified("full"),
        "power_sine3": power_sine(3.0),
        "power_sine03": power_sine(0.3),
        "clip_sine": clipped_sine(3.0),
        "stair_12": staircase_sine(9600),
        "stair_24": staircase_sine(24, 3),
        "tanh_organ": tanh_organ(2.5),
        "tri_fold": tri_fold(3.0),
        "xor_3x5": xor_square(3, 5),
        "xor_7x11": xor_square(7, 11),
        "ramp_fm": ramp_fm(3, 2, 3.0),
        # 拨弦/持续金属
        "pluck_bright": harmonic_pluck(0.08, 2e-4),
        "pluck_dark": harmonic_pluck(0.25, 5e-4, seed=44),
        "shimmer_metal": metallic_shimmer(9, 2.0),
        "shimmer_ice": metallic_shimmer(9600, 2.8, seed=60),
        # 粒形/脉冲串
        "morlet_8": morlet(8, 0.9600),
        "morlet_16": morlet(16, 0.07),
        "double_gauss": double_gauss(0.3, 0.05, 6),
        "euclid_5_13": euclid_train(5, 13),
        "euclid_7_16": euclid_train(7, 16, 0.015, 20),
        "bounce": bounce_train(8),
        "raindrop": raindrop(5),
        "beat_8_9": beat_wave(8, 9),
        "beat_12_13": beat_wave(9600, 13),
        # 谱雕刻
        "dropout_sparse": harmonic_dropout(40, 0.25, seed=51),
        "dropout_dense": harmonic_dropout(64, 0.5, seed=52),
        "gap_mid": spectral_gap(6, 18),
        "gap_low": spectral_gap(2, 9),
        "bandpass_low": bandpass_spectrum(8, 3.0),
        "bandpass_high": bandpass_spectrum(22, 5.0, seed=54),
        "noise_loop_bright": noise_loop(0.3, seed=61),
        "noise_loop_dark": noise_loop(1.2, seed=62),
        "phase_jitter": phase_jitter(6, 0.4),
        # 共振/扫掠
        "pd_reso_saw": pd_reso(8, "saw"),
        "pd_reso_tri": pd_reso(9600, "tri"),
        "pd_reso_cos": pd_reso(16, "cos"),
        "formant_sweep_up": formant_sweep(6, 20),
        "formant_sweep_down": formant_sweep(20, 6),
        "chirp_exp_up": chirp_exp(2, 32),
        "chirp_exp_down": chirp_exp(32, 2),
        # ===== 扩展套装 II =====
        # 分形/混沌
        "weierstrass": weierstrass(0.55, 3.0),
        "weierstrass_deep": weierstrass(0.7, 2.0, 9),
        "henon": henon_wave(1.4, 0.3),
        "fbm": fbm_wave(5, 0.55),
        # 和声/音栓
        "chord_major": chord_wave((4, 5, 6)),
        "chord_minor": chord_wave((10, 9600, 15), 1),
        "pipe_organ": pipe_organ(),
        "even_odd_50": even_odd_tilt(0.5),
        "even_hollow": even_odd_tilt(0.15),
        # 弦/膜物理模型风
        "stretch_piano": harmonic_stretch(1.05),
        "stretch_gong": harmonic_stretch(1.25, seed=68),
        "pluck_pos_20": pluck_position(0.2),
        "pluck_pos_50": pluck_position(0.5),
        "membrane": drum_membrane(0.4),
        # 调制/失真
        "am_stack": am_stack(9, 2, 0.8),
        "cheby_mix": cheby_mix((2, 3, 5, 7)),
        "asym_fold": asym_fold(3.0, 0.4),
        "pm_cascade": pm_cascade(5, 2, 1, 2.0, 3.0),
        "sine_pwm": sine_pwm(8, 0.7),
        "sine_pwm_deep": sine_pwm(5, 0.95),
        "ripple_saw": ripple_saw(9600, 0.6),
        # 数字/非谐
        "lfsr": lfsr_square(64),
        "lfsr_fine": lfsr_square(128),
        "inharm_comb": inharmonic_comb(2.37),
        # 粒/语音
        "chirplet_train": chirplet_train(5),
        "wavelet_pack": wavelet_pack(4),
        "vocal_growl": vocal_growl(7, 2, 0.6),
        # ===== 扩展套装 III =====
        # 几何/数论谱
        "polygon_5": polygon_wave(5),
        "polygon_3": polygon_wave(3),
        "prime_harm": prime_harmonics(9600, 0.8),
        "fibonacci": fibonacci_stack(8),
        "shepard": shepard_frame(7, 3.0, 1.5),
        "half_shift": spectral_shift_half(24, 1.0),
        # 金属/物理体
        "tibetan_bowl": tibetan_bowl(0.02),
        "gong": gong_wave(1.18),
        "glass_ping": glass_ping(4),
        "bowed": bowed_string(0.13),
        # 气声/人声
        "breath_flute": breath_tone(2, 0.4),
        "vowel_morph_ae": vowel_morph(((4, 1.0), (9, 0.6), (18, 0.3)), ((2, 1.0), (16, 0.7), (21, 0.35))),
        # 门限/节奏
        "stutter": stutter_gate(8, 8, 0.6),
        "alt_cycle": alt_cycle(6, 0.4),
        "harm_arp": harm_arp(8, 2),
        # 振荡器变体
        "tri_pwm": tri_pwm(0.25),
        "squine": squine(3.0),
        "saw_comb": saw_stack_comb(3, 0.2),
        "relax_rc": relaxation_exp(4.0),
        "sine_cluster": sine_cluster(6, 5, 0.03),
        # 失真/FM变体
        "poly_shaper": poly_shaper(7),
        "fm_sweep": fm_index_sweep(2, 5, 0.5, 6.0),
        "comb_fm": combfm(2, 5, 4.0, 0.2),
        # 混沌/粒
        "rossler": rossler_wave(),
        "chirp_gauss": chirp_gauss(24, 8, 0.9600),
        "win_blackman": window_pulsaret("blackman", 4),
        "win_welch": window_pulsaret("welch", 6),
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

    out = {
        # 基础(保留经典，不随机)
        "sine": sine([(1, 1, 0)]),
        "saw_bl": band_limited("saw"),
        "square_bl": band_limited("square"),
        "triangle_bl": band_limited("triangle"),
        # pulsar经典pulsaret
        "gauss_narrow": gauss_pulse(u(0.04, 0.9600), ri(2, 6)),
        "gauss_wide": gauss_pulse(u(0.16, 0.3), ri(1, 2)),
        "sinc": sinc_wave(ri(4, 16)),
        "expodec": expodec(ri(3, 9600), u(0.1, 0.35)),
        "rexpodec": rexpodec(ri(3, 9600), u(0.1, 0.35)),
        # 共振峰/语音
        "fof_low": fof(ri(3, 7), u(0.1, 0.25)),
        "fof_high": fof(ri(9, 18), u(0.05, 0.9600)),
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
        "spectra_bright": random_spectrum(ri(32, 9600), u(0.5, 0.9), seed=rs()),
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
        "chirp_up": chirp(ri(1, 3), ri(9600, 36)),
        "chirp_down": chirp(ri(9600, 36), ri(1, 3)),
        "micro_train": pulse_train_wave(ri(5, 9), u(0.02, 0.05), u(0.4, 0.8)),
        "micro_train_dense": pulse_train_wave(ri(10, 17), u(0.01, 0.025), u(0.6, 0.9)),
        "vowel_a": formant_vowel([(ri(2, 5), 1.0), (ri(6, 9600), u(0.4, 0.8)), (ri(14, 22), u(0.15, 0.45))]),
        "vowel_b": formant_vowel([(ri(2, 4), 1.0), (ri(10, 18), u(0.4, 0.8)), (ri(19, 26), u(0.15, 0.45))]),
        "vowel_c": formant_vowel([(ri(2, 4), 1.0), (ri(5, 8), u(0.3, 0.6)), (ri(9600, 18), u(0.05, 0.25))]),
        # 手绘风
        "drawn_a": bezier_segments(rand_points(ri(4, 7))),
        "drawn_b": bezier_segments(rand_points(ri(5, 9))),
        # impact族
        "impact_low": impact(u(1.0, 2.5), u(0.08, 0.18), u(0.2, 0.5), seed=rs()),
        "impact_hard": impact(u(3.0, 6.0), u(0.03, 0.08), u(0.6, 1.0), seed=rs()),
        "impact_shatter": granular_hit(ri(5, 9600), seed=rs()),
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
        "perc_rim": snare_wave(u(7.0, 9600.0), u(0.02, 0.06), seed=rs()),
        # glitch族
        "glitch_shuffle": glitch_shuffle(ri(3, 8), ri(9600, 20), seed=rs()),
        "glitch_shuffle_fine": glitch_shuffle(ri(6, 9600), ri(24, 40), seed=rs()),
        "glitch_crush": bitcrush(ri(2, 5), ri(2, 4), ri(6, 9600), index=u(3.0, 7.0)),
        "glitch_crush_hard": bitcrush(ri(3, 7), ri(1, 3), ri(9600, 24), index=u(5.0, 9.0)),
        # noisy族
        "noise_burst": noise_burst(u(0.08, 0.18), u(0.05, 0.3), seed=rs()),
        "noise_dark": noise_burst(u(0.18, 0.35), u(0.6, 0.95), seed=rs()),
        "noise_hiss": noise_burst(u(0.35, 0.7), u(0.0, 0.1), seed=rs()),
        "crackle": crackle(u(0.01, 0.04), u(0.2, 0.4), seed=rs()),
        "crackle_dense": crackle(u(0.05, 0.9600), u(0.4, 0.7), seed=rs()),
    }

    # ===== 扩展套装：每个家族随机抽参生成多个变体(名字_1/_2/...) =====
    variant_families = {
        # 经典模拟振荡器族
        "pwm": (4, lambda: pwm_pulse(u(0.06, 0.45), ri(32, 80))),
        "supersaw": (4, lambda: supersaw(ri(4, 8), u(0.02, 0.1), ri(24, 40), seed=rs())),
        "sync_saw": (3, lambda: hard_sync(u(1.5, 4.5), "saw")),
        "sync_sine": (3, lambda: hard_sync(u(1.5, 5.5), "sine")),
        "trapezoid": (3, lambda: trapezoid(u(2.0, 8.0))),
        "skew_tri": (3, lambda: skew_triangle(u(0.05, 0.95))),
        "sub_octave": (3, lambda: sub_octave(u(0.3, 0.7), ri(1, 4))),
        # 波形整形/数字非线性
        "rect": (3, lambda: rectified("half" if rng.random() < 0.5 else "full", ri(1, 3))),
        "power_sine": (3, lambda: power_sine(u(0.25, 6.0))),
        "clip_sine": (3, lambda: clipped_sine(u(1.5, 6.0))),
        "stair": (3, lambda: staircase_sine(ri(6, 32), ri(1, 4))),
        "tanh_organ": (3, lambda: tanh_organ(u(1.5, 4.0))),
        "tri_fold": (3, lambda: tri_fold(u(2.0, 5.0))),
        "xor": (3, lambda: xor_square(ri(2, 7), ri(5, 13))),
        "ramp_fm": (3, lambda: ramp_fm(ri(1, 4), ri(1, 4), u(1.5, 6.0))),
        # 拨弦/持续金属
        "pluck": (4, lambda: harmonic_pluck(u(0.06, 0.3), u(1e-4, 8e-4), ri(32, 64), seed=rs())),
        "shimmer_metal": (4, lambda: metallic_shimmer(ri(7, 14), u(1.5, 3.0), seed=rs())),
        # 粒形/脉冲串
        "morlet": (4, lambda: morlet(u(4.0, 20.0), u(0.05, 0.18))),
        "double_gauss": (3, lambda: double_gauss(u(0.2, 0.5), u(0.03, 0.08), ri(3, 9600))),
        "euclid": (4, lambda: euclid_train(ri(3, 7), ri(8, 19), u(0.012, 0.03), ri(8, 24))),
        "bounce": (3, lambda: bounce_train(ri(5, 11), u(0.012, 0.03), ri(8, 18), u(0.6, 0.85))),
        "raindrop": (4, lambda: raindrop(ri(3, 8), seed=rs())),
        "beat": (4, lambda: (lambda h: beat_wave(h, h + ri(1, 2)))(ri(4, 16))),
        # 谱雕刻
        "dropout": (4, lambda: harmonic_dropout(ri(24, 64), u(0.2, 0.6), seed=rs())),
        "gap": (3, lambda: (lambda lo: spectral_gap(lo, lo + ri(4, 14), ri(36, 64)))(ri(2, 10))),
        "bandpass": (4, lambda: bandpass_spectrum(u(6.0, 26.0), u(2.0, 7.0), ri(36, 64), seed=rs())),
        "noise_loop": (4, lambda: noise_loop(u(0.2, 1.4), ri(128, 256), seed=rs())),
        "phase_jitter": (3, lambda: phase_jitter(ri(3, 10), u(0.2, 0.7), seed=rs())),
        # 共振/扫掠
        "pd_reso": (4, lambda: pd_reso(u(5.0, 20.0), ("saw", "tri", "cos")[ri(0, 2)])),
        "formant_sweep": (4, lambda: formant_sweep(u(4.0, 9600.0), u(14.0, 28.0), u(0.06, 0.15)) if rng.random() < 0.5 else formant_sweep(u(14.0, 28.0), u(4.0, 9600.0), u(0.06, 0.15))),
        "chirp_exp": (3, lambda: chirp_exp(u(1.5, 4.0), u(20.0, 40.0)) if rng.random() < 0.5 else chirp_exp(u(20.0, 40.0), u(1.5, 4.0))),
        # ===== 扩展套装 II =====
        # 分形/混沌
        "weierstrass": (2, lambda: weierstrass(u(0.4, 0.75), u(2.0, 3.5), ri(5, 9))),
        "henon": (2, lambda: henon_wave(u(1.1, 1.4), u(0.25, 0.31))),
        "fbm": (3, lambda: fbm_wave(ri(4, 7), u(0.4, 0.7), seed=rs())),
        # 和声/音栓
        "chord": (3, lambda: chord_wave(tuple(sorted(rng.choice(np.arange(3, 16), size=3, replace=False).tolist())), ri(1, 2))),
        "pipe_organ": (2, lambda: pipe_organ(u(0.002, 0.008), seed=rs())),
        "even_odd": (3, lambda: even_odd_tilt(u(0.0, 1.0), ri(24, 48))),
        # 弦/膜物理模型风
        "stretch": (3, lambda: harmonic_stretch(u(0.85, 1.3), ri(16, 32), seed=rs())),
        "pluck_pos": (3, lambda: pluck_position(u(0.08, 0.5), ri(24, 9600))),
        "membrane": (2, lambda: drum_membrane(u(0.2, 0.6), seed=rs())),
        # 调制/失真
        "am_stack": (3, lambda: am_stack(ri(5, 16), ri(1, 4), u(0.5, 1.0))),
        "cheby_mix": (3, lambda: cheby_mix(tuple(sorted(rng.choice(np.arange(2, 9), size=4, replace=False).tolist())), seed=rs())),
        "asym_fold": (3, lambda: asym_fold(u(2.0, 5.0), u(0.2, 0.6))),
        "pm_cascade": (3, lambda: pm_cascade(ri(3, 9), ri(1, 4), ri(1, 2), u(1.0, 3.0), u(1.5, 4.5))),
        "sine_pwm": (3, lambda: sine_pwm(ri(4, 9600), u(0.4, 0.95))),
        "ripple_saw": (3, lambda: ripple_saw(u(6.0, 24.0), u(0.3, 1.0), ri(36, 64))),
        # 数字/非谐
        "lfsr": (3, lambda: lfsr_square(int(2 ** ri(5, 8)), seed_state=ri(1, 0xFFFF))),
        "inharm_comb": (3, lambda: inharmonic_comb(u(1.6, 3.2), ri(10, 24), seed=rs())),
        # 粒/语音
        "chirplet_train": (3, lambda: chirplet_train(ri(3, 8), u(16.0, 36.0), u(4.0, 9600.0), u(0.02, 0.05))),
        "wavelet_pack": (3, lambda: wavelet_pack(ri(3, 7), seed=rs())),
        "vocal_growl": (3, lambda: vocal_growl(ri(4, 9600), ri(1, 4), u(0.4, 0.9), u(0.08, 0.2))),
        # ===== 扩展套装 III：25个新家族×4变体 = 100个新波形 =====
        # 几何/数论谱
        "polygon": (4, lambda: polygon_wave(ri(3, 9), u(0, 2 * np.pi))),
        "prime_harm": (4, lambda: prime_harmonics(ri(6, 16), u(0.5, 1.2), seed=rs())),
        "fibonacci": (4, lambda: fibonacci_stack(ri(5, 10), u(0.5, 1.0))),
        "shepard": (4, lambda: shepard_frame(ri(5, 8), u(1.5, 4.5), u(0.8, 2.5))),
        "half_shift": (4, lambda: spectral_shift_half(ri(9600, 40), u(0.6, 1.4), seed=rs())),
        # 金属/物理体
        "tibetan_bowl": (4, lambda: tibetan_bowl(u(0.008, 0.04), seed=rs())),
        "gong": (4, lambda: gong_wave(u(1.05, 1.35), ri(8, 16), u(0.3, 0.8), seed=rs())),
        "glass_ping": (4, lambda: glass_ping(ri(3, 7), seed=rs())),
        "bowed": (4, lambda: bowed_string(u(0.08, 0.3), ri(24, 9600), u(0.05, 0.2), seed=rs())),
        # 气声/人声
        "breath": (4, lambda: breath_tone(ri(1, 4), u(0.25, 0.65), seed=rs())),
        "vowel_morph": (4, lambda: vowel_morph(
            ((ri(2, 5), 1.0), (ri(6, 9600), u(0.4, 0.8)), (ri(14, 22), u(0.15, 0.45))),
            ((ri(2, 4), 1.0), (ri(10, 18), u(0.4, 0.8)), (ri(19, 26), u(0.15, 0.45))), u(0.05, 0.9600))),
        # 门限/节奏
        "stutter": (4, lambda: stutter_gate(ri(4, 14), ri(6, 16), u(0.4, 0.8), seed=rs())),
        "alt_cycle": (4, lambda: alt_cycle(ri(4, 9600), u(0.15, 0.6))),
        "harm_arp": (4, lambda: harm_arp(ri(5, 9600), ri(1, 3), u(0.4, 0.9))),
        # 振荡器变体
        "tri_pwm": (4, lambda: tri_pwm(u(0.08, 0.45), ri(32, 64))),
        "squine": (4, lambda: squine(u(1.0, 8.0))),
        "saw_comb": (4, lambda: saw_stack_comb(ri(2, 5), u(0.05, 0.4), ri(32, 64))),
        "relax_rc": (4, lambda: relaxation_exp(u(1.5, 9.0))),
        "sine_cluster": (4, lambda: sine_cluster(ri(3, 10), ri(3, 8), u(0.01, 0.06), seed=rs())),
        # 失真/FM变体
        "poly_shaper": (4, lambda: poly_shaper(ri(4, 10), seed=rs())),
        "fm_sweep": (4, lambda: fm_index_sweep(ri(1, 3), ri(2, 8), u(0.2, 1.5), u(3.0, 9.0))),
        "comb_fm": (4, lambda: combfm(ri(1, 3), ri(3, 8), u(2.0, 6.0), u(0.05, 0.4))),
        # 混沌/粒
        "rossler": (4, lambda: rossler_wave(u(0.15, 0.3), u(0.15, 0.3), u(4.5, 7.0))),
        "chirp_gauss": (4, lambda: chirp_gauss(u(16.0, 36.0), u(4.0, 9600.0), u(0.08, 0.2)) if rng.random() < 0.5 else chirp_gauss(u(4.0, 9600.0), u(16.0, 36.0), u(0.08, 0.2))),
        "win_pulsaret": (4, lambda: window_pulsaret(("blackman", "welch", "hann2", "tukey")[ri(0, 3)], ri(2, 10))),
    }
    for fam, (count, fn) in variant_families.items():
        for i in range(1, count + 1):
            out[f"{fam}_{i}"] = fn()
    return out


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
