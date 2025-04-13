import json
import os

import matplotlib
from matplotlib import pyplot as plt
import numpy as np
import math

BASELINE_TRACE_FILE = 'Data/SVAO_Baseline.json'
OPTIMIZED_TRACE_FILE = './Data/AdaptiveSampling/SVAO_PP_AdatpiveSampling.json'

# OPTIMIZED_TRACE_FILE = './Data/SVAO_PP_Sponza_Tweaked.json'
# BASELINE_TRACE_FILE = './Data/Baseline_Sponza.json'

# OPTIMIZED_TRACE_FILE = './Data/SVAO_PP_Sponza_NoCurtains.json'
# BASELINE_TRACE_FILE = './Data/Baseline_Sponza_NoCurtains.json'

# OPTIMIZED_TRACE_FILE = './Data/SampleTrace.json'
# BASELINE_TRACE_FILE = './Data/BaselineTrace.json'

VAO_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/VAO/gpu_time'
VAO_PREPASS_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/VAOPrepass/gpu_time'
RT_STOCHASTIC_DEPTH_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/RTStochasticDepth/gpu_time'
SVAO_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/SVAO/gpu_time'
FRAME_TIME_GPU_NAME = '/onFrameRender/gpu_time'
DEFERRED_LIGHTING_NAME = '/onFrameRender/RenderGraphExe::execute()/DeferredLighting/gpu_time'

FRAME_COUNT_NAME = 'frame_count'
EVENTS_NAME = 'events'
RECORDS_NAME = 'records'

SMOOTH_WEIGHT = 0.98

FONT = {'size'   : 13}

def smooth(scalars: list[float], weight: float) -> list[float]:
    """
    EMA implementation according to
    https://github.com/tensorflow/tensorboard/blob/34877f15153e1a2087316b9952c931807a122aa7/tensorboard/components/vz_line_chart2/line-chart.ts#L699
    """
    last = 0
    smoothed = []
    num_acc = 0
    for next_val in scalars:
        last = last * weight + (1 - weight) * next_val
        num_acc += 1
        # de-bias
        debias_weight = 1
        if weight != 1:
            debias_weight = 1 - math.pow(weight, num_acc)
        smoothed_val = last / debias_weight
        smoothed.append(smoothed_val)

    return smoothed

def load_trace(trace_file):
    with open(trace_file) as trace_raw_data:
        trace_data = json.load(trace_raw_data)

        return trace_data

def calculate_frame_times(trace_data):
    frame_count = trace_data[FRAME_COUNT_NAME]
    frame_time = np.zeros(frame_count)
    frame_time_records = np.array(trace_data[EVENTS_NAME][FRAME_TIME_GPU_NAME][RECORDS_NAME]);

    trace_duration = 0
    for frame in range(frame_count):
        trace_duration += frame_time_records[frame]
        frame_time[frame] = trace_duration

    frame_time *= 1e-3

    return frame_time

def draw_frame_chart(trace_data):
    frame_time = calculate_frame_times(trace_data)

    frame_time_records = np.array(trace_data[EVENTS_NAME][FRAME_TIME_GPU_NAME][RECORDS_NAME]);
    frame_time_records = smooth(frame_time_records, SMOOTH_WEIGHT)

    vao_prepass = np.array(trace_data[EVENTS_NAME][VAO_PREPASS_GPU_NAME][RECORDS_NAME]);
    vao_prepass = smooth(vao_prepass, SMOOTH_WEIGHT)

    vao_records = np.array(trace_data[EVENTS_NAME][VAO_GPU_NAME][RECORDS_NAME]);
    vao_records = smooth(vao_records, SMOOTH_WEIGHT)

    svao_records = np.array(trace_data[EVENTS_NAME][SVAO_GPU_NAME][RECORDS_NAME]);
    svao_records = smooth(svao_records, SMOOTH_WEIGHT)

    plt.plot(frame_time, vao_prepass, 'c', label='VAO Prepass')
    plt.plot(frame_time, vao_records, 'b', label='VAO')
    plt.plot(frame_time, svao_records, 'g', label='SVAO')
    plt.plot(frame_time, frame_time_records, 'r', label='Frame Time')
    # plt.legend()

    plt.ylabel('czas przetwarzania [ms]')
    plt.xlabel('czas [s]')

    plt.show()

def get_channels_sum(trace_data, channels):
    frame_count = trace_data[FRAME_COUNT_NAME]

    result = np.zeros(frame_count)

    for channel in channels:
        result += trace_data[EVENTS_NAME][channel][RECORDS_NAME]

    return result


