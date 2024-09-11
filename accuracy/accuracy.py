import math
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import os
import pandas as pd
import pickle
import pywt
import warnings


def real_to_fixed(x, f):
    x_fixed = np.floor(np.array(x) * 2**f).astype(int)
    return x_fixed


def fixed_to_real(x, f):
    return x / 2**f


def test(f, lut_size, J, x, func, wave):
    y_real = func(x)
    x = real_to_fixed(x, f)
    x_msb = x >> J
    x_lsb = x % 2**J
    del x

    if wave == "quant":
        y_coeffs = func(np.unique(fixed_to_real(x_msb * 2**J, f)))
        y_coeffs[y_coeffs == float("inf")] = 0
        y_coeffs[y_coeffs == -float("inf")] = 0
    else:
        y_coeffs = pywt.downcoef('a', y_real, wave, mode='smooth', level=J)

    if wave == "haar":
        y_coeffs = y_coeffs * 2**(-J/2)
    elif wave == "bior2.2":
        y_coeffs = y_coeffs * 2**(J/2)

    lut_db = real_to_fixed(y_coeffs, f)

    if wave in ("quant", "haar"):
        y_db = lut_db[x_msb]
    elif wave in ("bior2.2"):
        lut0 = np.roll(lut_db, -2)[:2**lut_size]
        lut1 = np.roll(lut_db, -3)[:2**lut_size]
        r = lut0[x_msb] * (2**J - x_lsb) + lut1[x_msb] * x_lsb
        y_db = r // int(2**(2*J))
    elif wave in ("db2"):
        y_coeffs = fixed_to_real(real_to_fixed(y_coeffs, f), f)
        y_db_real = pywt.upcoef('a', y_coeffs, wave, level=J, take=len(y_real))
        y_db =  real_to_fixed(y_db_real, f)
    y_db_real = fixed_to_real(y_db,f)
    diff = np.abs(y_real-y_db_real)
    return np.mean(diff), np.max(diff)


def compute_df(bit_width, precision, func, overwrite=False):
    file_path = f'./data/{func.__name__}.pkl'

    # Check if the dataframe has already been computed
    if os.path.exists(file_path) and not overwrite:
        # Deserialize the DataFrame
        with open(file_path, 'rb') as file:
            df = pickle.load(file)
        print(f"The file {file_path} was loaded successfully.")
        return df

    print(f"Writing to the file {file_path}.")

    domain = np.linspace(0, 2**(bit_width-precision)-2**-precision, 2**bit_width)
    table_size_range = range(1, bit_width)
    errQ = []
    maxQ = []
    errH = []
    maxH = []
    errB = []
    maxB = []
    for lut_size in table_size_range:
        depth = bit_width - lut_size
        mean, maxx = test(precision, lut_size, depth, domain, func, "quant")
        errQ.append(mean)
        maxQ.append(maxx)
        mean, maxx = test(precision, lut_size, depth, domain, func, "haar")
        errH.append(mean)
        maxH.append(maxx)
        mean, maxx = test(precision, lut_size, depth, domain, func, "bior2.2")
        errB.append(mean)
        maxB.append(maxx)

    sz = len(table_size_range)
    df = pd.DataFrame({
        'Compression Depth': [str(bit_width-i) for i in table_size_range] * 3,
        'dwt_type': ['Quantization']*sz + ['Haar']*sz + ['Bior-2.2']*sz,
        'Mean Error': errQ + errH + errB,
        'Max Error': maxQ + maxH + maxB
    })
    # Serialize the DataFrame
    with open(file_path, 'wb') as file:
        pickle.dump(df, file)
    return df


