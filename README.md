# Auxiliary scripts for cluster management

## Contents
- `snode.c`: query SLURM cluster status, one node per line (load, allocated
  resources, active jobs, etc)
- `sjob` by @ZichaoLong: query SLURM jobs (a wrapper for `squeue`)
- `matslm.rb`: generate SLURM scripts and submit MATLAB jobs.
- `ssh-auto-keygen.sh`: configure SSH public key authentication among the cluster
  nodes (normal users)

## Usage

### `snode.c`
Edit `Makefile`: set `SLURM_ROOT` to your SLURM installation path.
Then type `make` to compile.

You'll see `snode` in the current directory. Copy it to somewhere you prefer
(typically `/usr/local/lib`).

**Note**: if SLURM is upgraded, `snode.c` must be compiled again.

### `sjob`
See `sjob -h`.

### `matslm.rb`
Install ruby (probably with `apt` or `yum`). Then type `matslm.rb -h` for help.

### `ssh-auto-keygen.sh`
The script assumes the home directory is shared among the
nodes via some network filesystem (or lustre, gpfs).

Just put the script under `/etc/profile.d/`.

