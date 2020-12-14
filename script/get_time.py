import os
import numpy as np
import csv
ll = [1,2,4,8,12,16,20,24]

def get_data(two_lines, file_path):
    try:
        line = two_lines[0]
        user = line.split("user")[0]
        system = line.split("system")[0].split()[1]
        elapsed = line.split("elapsed")[0].split()[2].replace("0:","")
        return float(user),float(system),float(elapsed)
    except:
        print(file_path)
        return -1,-1,-1

def get_data_from_lines(lines, file_path):
    num = len(lines)/2
    user_list = []
    system_list = []
    elapsed_list = []
    for i in range(0,len(lines),2):
        user,system,elapsed = get_data(lines[i:i+2], file_path)
        if(user == -1):
            print(len(lines))
            print(lines[0:5])
            return -1,-1,-1,-1
            
        user_list.append(user)
        system_list.append(system)
        elapsed_list.append(elapsed)
    user_arr = np.asarray(user_list)
    system_arr = np.asarray(system_list)
    elapsed_arr = np.asarray(elapsed_list)
    return np.mean(user_arr),np.mean(system_arr),np.mean(elapsed_arr),num

files = os.listdir("../test_set")
print(files)

raw_list = []
for file_path in files:
    if(file_path.endswith(".raw")):
        raw_list.append(file_path)

thread_list = [1,2,4,8,12,16,20,24,28,32,36,40,44,48,"serial","opencl"]
quality_list = [20,40,60,80,100]

with open('values.csv', 'w', newline='') as csvfile:
    fieldnames = ['thread_number', 'quality', 'file_path', 'real', 'user', 'system', 'num']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for raw_name in raw_list:
        for thread_num in thread_list:
            for quality in quality_list:
                dir_path = "./result/"+raw_name.split(".raw")[0]
                s = os.path.join(dir_path, "time_t" +str(thread_num) + "_q" + str(quality) + "_.out")
                f = open(s,'r')
                lines = f.readlines()
                user_avg,system_avg,elapsed_avg,num = get_data_from_lines(lines, s)
                print("file path is"+str(s))
                # print("real: "+ str(elapsed_avg))
                # print("user: "+ str(user_avg))
                # print("system: "+ str(system_avg))
                # print(num)
                # print("-----")

                writer.writerow({'thread_number':thread_num, 'quality':quality,'file_path':s, 'real': elapsed_avg, 'user': user_avg, 'system':system_avg, 'num':num})


with open('values2.csv', 'w', newline='') as csvfile:
    fieldnames = ['thread_number', 'quality', 'file_path', 'real', 'user', 'system', 'num']
    writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    writer.writeheader()
    for raw_name in raw_list:
        for quality in quality_list:
            for thread_num in thread_list:
                dir_path = "./result/"+raw_name.split(".raw")[0]
                s = os.path.join(dir_path, "time_t" +str(thread_num) + "_q" + str(quality) + "_.out")
                f = open(s,'r')
                lines = f.readlines()
                user_avg,system_avg,elapsed_avg,num = get_data_from_lines(lines, s)
                # print("file path is"+str(s))
                # print("real: "+ str(elapsed_avg))
                # print("user: "+ str(user_avg))
                # print("system: "+ str(system_avg))
                # print(num)
                # print("-----")

                writer.writerow({'thread_number':thread_num, 'quality':quality,'file_path':s, 'real': elapsed_avg, 'user': user_avg, 'system':system_avg, 'num':num})

# s = "time_serial.out"
# f = open(s,'r')
# lines = f.readlines()
# user_avg,system_avg,elapsed_avg,num = get_data_from_lines(lines)
# print(s)
# print("real: "+ str(elapsed_avg))
# print("user: "+ str(user_avg))
# print("system: "+ str(system_avg))
# print(num)
# print("-----")
