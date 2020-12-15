import subprocess
import os
import re
num = 50

files = os.listdir("../test_set")
print(files)

raw_list = []
for file_path in files:
    if(file_path.endswith(".raw")):
        raw_list.append(file_path)

def parser_path(path):
    print(path)
    resolution = (re.search('-(.*)-',str(path)).group(1))
    fps = (re.search('-f(.*)\.',str(path)).group(1))
    return fps, resolution

thread_list = [1,2,4,8,12,16,20,24,28,32,36,40,44,48]
quality_list = [20,40,60,80,100]
thread_list.reverse()
quality_list.reverse()

for quality in quality_list:
    for raw_name in raw_list:
        fps, resolution = parser_path(raw_name)
        dir_path = "./result/"+raw_name.split(".raw")[0]

        output_path = dir_path + '/time_t' + 'opencl' + '_q' + str(quality) + '_'  + '.out'
        if os.path.exists(output_path):
            os.remove(output_path)

        os.makedirs(dir_path,exist_ok=True)
        raw_path = "../test_set/" + raw_name
        for i in range(0, num):
            cmd = '/usr/bin/time ../mjpeg_encoder -i '+ raw_path + ' -s ' + resolution + ' -r ' + fps + ' -o ./test_' + raw_name.split(".raw")[0] + '.avi -k "opencl" -T "./tmp_jpegs_mp/" -t 1 -q ' + str(quality) + ' -d gpu 2>> ' + dir_path + '/time_t' + 'opencl' + '_q' + str(quality) + '_'  + '.out' 
            print(cmd)
            subprocess.call(cmd, shell=True)