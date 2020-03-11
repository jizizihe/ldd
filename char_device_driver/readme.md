### 执行流程：

#### 1、加载驱动并通过mknod创建设备节点

````bash
sudo insmod cdd.ko
sudo mknod /dev/cdd c 230 0
````

#### 2、读写

```bash
sudo chmod 666 /dev/cdd
sudo echo "hello world" > /dev/cdd
cat /dev/cdd   //读取到hello world
```

#### 3、多个设备multicdd

```bash
sudo mknod /dev/cdd0 c 230 0
sudo mknod /dev/cdd1 c 230 1
sudo mknod /dev/cdd2 c 230 2
                     ......
```

注：不能超过10个。