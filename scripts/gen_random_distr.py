import os
import argparse
import numpy as np

from pathlib import Path
from enum import Enum, auto


class Pattern(Enum):
    RANDOM = auto()
    ALTERNATING = auto()


def urandom_distr(bits) -> bytes:
    # Gives unbalanced proportions, but could be good
    return os.urandom(bits//8)


def to_byte(bin_iter):
    # Takes 8 bits and returns the corresonding int
    total = 0
    for bin in bin_iter:
        total = (total << 1) + bin

    return int(total).to_bytes(1, 'big')



def biased_random_distr(zero_bits, one_bits):

    bin_arr = np.array([0] * zero_bits + [1] * one_bits)
    np.random.shuffle(bin_arr)
    bytes_list = [to_byte(bin_arr[i:i + 8]) for i in range(0, bin_arr.size, 8)]

    return b''.join(bytes_list)


def alternating_distr(bits) -> bytes:
    # \xaa is 10101010 in binary, so let each byte be that
    return b'\xaa'*(bits//8)


def main(bits: int=10_000, threads=16, pattern=Pattern.RANDOM.value, folder='./workload'):
    pattern = Pattern(pattern)

    folder = f"{folder}/{pattern.name.lower()}"

    os.makedirs(folder, exist_ok=True)

    if (bits % 8 != 0):
        bits += 8 - bits % 8

    for thread in range(threads):
        if pattern == Pattern.RANDOM:
            distr = biased_random_distr(bits//2, bits//2)
        elif pattern == Pattern.ALTERNATING:
            distr = alternating_distr(bits)
        else:
            print("ERROR: Choose a valid pattern!")
            exit()

        with open(f"{folder}/{thread}", 'wb') as f:
            f.write(distr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generates distributions to use as workloads during tests')
    parser.add_argument('--bits', '-b', type=int,
                        help='The amount of bits to generate for each thread')
    parser.add_argument('--threads', '-n', type=int,
                        help='For how many threads to generate the distributions')
    parser.add_argument('--pattern', '-p', type=int,
                        help=f'Which pattern to use, {Pattern.ALTERNATING.value}: Alternating, {Pattern.RANDOM.value}: random')
    parser.add_argument('--folder', '-f',
                        help='Which folder to store the distributions in. NOTE: They will be in subfolder after distribution')

    args = parser.parse_args()
    args_dict = {arg:val for (arg, val) in vars(args).items() if val is not None}

    main(**args_dict)
