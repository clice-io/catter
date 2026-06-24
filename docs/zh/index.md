---
layout: home

hero:
  name: catter
  text: 构建过程拦截工具
  tagline: 跨构建系统捕获、分析和修改 C++ 编译命令
  actions:
    - theme: brand
      text: 什么是 catter？
      link: ./guide/what-is-catter
    - theme: alt
      text: 快速开始
      link: ./guide/quick-start

features:
  - icon: 🔍
    title: 通用编译数据库生成
    details: 从任何构建系统生成 compile_commands.json — Make、Ninja、CMake 或自定义脚本
  - icon: 🪝
    title: 深度进程拦截
    details: 在操作系统层面拦截进程创建（Unix 上使用 LD_PRELOAD，Windows 上使用 DLL 注入）
  - icon: 📜
    title: JavaScript 脚本
    details: 使用内嵌的 QuickJS 引擎编写自定义脚本来分析、过滤或修改构建命令
  - icon: 🌳
    title: 构建图分析
    details: 可视化命令树和目标依赖关系，帮助理解构建结构
---
