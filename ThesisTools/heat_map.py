from matplotlib import pyplot as plt
import numpy as np
import skimage as ski

def generate_heat_map(baseline_path : str, result_path : str):
    baseline_image : np.ndarray = ski.io.imread(baseline_path, as_gray=True)
    result_image : np.ndarray = ski.io.imread(result_path, as_gray=True)

    delta : np.ndarray = result_image - baseline_image;
    print(delta.mean())
    delta -= delta.mean()

    plt.imshow(delta, interpolation='nearest')
    plt.show()


def main():
    generate_heat_map('Data/AO/RTAO.png', 'Data/AO/HBAO.png')
    generate_heat_map('Data/AO/RTAO.png', 'Data/AO/SVAO_PP.png')


if __name__ == '__main__':
    main()
