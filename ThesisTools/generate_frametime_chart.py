import json
from matplotlib import pyplot as plt
import numpy as np

OPTIMIZED_TRACE_FILE = './Data/SampleTrace.json'
BASELINE_TRACE_FILE = './Data/BaselineTrace.json'

VAO_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/VAO/gpu_time'
VAO_PREPASS_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/VAOPrepass/gpu_time'
SVAO_GPU_NAME = '/onFrameRender/RenderGraphExe::execute()/SVAO/gpu_time'
FRAME_TIME_GPU_NAME = '/onFrameRender/gpu_time'

FRAME_COUNT_NAME = 'frame_count'
EVENTS_NAME = 'events'
RECORDS_NAME = 'records'

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
    vao_prepass = np.array(trace_data[EVENTS_NAME][VAO_PREPASS_GPU_NAME][RECORDS_NAME]);
    vao_records = np.array(trace_data[EVENTS_NAME][VAO_GPU_NAME][RECORDS_NAME]);
    svao_records = np.array(trace_data[EVENTS_NAME][SVAO_GPU_NAME][RECORDS_NAME]);

    plt.plot(frame_time, vao_prepass, 'c', label='VAO Prepass')
    plt.plot(frame_time, vao_records, 'b', label='VAO')
    plt.plot(frame_time, svao_records, 'g', label='SVAO')
    plt.plot(frame_time, frame_time_records, 'r', label='Frame Time')
    plt.legend()

    plt.ylabel('duration [ms]')
    plt.xlabel('time [s]')

    plt.show()

def get_channels_sum(trace_data, channels):
    frame_count = trace_data[FRAME_COUNT_NAME]

    result = np.zeros(frame_count)

    for channel in channels:
        result += trace_data[EVENTS_NAME][channel][RECORDS_NAME]

    return result


def draw_compare_chart(first_trace_data, first_channels, first_name, second_trace_data, second_channels, second_name):
    first_frame_time = calculate_frame_times(first_trace_data)
    first_trace = get_channels_sum(first_trace_data, first_channels)

    second_frame_time = calculate_frame_times(second_trace_data)
    second_trace = get_channels_sum(second_trace_data, second_channels)

    plt.plot(first_frame_time, first_trace, 'r', label=first_name)
    plt.plot(second_frame_time, second_trace, 'b', label=second_name)
    plt.legend()

    plt.ylabel('duration [ms]')
    plt.xlabel('time [s]')

    plt.show()


def main():
    # draw_frame_chart(load_trace(OPTIMIZED_TRACE_FILE))
    # draw_frame_chart(load_trace(BASELINE_TRACE_FILE))
    draw_compare_chart(
        load_trace(BASELINE_TRACE_FILE), [VAO_GPU_NAME, SVAO_GPU_NAME], "SVAO (Baseline)",
        load_trace(OPTIMIZED_TRACE_FILE), [VAO_PREPASS_GPU_NAME, VAO_GPU_NAME, SVAO_GPU_NAME], "SVAO++"
    )

if __name__ == '__main__':
    main()
