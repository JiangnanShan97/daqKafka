#### 简介：
此模块用于LinuxCNC系统的数据采集，通过引脚输出数据采集卡模拟输入通道的电压值，包含两部分：
(1)HAL模块部分，由源文件“daq.c”构成；
(2)数据采集模块部分，由源文件“usb_4711a.c”构成，可以根据数据采集卡型号添加新的源文件。

#### HAL模块介绍：
1、引脚
(1)daq.analog-in.%d    float类型，输出引脚，以电压的方式输出第%d个模拟输入通道的采集值，在加载HAL模块时设置采集的模拟输入通道数。
(2)daq.analog-in-val.%d    float类型，输出引脚，以物理量的方式输出第%d个模拟输入通道的采集值，analog-in-val=analog-in×scale-in。
2、参数
(1)daq.gain-in.%d    s32类型，第%d个模拟输入通道的输入电压范围，参数与输入电压范围的对应关系与数据采集卡型号有关，见数据采集模块介绍。
(2)daq.type.%d    s32类型，第%d个模拟输入通道所在数据采集卡的型号，目前支持型号及其对应参数如下：
-----------------------------------------------
    参数      数据采集卡型号
-----------------------------------------------
    0        研华USB-4711A
-----------------------------------------------
(3)daq.channel.%d    s32类型，第%d个模拟输入通道在数据采集卡上的通道号。
(4)daq.read-mode.%d    s32类型，第%d个模拟输入通道的信号读取模式，参数与读取模式的对应关系与数据采集卡型号有关，见数据采集模块介绍。
(5)daq.scale-in.%d    float类型，第%d个模拟输入通道的信号放大比例，需根据该通道的电压与实际物理量数值关系设置，例如1V电压对应100N，就将这个值设置为100。
3、函数
(1)read.analog-in.all    读取全部模拟输入通道的信号。
(2)read.analog-in.%d    读取第%d个模拟输入通道的信号。

#### 数据采集模块介绍：
数据采集功能的具体实现，每种型号的数据采集卡对应一个数据采集模块源文件。
目前支持的数据采集卡型号如下：
1、研华USB-4711A
(1)double usb_4711a_read_a_chan(int channel, int gain, int read_mode)    读取数据采集卡一个模拟通道的信号，以电压值的形式返回，输入参数说明如下：
-----------------------------------------------
    输入参数     说明
-----------------------------------------------
    channel     数据采集卡模拟通道号
-----------------------------------------------
    gain        数据采集卡模拟通道的输入电压范围
                0 表示-10～10V
                1 表示0~10V
                2 表示-5~5V
                3 表示0~5V
-----------------------------------------------
    read_mode   数据采集卡模拟通道信号读取模式
                0 表示普通模式
                1 表示差分模式
-----------------------------------------------

#### 使用说明：
1、编译方法
复制"linuxcnc-xxx/src/Makefile.modinc"到此模块源代码目录，编译：
> make install
编译生成的模块自动加入"linuxcnc-xxx/rtlib/daq.so"。
清理编译时产生的中间文件：
> make clean
2、加载和使用
“run_daq.hal”为测试运行时使用的HAL文件，集成到系统中还需要修改数控系统的“*.hal”文件。
(1)在“*.hal”文件中加载此模块，指令如下：
loadrt daq num_a_chans=n    (n为需要采集的模拟通道数)
(2)在“*.hal”文件中用setp设置采集参数。
(3)在“*.hal”文件中用addf将函数加入线程中，只采集某些通道用函数“read.analog-in.%d”，采集所有通道用函数“read.analog-in.all”。
(4)在“*.hal”文件中将输出引脚“daq.analog-in.%d”连接到需要数据的模块引脚上。


