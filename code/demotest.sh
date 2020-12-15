
#rebuild all
cd build.linux
make clean
make

#test Part1
cd ../test
echo "***************************************"
cp /home/os2020/share/hw3t{1,2,3}.c ./
cat /home/os2020/share/hw3Makefile >> Makefile
make hw3t1 hw3t2 hw3t3
../build.linux/nachos -ep hw3t1 0 -ep hw3t2 0
../build.linux/nachos -ep hw3t1 50 -ep hw3t2 50
../build.linux/nachos -ep hw3t1 50 -ep hw3t2 90
../build.linux/nachos -ep hw3t1 100 -ep hw3t2 100
../build.linux/nachos -ep hw3t1 40 -ep hw3t2 55
../build.linux/nachos -ep hw3t1 40 -ep hw3t2 90
../build.linux/nachos -ep hw3t1 90 -ep hw3t2 100
../build.linux/nachos -ep hw3t1 60 -ep hw3t3 50

