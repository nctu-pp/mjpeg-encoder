import subprocess
import os
import re
num = 3

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
    print(fps, resolution)
    return fps, resolution

thread_list = [1,2,4,8,12,16,20,24,28,32,36,40,44,48]
thread_list.reverse()

num = 20
for thread_num in thread_list:
    for raw_name in raw_list:
            fps, resolution = parser_path(raw_name)
            dir_path = "./test/"+raw_name.split(".raw")[0]
            os.makedirs(dir_path,exist_ok=True)
            raw_path = "../test_set/" + raw_name

            output_path =  dir_path + '/time_tffmjpeg_' + str(thread_num) + '.out' 
            if os.path.exists(output_path):
                os.remove(output_path)

            for i in range(0,num):
                cmd = "/usr/bin/time ffmpeg -f rawvideo -pixel_format bgra -video_size " + str(resolution) + " -framerate "+ str(fps) + " -i " + raw_path + " -c:v mjpeg -huffman default -pix_fmt yuvj444p -an -q:v 2 -threads "+ str(thread_num) + " output.avi 2>> " + dir_path + "/time_tffmjpeg_" + str(thread_num) + ".out -y"
                print(cmd)
                subprocess.call(cmd, shell=True)
'''
ffmpeg  -f rawvideo -pixel_format bgra -video_size 1920x1080 -framerate 20 -i ./test_set/repeat105M6oB2ZNnLc-1920x1080-f15.raw -c:v mjpeg -huffman default -pix_fmt yuvj444p -an -q:v 2 -threads 8 output.avi
'''