def draw_compare_chart(first_trace_data, first_channels, second_trace_data, second_channels, third_trace_data = None, third_channels = None, save_path = None):
    first_frame_time = calculate_frame_times(first_trace_data)
    first_trace = get_channels_sum(first_trace_data, first_channels)
    first_trace = smooth(first_trace, SMOOTH_WEIGHT)
    plt.plot(first_frame_time, first_trace, 'r')

    if second_trace_data is not None and second_channels is not None:
        second_frame_time = calculate_frame_times(second_trace_data)
        second_trace = get_channels_sum(second_trace_data, second_channels)
        second_trace = smooth(second_trace, SMOOTH_WEIGHT)
        plt.plot(second_frame_time, second_trace, 'b')

    if third_trace_data is not None and third_channels is not None:
        third_frame_time = calculate_frame_times(third_trace_data)
        third_trace = get_channels_sum(third_trace_data, third_channels)
        third_trace = smooth(third_trace, SMOOTH_WEIGHT)

        plt.plot(third_frame_time, third_trace, 'g')

    plt.ylabel('czas przetwarzania [ms]')
    plt.xlabel('czas [s]')

    if save_path is None:
        plt.show()
    else:
        plt.savefig(save_path)

    plt.close()


def main():
    matplotlib.rc('font', **FONT)

    generate_adaptive_sampling_plots()
    generate_prepass_plots()
    generate_blending_plots()
    generate_finals_plots()


def generate_adaptive_sampling_plots():
    channels = [VAO_GPU_NAME, RT_STOCHASTIC_DEPTH_GPU_NAME, SVAO_GPU_NAME];
    base_path = "Output/results/adaptive_sampling"

    os.makedirs(base_path, exist_ok=True)

    draw_compare_chart(
        load_trace("Data/SVAO_Baseline.json"), channels,
        load_trace("Data/AdaptiveSampling/SVAO_PP_AdatpiveSampling.json"), channels,
        save_path = os.path.join(base_path, "baseline_vs_adaptive_curtains.png")
    )
    draw_compare_chart(
        load_trace("Data/SVAO_Baseline_NoCurtains.json"), channels,
        load_trace("Data/AdaptiveSampling/SVAO_PP_AdatpiveSampling_NoCurtains.json"), channels,
        save_path = os.path.join(base_path, "baseline_vs_adaptive_nocurtains.png")
    )
    draw_compare_chart(
        load_trace("Data/SVAO_Baseline.json"), channels,
        load_trace("Data/AdaptiveSampling/SVAO_PP_AdatpiveSampling_SecondPass.json"), channels,
        save_path = os.path.join(base_path, "baseline_vs_adaptive_svao.png")
    )

def generate_prepass_plots():
    baseline_channels = [VAO_GPU_NAME, RT_STOCHASTIC_DEPTH_GPU_NAME, SVAO_GPU_NAME];
    prepass_channels = baseline_channels;
    prepass_channels.append(VAO_PREPASS_GPU_NAME)
    base_path = "Output/results/prepass"

    os.makedirs(base_path, exist_ok=True)

    draw_compare_chart(
        load_trace("Data/Prepass/baseline.json"), baseline_channels,
        load_trace("Data/Prepass/VAO_Prepass.json"), prepass_channels,
        save_path = os.path.join(base_path, "baseline_vs_prepass_curtains.png")
    )

    draw_compare_chart(
        load_trace("Data/SVAO_Baseline_NoCurtains.json"), baseline_channels,
        load_trace("Data/Prepass/VAO_Prepass_NoCurtains.json"), prepass_channels,
        save_path = os.path.join(base_path, "baseline_vs_prepass_nocurtains.png")
    )

    draw_compare_chart(
        load_trace("Data/Prepass/carefull_heuristic.json"), [VAO_GPU_NAME],
        load_trace("Data/Prepass/greedy_heuristic.json"), [VAO_GPU_NAME],
        save_path = os.path.join(base_path, "greedy_vs_carefull.png")
    )

def generate_blending_plots():
    channels = [DEFERRED_LIGHTING_NAME];
    base_path = "Output/results/blending"

    os.makedirs(base_path, exist_ok=True)

    draw_compare_chart(
        load_trace("Data/AOBlending/blending_naive.json"), channels,
        load_trace("Data/AOBlending/blending_ambient.json"), channels,
        load_trace("Data/AOBlending/blending_luminance.json"), channels,
        save_path = os.path.join(base_path, "blending.png")
    )

def generate_finals_plots():
    baseline_channels = [DEFERRED_LIGHTING_NAME, VAO_GPU_NAME, SVAO_GPU_NAME, RT_STOCHASTIC_DEPTH_GPU_NAME];
    channels = [DEFERRED_LIGHTING_NAME, VAO_GPU_NAME, SVAO_GPU_NAME, RT_STOCHASTIC_DEPTH_GPU_NAME, VAO_PREPASS_GPU_NAME];
    base_path = "Output/results/final"

    os.makedirs(base_path, exist_ok=True)

    draw_compare_chart(
        load_trace("Data/Final/SVAO_Baseline_NoCurtains.json"), baseline_channels,
        load_trace("Data/Final/SVAO_PP_NoCurtains.json"), channels,
        save_path = os.path.join(base_path, "no_curtains.png")
    )

    draw_compare_chart(
        load_trace("Data/Final/SVAO_Baseline_Curtains.json"), baseline_channels,
        load_trace("Data/Final/SVAO_PP_Curtains.json"), channels,
        save_path = os.path.join(base_path, "curtains.png")
    )



if __name__ == '__main__':
    main()