def subplot_from_df(funcs, dfs):
    font = {'weight': 'bold', 'size': 16}
    matplotlib.rc('font', **font)
    fig, axs = plt.subplots(3, 3, figsize=(16, 16))
    for i in range(9):
        ax = axs[i // 3][i % 3]
        df = dfs[i]
        func = funcs[i]
        ax.plot(df['Compression Depth'][df['dwt_type'] == "Quantization"][::-1], df['Mean Error'][df['dwt_type'] == "Quantization"][::-1], label='Quantization')
        ax.plot(df['Compression Depth'][df['dwt_type'] == "Haar"][::-1], df['Mean Error'][df['dwt_type'] == "Haar"][::-1], label='Haar')
        ax.plot(df['Compression Depth'][df['dwt_type'] == "Bior-2.2"][::-1], df['Mean Error'][df['dwt_type'] == "Bior-2.2"][::-1], label='Bior(5,3)')
        if i == 0:
            ax.legend(prop={'size': 16})
        ax.set_yscale('log')
        ax.set_title(f"{func.__name__}", size=20, fontweight="bold", loc="left")
        ax.set_ylabel("Mean Error", size=16)
        ax.set_xlabel("Compression Depth", size=16, loc="right")
        ax.grid(True)
        ax.set_xticks(range(0, df['Compression Depth'].nunique(), 4))
        plt.setp(ax.get_xticklabels(), fontsize=16)
        plt.setp(ax.get_yticklabels(), fontsize=16)
    fig.savefig('./data/lutsize.pdf', format='pdf', bbox_inches='tight')


def gelu(x):
    return np.array([(x_ / 2 * (1 + math.erf(x_/math.sqrt(2)))) for x_ in x])

def selu(x):
    return 1.6733 * 1.0507 * (np.exp(x) - 1) * (x < 0) + 1.0507 * x * (x>=0)

def sigmoid(x):
    return 1 / (1 + np.exp(-x))

def silu(x):
    return x * sigmoid(x)

def softplus(x):
    return np.log(1 + np.exp(x))

def mish(x):
    return x * np.tanh(softplus(x))


def main():
    warnings.filterwarnings("ignore")

    overwrite = True
    precision = 12
    df = [0] * 9

    def GeLU(x):
        return gelu(x-8)
    df[0] = compute_df(
        bit_width = precision + 4,
        precision = precision,
        func = GeLU,
        overwrite = overwrite,
    )
    def Sigmoid(x):
        return sigmoid(x-16)
    df[1] = compute_df(
        bit_width = precision + 5,
        precision = precision,
        func = Sigmoid,
        overwrite = overwrite,
    )
    def Tanh(x):
        return np.tanh(x-8)
    df[2] = compute_df(
        bit_width = precision + 4,
        precision = precision,
        func = Tanh,
        overwrite = overwrite,
    )
    def SiLU(x):
        return silu(x-16)
    df[3] = compute_df(
        bit_width = precision + 5,
        precision = precision,
        func = SiLU,
        overwrite = overwrite,
    )
    def Softplus(x):
        return softplus(x-16)
    df[4] = compute_df(
        bit_width = precision + 5,
        precision = precision,
        func = Softplus,
        overwrite = overwrite,
    )
    def SELU(x):
        return selu(x-16)
    df[5] = compute_df(
        bit_width = precision + 4,
        precision = precision,
        func = SELU,
        overwrite = overwrite,
    )
    def Mish(x):
        return mish(x-16)
    df[6] = compute_df(
        bit_width = precision + 5,
        precision = precision,
        func = Mish,
        overwrite = overwrite,
    )
    def Exponential(x):
        return np.exp(x-16)
    df[7] = compute_df(
        bit_width = precision + 4,
        precision = precision,
        func = Exponential,
        overwrite = overwrite,
    )
    def Reciprocal(x):
        return np.reciprocal(x+1)
    df[8] = compute_df(
        bit_width = precision + 6,
        precision = precision,
        func = Reciprocal,
        overwrite = overwrite,
    )
    subplot_from_df([
            GeLU,
            Sigmoid,
            Tanh,
            SiLU,
            Softplus,
            SELU,
            Mish,
            Exponential,
            Reciprocal
        ],
        df
    )


if __name__ == "__main__":
    main()
