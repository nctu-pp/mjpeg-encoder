'''
the objective of this file is to get the measurement result of OpenMP and serial version
'''

import os
import numpy as np
import re
import csv


def read_file(file_path):
    ll = []
    f = open(file_path, "r")
    lines = f.readlines()
    for line in lines:
        if(not line.startswith("[A") and not line.startswith("Video") and len(line)!=1):
            ll.append(line)
    return ll
    
def read_content(single_content_list):
    padding_list = []
    dct_list = []
    encode_list = []
    run_time_list = []
    for line in single_content_list:
        if(line.startswith("Padding")):
            padding_list.append(line.split()[2])
        elif(line.startswith("Dct")):
            dct_list.append(line.split()[2])
        elif(line.startswith("Enc")):
            encode_list.append(line.split()[2])
        elif(line.startswith("RUN")):
            run_time_list.append(line.split()[2])
    assert len(padding_list) == len(dct_list)
    assert len(encode_list) == len(dct_list)
    assert len(run_time_list) == len(dct_list)
    assert len(run_time_list) == len(padding_list)
    return padding_list, dct_list, encode_list, run_time_list
        
def get_list_avg(padding_list, dct_list, encode_list, run_time_list):
    padding_arr = np.asarray(padding_list)
    dct_arr = np.asarray(dct_list)
    encode_arr = np.asarray(encode_list)
    run_time_arr = np.asarray(run_time_list)
    pad_mean = np.mean(padding_arr[1:-1].astype(np.float))
    dct_mean = np.mean(dct_arr[1:-1].astype(np.float))
    encode_mean = np.mean(encode_arr[1:-1].astype(np.float))
    run_time_mean = np.mean(run_time_arr[1:-1].astype(np.float))
    print("padding arr :", pad_mean)
    print("dct arr     :", dct_mean)
    print("encode arr  :", encode_mean)
    print("run_time arr:", run_time_mean)
    return pad_mean, dct_mean, encode_mean, run_time_mean

RESULT_PATH = "./result"
input_cases = os.listdir(RESULT_PATH)

input_case_dir_list = []
for file in input_cases:
    input_case_dir_list.append(os.path.join(RESULT_PATH, file))

record_list = []
dic = {}
for input_case_path in input_case_dir_list:
    _thread_files = os.listdir(input_case_path)
    _thread_files = sorted(_thread_files, key = lambda thread:(len(thread), str(re.findall("_t(.*)_q", thread)[0])))

    for _thread_file in _thread_files:
        content_list = []
        if(not _thread_file.startswith("time_topen")):
            target_file_path = os.path.join(input_case_path, _thread_file)
            print(target_file_path)
            single_content_list = read_file(target_file_path)
            padding_list, dct_list, encode_list, run_time_list = read_content(single_content_list)
            pad_mean, dct_mean, encode_mean, run_time_mean = get_list_avg(padding_list, dct_list, encode_list, run_time_list)
            dic[target_file_path] = [pad_mean, dct_mean, encode_mean, run_time_mean]
            # content_list.append(single_content_list)
        # print(single_content_list)

with open('openmp.csv', 'w', newline='') as csvfile:
    fieldnames = ["file_path", "padding", "dct", "enc", "run_time","io_time"]
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for key, value in dic.items():
        writer.writerow({'file_path':key, 'padding':value[0], 'dct': value[1], 'enc': value[2], 'run_time':value[3], 'io_time': value[3]-value[2]-value[1]-value[0]})
