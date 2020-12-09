import os
import numpy as np
ll = [1,2,4,8,12,16,20,24]

def get_data(two_lines):
    line = two_lines[0]
    user = line.split("user")[0]
    system = line.split("system")[0].split()[1]
    elapsed = line.split("elapsed")[0].split()[2].replace("0:","")
    return float(user),float(system),float(elapsed)

def get_data_from_lines(lines):
    num = len(lines)/2
    user_list = []
    system_list = []
    elapsed_list = []
    for i in range(0,len(lines),2):
        user,system,elapsed = get_data(lines[i:i+2])
        user_list.append(user)
        system_list.append(system)
        elapsed_list.append(elapsed)
    user_arr = np.asarray(user_list)
    system_arr = np.asarray(system_list)
    elapsed_arr = np.asarray(elapsed_list)
    return np.mean(user_arr),np.mean(system_arr),np.mean(elapsed_arr),num

for l in ll:
    s = "time" + str(l) + ".out"
    f = open(s,'r')
    lines = f.readlines()
    user_avg,system_avg,elapsed_avg,num = get_data_from_lines(lines)
    print("thread number is"+str(l))
    print("real: "+ str(elapsed_avg))
    print("user: "+ str(user_avg))
    print("system: "+ str(system_avg))
    print(num)
    print("-----")

s = "time_serial.out"
f = open(s,'r')
lines = f.readlines()
user_avg,system_avg,elapsed_avg,num = get_data_from_lines(lines)
print(s)
print("real: "+ str(elapsed_avg))
print("user: "+ str(user_avg))
print("system: "+ str(system_avg))
print(num)
print("-----")
