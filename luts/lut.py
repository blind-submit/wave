#!/usr/bin/env python3

# Example: python lut.py -f silu --file lut

import argparse
import math
import numpy as np
import pywt


def real_to_fixed(x, f):
    x_fixed = np.floor(np.array(x) * 2**f).astype(int)
    return x_fixed

def fixed_to_real(x, f):
    return x / 2**f

def get_args():
    parser = argparse.ArgumentParser(description="Generate Lookup Table")
    functions = ['gelu','sigmoid','tanh','silu','softplus','selu','mish','nexp',
                 'reciprocal']
    parser.add_argument(
        "--function",
        "-f",
        choices=functions,
        required=True,
        help="choose a function to create LUT: {}".format(functions),
    )
    parser.add_argument(
        "--negative",
        default=False,
        action="store_true",
        help="domain contains negatives",
    )
    parser.add_argument(
        "--bit-width",
        "-l",
        type=int,
        default=64,
        help="the bit width of the values",
    )
    parser.add_argument(
        "--size",
        "-n",
        type=int,
        default=4,
        help="the bit width of the values",
    )
    parser.add_argument(
        "--precision",
        "-p",
        type=int,
        default=16,
        help="the precision of the fixed rationals",
    )
    parser.add_argument(
        "--wave",
        "-w",
        default="quant",
        help="the wave type to create LUT",
    )
    parser.add_argument(
        "--depth",
        "-d",
        type=int,
        default=0,
        help="the wave depth to apply DWT",
    )
    parser.add_argument(
        "--file",
        default="lut",
        help="the file to write the LUT",
    )
    args = parser.parse_args()
    return args

def gelu(x):
    return np.array([(x_ / 2 * (1 + math.erf(x_/math.sqrt(2)))) for x_ in x])

def sigmoid(x):
    return 1 / (1 + np.exp(-x))

def tanh(x):
    return np.tanh(x)

def silu(x):
    return x * sigmoid(x)

def softplus(x):
    return np.log(1 + np.exp(x))

def selu(x):
    return 1.6733 * 1.0507 * (np.exp(x) - 1) * (x < 0) + 1.0507 * x * (x>=0)

def mish(x):
    return x * np.tanh(softplus(x))

def nexp(x):
    return np.exp(-x)

def reciprocal(x):
    return np.reciprocal(x)

def main():
    args = get_args()
    if args.negative:
        x = np.linspace(-2**(args.size-1-args.precision), 2**(args.size-1-args.precision)-2**-args.precision, 2**args.size)
    else:
        x = np.linspace(0, 2**(args.size-args.precision)-2**-args.precision, 2**args.size)
    y_real = eval(args.function+"(x)")

    if args.wave == "quant":
        x = real_to_fixed(x, args.precision)
        x = x >> args.depth
        y_coeffs = eval(args.function+"(np.unique(fixed_to_real(x * 2**args.depth, args.precision)))")
        y_coeffs[y_coeffs == float("inf")] = 0
        y_coeffs[y_coeffs == -float("inf")] = 0
    else:
        y_coeffs = pywt.downcoef('a', y_real, args.wave, mode='smooth', level=args.depth)

    if args.wave == "haar":
        y_coeffs = y_coeffs * 2**(-args.depth/2)
    elif args.wave == "bior2.2":
        y_coeffs = y_coeffs * 2**(args.depth/2)

    lut_db = real_to_fixed(y_coeffs, args.precision)
    print(lut_db)
    unsigned_lut_db = lut_db % 2**args.bit_width
    lut_bytes = [x.to_bytes((args.bit_width+7)//8) for x in unsigned_lut_db]
    for i, byte in enumerate(lut_bytes):
        lut_bytes[i] = byte[::-1]
    print(lut_bytes)
    byte_array = b''.join(lut_bytes)
    print(len(byte_array))
    print(byte_array)
    with open(args.file, 'wb') as file:
        file.write(byte_array)

if __name__ == "__main__":
    main()
