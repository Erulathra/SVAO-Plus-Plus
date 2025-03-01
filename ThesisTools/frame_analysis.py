import sys
import skimage as ski
import math

BASELINE_FILE='Data/RTAO_Statue.png'
RESULT_FILE='Data/SVAO_PP_Statue.png'

def analyse_frame(baseline_path, result_path):
    print(f'Analysing baseline: {baseline_path} vs {result_path}')

    baseline_image = ski.io.imread(baseline_path, as_gray=True)
    result_image = ski.io.imread(result_path, as_gray=True)

    simm = ski.metrics.structural_similarity(baseline_image, result_image, data_range=baseline_image.max() - baseline_image.min())
    mse = ski.metrics.mean_squared_error(baseline_image, result_image)
    rmse = math.sqrt(ski.metrics.mean_squared_error(baseline_image, result_image))
    psnr = ski.metrics.peak_signal_noise_ratio(baseline_image, result_image)

    print(f"SIMM:\t{simm}")
    print(f"MSE:\t{mse}")
    print(f"RMSE:\t{rmse}")
    print(f"PSNR:\t{psnr}dB")

def main():
    if len(sys.argv) == 3:
        analyse_frame(sys.argv[1], sys.argv[2])
    elif len(sys.argv) == 1:
        analyse_frame(BASELINE_FILE, RESULT_FILE)
    else:
        print("Error: wrong number of arguments")

if __name__ == '__main__':
    main()
