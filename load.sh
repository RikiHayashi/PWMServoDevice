sudo rmmod pwmservo
make
sudo insmod pwmservo.ko
sudo chmod 666 /dev/pwmservo0
