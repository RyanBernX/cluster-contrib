# 集群辅助管理脚本

开发者维护一个列表，简要说明每个文件的作用。更新的时候
填写更新日志。

## 主要内容
- `snode.bak` by @liuhy: 按节点查看 SLURM 状态，包含节点任务，负载，资源占用等情况。
  已经被 `snode.c`（C 语言版本）替代。
- `sjob` by @zlong: 按 job 查看 SLURM 状态，实际是 `squeue` 一些常用参数的组合。
- `matslm.rb` by @liuhy: 快速生成 SLURM 脚本并提交 MATLAB 任务。实际上是给不愿
学习 SLURM 脚本的纯 MATLAB 用户提供方便。
- `ssh-auto-keygen.sh` by @sugon: 首次登录自动配置普通用户在计算节点间的 SSH
无密码登录。
