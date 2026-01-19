**对于Kconfig的使用流程**

1.先用 `idf.py set-target esp32-c3` 进行选型;

2.再用 `idf.py menuconfig` 进行Kconfig勾选;

3.最后再进行 `idf.py build flash monitor` 。