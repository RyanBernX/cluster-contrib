#!/usr/bin/ruby

require 'getoptlong'
require 'fileutils'

MATLAB = "matlab"
SBATCH = "sbatch"
VALID_PARTITIONS = ["cpu", "gpu"]
VALID_QOS = ["debug", "normal"]

opts = GetoptLong.new(
  [ '--help', '-h', GetoptLong::NO_ARGUMENT ],
  [ '--cpus', '-c', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--walltime', '-t', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--out', '-o', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--partition', '-p', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--qos', '-q', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--gpus', '-g', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--tmp-dir', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--name', '-N', GetoptLong::REQUIRED_ARGUMENT ],
  [ '--sargs', GetoptLong::REQUIRED_ARGUMENT ]
)

cpus = 1
gpus = 0
walltime = '01:00:00'
out = ""
partition = "cpu"
qos = "normal"
dirname = ""
jobname = ""
sargs = ""



def slurm_template(name, partition, qos, cpu, gpu, time, out, ver)
  ret = %{#!/bin/bash
#SBATCH -J #{name}
#SBATCH -p #{partition}
#SBATCH -N 1
#SBATCH --cpus-per-task=#{cpu}
#SBATCH -t #{time}
#SBATCH --qos=#{qos}
#SBATCH -o #{out}
#{gpu == 0 ? "" : "#SBATCH --gres=gpu:#{gpu}"}

module add matlab#{ver.empty? ? "" : "/" + ver[0][0]}
}
end

opts.each do |opt, arg|
  case opt
    when '--help'
      puts <<-EOF
matpbs.rb [OPTION] ... FILENAME

OPTIONS:

-h, --help:
   Show help

-c CPUS, --cpus=CPUS:
   The number of cores/cpus you request.
   Default: 1

--gpus=GPUS:
   The number of GPU cards you request. Ignored when --partition=cpu.
   Default: 0

-t TIME, --walltime=TIME:
   The wallclock time you request. Format HH:MM:SS.
   Default: 01:00:00

-p PARTITION, --partition=PARTITION
   The slurm partition you request, must be 'cpu' or 'gpu'.
   Default: cpu

-q qos, --queue=qos
   The QoS you'd like to use, must be 'debug' or 'normal'.
   Default: normal

-o OUTPUT, --out=OUTPUT
   The file that stores all standard outputs.
   Default: $script_name.txt (in tmp_dir)

--tmp-dir=DIR
   the directory to store intermediate files (log and err files)

-N JOBNAME, --name=JOBNAME
   Custom SLURM job name.
   Default: the name of your MATLAB script

--sargs='args'
   Additional arguments which will be passed directly to
   sbatch. sargs has higher priority. It will overwrite the
   operands of other options. For example, if one uses
      matslm.rb --sargs='-J ts11' -N ts test.m
   then the job name will be 'ts11' instead of 'ts'.

FILENAME: The script you'd like to run.

      EOF
      exit(0)
    when '--cpus'
      cpus = arg.to_i
    when '--gpus'
      gpus = arg.to_i
    when '--walltime'
      walltime = arg
    when '--out'
      out = arg
    when '--partition'
      partition = arg
    when '--qos'
      qos = arg
    when '--tmp-dir'
      dirname = arg
    when '--name'
      jobname = arg
    when '--sargs'
      sargs = arg
  end
end

if ARGV.length != 1
  puts "Please tell me which script you'd like to run (try --help or man matslm.rb)"
  exit 0
end

script = ARGV.shift
script_no_suffix = File.basename(script, ".m")
script_dir = File.dirname(script)


# parse time
hms = walltime.split(":").map{|t| t.to_i}
total_sec = 3600 * hms[0] + 60 * hms[1] + hms[2]

# check partition and qos
unless VALID_PARTITIONS.include?(partition)
  puts "Invalid partition specified. Expected cpu or gpu (got #{partition})."
  exit(1)
end

unless VALID_QOS.include?(qos)
  puts "Invalid QoS specified. Expected debug or normal (got #{qos})."
  exit(1)
end

# check gpus
if partition == "cpu"
  puts "Warning. Ignoring --gpus=#{gpus} in partition cpu." if gpus > 0
  gpus = 0
else
  if gpus == 0
    puts "It seems that you didn't request gpus on partition gpu."
    puts "Hope you are OK with that."
  else
    gpus = [2, [gpus, 1].max].min
    puts "Warning. gpus is set to #{gpus}."
  end
end

# check cpus
max_cpu = partition == "cpu" ? 48 : 56
if cpus < 1 || cpus > max_cpu
  cpus = [max_cpu, [cpus, 1].max].min
  puts "Warning. cpus is set to #{cpus}."
end


t = Time.now
time_s = t.strftime("%Y-%m-%d-%H-%M-%S")

if dirname == ""
  dirname = "SLURM_#{time_s}_MATLAB_#{script_no_suffix}"
end

# make target directory
unless FileTest.exist?(dirname)
  FileUtils.mkdir_p(dirname)
end

# set output
if out == ""
  out = "slurm-%A.out"
else
  outdir = File.dirname(out)
  FileUtils.mkdir_p(outdir) unless File.exist? outdir
  out = File.expand_path(out)
end

# check the modules currently loaded
# determine the version of matlab
ret = %x( module list 2>&1 )
ver = ret.scan(/\d+\) matlab\/(\S+)/)

pwd = Dir.pwd

Dir.chdir(dirname) do
  slurm_filename = "run_#{script_no_suffix}.slurm"
  puts "Output files and log files can be found in #{dirname}"
  File.open(slurm_filename, "w") do |f|
    # load the matlab module
    jobname = script_no_suffix if jobname == ""
    f.write slurm_template(jobname, partition, qos, cpus, gpus, walltime, out, ver)
    # run matlab script
    f.write("cd #{pwd}\n")
    f.write("cd #{script_dir}\n")
    f.write("#{MATLAB} -nodesktop -nodisplay -nosplash -r \'#{script_no_suffix}\'\n")
  end
  ret = %x( #{SBATCH} #{sargs} #{slurm_filename} )
  puts ret
end


