
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
../build.linux/nachos -ep test1 40 -ep test2 80

