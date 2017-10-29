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
./benchmark/benchmark 16 8192 1
cat *.log > trace
sort -n -k 3 trace > sorted_trace
./benchmark/validate 16 8192 < sorted_trace
rm -f *.log
sudo rmmod tnpheap
sudo rmmod npheap

