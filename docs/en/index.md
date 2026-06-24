---
layout: home

hero:
  name: catter
  text: Build Process Interception Tool
  tagline: Capture, analyze, and modify C++ build commands across any build system
  actions:
    - theme: brand
      text: What is catter?
      link: ./guide/what-is-catter
    - theme: alt
      text: Quick Start
      link: ./guide/quick-start

features:
  - icon: 🔍
    title: Universal CDB Generation
    details: Generate compile_commands.json from any build system — Make, Ninja, CMake, or custom scripts
  - icon: 🪝
    title: Deep Process Interception
    details: Hooks into process creation at the OS level (LD_PRELOAD on Unix, DLL injection on Windows)
  - icon: 📜
    title: JavaScript Scripting
    details: Write custom scripts with the embedded QuickJS engine to analyze, filter, or modify build commands
  - icon: 🌳
    title: Build Graph Analysis
    details: Visualize command trees and target dependencies to understand your build structure
---
