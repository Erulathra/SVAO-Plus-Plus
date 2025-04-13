import sys
import skimage as ski
import math

BASELINE_FILE='Data/RTAO_Tweaked.png'
RESULT_FILE='Data/HBAO.png'

def analyse_frame(baseline_path, result_path):
    print(f'Analysing baseline: {baseline_path} vs {result_path}')

    baseline_image = ski.io.imread(baseline_path, as_gray=True)
    result_image = ski.io.imread(result_path, as_gray=True)

    simm = ski.metrics.structural_similarity(baseline_image, result_image, data_range=baseline_image.max() - baseline_image.min())
    mse = ski.metrics.mean_squared_error(baseline_image, result_image)
    rmse = math.sqrt(ski.metrics.mean_squared_error(baseline_image, result_image))
    psnr = ski.metrics.peak_signal_noise_ratio(baseline_image, result_image)

    print(f"SIMM:\t{simm} (Higher is better")
    print(f"MSE:\t{mse} (Lower is better)")
    print(f"RMSE:\t{rmse} (Lower is better)")
    print(f"PSNR:\t{psnr}dB (Higher is better)")

def main():
    print("Baseline vs Adaptive sampling (DeltaMin)")
    analyse_frame('Data/AdaptiveSampling/baseline_min_delta.png', 'Data/AdaptiveSampling/adaptive_sampling_min_delta.png')
    print("Baseline vs Adaptive sampling (DeltaMax)")
    analyse_frame('Data/AdaptiveSampling/baseline_Max_delta.png', 'Data/AdaptiveSampling/adaptive_sampling_Max_delta.png')

    print("RTAO vs HBAO")
    analyse_frame('Data/Final/RTAO.png', 'Data/Final/HBAO.png')
    print("RTAO vs SVAO")
    analyse_frame('Data/Final/RTAO.png', 'Data/Final/SVAO_Baseline.png')
    print("RTAO vs SVAO++")
    analyse_frame('Data/Final/RTAO.png', 'Data/Final/SVAO_PlusPlus.png')

if __name__ == '__main__':
    main()
