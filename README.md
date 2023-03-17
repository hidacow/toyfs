# toyfs

一个极其简单的演示性文件系统，可供操作系统课程实验、学习使用

## 功能

- 仿Linux的无环图目录结构
- 简易Shell与命令处理
- 绝对、相对路径解析
- 简易用户权限（用户可以以文件夹身份登录，访问上级需`sudo`）、文件`rwx`权限
- 软链接、硬链接
- 递归删除目录
- 其它基本操作命令：
  - ls
  - open
  - close
  - create
  - delete
  - read
  - write
  - exec
  - chmod
  - mkdir
  - cd
  - tree
  - pwd
  - ln
  - whoami
  - exit
  - logout

## Get Started

使用支持C++14的编译器编译即可，内部含有非ASCII字符，可能在某些运行环境和编译器下造成乱码

## ~~TODO~~

- rename
- move

## License

MIT