# GCC 13.4 install

## 创建目录

```
mkdir -p $HOME/toolchains
cd $HOME/toolchains
```

## 下载 GCC 源码

```
wget https://ftp.gnu.org/gnu/gcc/gcc-13.4.0/gcc-13.4.0.tar.xz
tar xf gcc-13.4.0.tar.xz
cd gcc-13.4.0
```


## 下载依赖

```
./contrib/download_prerequisites
```

拉取 GCC 构建所需的若干依赖组件，例如 GMP、MPFR、MPC、ISL。这个流程就是 GCC 官方源码树里长期提供的标准做法。

## 单独建 build 目录

```
cd ..
mkdir gcc-13-build
cd gcc-13-build
```

## 配置 GCC

```
../gcc-13.4.0/configure \
  --prefix=$HOME/.local/gcc-13.4 \
  --disable-multilib \
  --enable-languages=c,c++
```


## 编译 GCC

```
make -j16 -l6
make install
```


## 设置环境变量

```
# ~/.bashrc

export GCC13_HOME=$HOME/.local/gcc-13.4
export PATH=$GCC13_HOME/bin:$PATH
export LD_LIBRARY_PATH=$GCC13_HOME/lib64:$LD_LIBRARY_PATH

source ~/.bashrc
```

## 验证版本

```
g++ --version
```

应该能看到 `g++ (GCC) 13.4.0` 一类输出。GCC 13.4 是 GNU 官方发布的实际版本。

## 验证 `std::format`

```
#include <format>
#include <iostream>
#include <string>

int main()
{
    std::string s = std::format("hello {}, {}", "dbase", 20);
    std::cout << s << '\n';
    return 0;
}
g++ -std=c++20 test_format.cpp -o test_format
./test_format
```
