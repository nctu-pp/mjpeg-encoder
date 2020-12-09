import subprocess
num = 100
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 1 2>> time1.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 2 2>> time2.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 4 2>> time4.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 8 2>> time8.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 12 2>> time12.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 16 2>> time16.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 20 2>> time20.out'
	subprocess.call(cmd, shell=True)
for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test_openmp.avi -k "openmp" -T "./tmp_jpegs_mp/" -t 24 2>> time24.out'
	subprocess.call(cmd, shell=True)

for i in range(0,num):
	cmd = '/usr/bin/time ./mjpeg_encoder -i enAd0wfp_GE.f137.raw -s 854x480 -r 60 -o ./test2.avi -k 0 2>> time_serial.out'
	subprocess.call(cmd, shell=True)
