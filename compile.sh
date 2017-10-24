sudo dmesg -C
sudo rmmod tnpheap
sudo rmmod npheap
cd kernel_module
sudo make clean
make
sudo make install
cd ../library
sudo make clean
make
sudo make install
cd ../benchmark
sudo make clean
sudo make benchmark
sudo make validate
cd ..
# change this to where your npheap is.
sudo insmod NPHeap/npheap.ko
sudo chmod 777 /dev/npheap
sudo insmod kernel_module/tnpheap.ko
sudo chmod 777 /dev/tnpheap
./benchmark/benchmark 1 8192 1

