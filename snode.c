#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <slurm/slurm.h>

#define BUFF_LEN_A 128
#define BUFF_LEN_B 1024

void slurm_xfree(void **);

void load_str(uint32_t cpu_load, uint32_t cpus, char *buff){
    double cpu_load_f = (double)cpu_load / 100;
    double factor = cpu_load_f / cpus;
    if ((int32_t)cpu_load < 0) /* node down or error */
        sprintf(buff, "%10s", "N/A");
    else if (factor > 1.2)
        sprintf(buff, "\e[31m\e[1m%7.2f(!)\e[0m", cpu_load_f);
    else if (factor > 0.75)
        sprintf(buff, "\e[33m%10.2f\e[0m", cpu_load_f);
    else
        sprintf(buff, "%10.2f", cpu_load_f);
}

void cpu_str(int cpu_alloc, int cpu_config, char *buff){
    sprintf(buff, "%d/%d", cpu_alloc, cpu_config);
}

void mem_str(int mem_alloc, int mem_config, char *buff){
    sprintf(buff, "%d/%dG", mem_alloc / 1024, mem_config / 1024);
}

void gpu_str(int gpu_alloc, int gpu_config, char *buff){
    if (gpu_config == -1) /* no gpus */
        sprintf(buff, "---");
    else
        sprintf(buff, "%d/%d", gpu_alloc, gpu_config);
}

void time_left_str(time_t t_left, char *buff){
    uint32_t dhms[4];
    uint32_t divider[] = {60, 60, 24};

    if (t_left < 0){
        sprintf(buff, "---");
        return;
    }

    for (int i = 0; i < 3; ++i){
        dhms[i] = t_left % divider[i];
        t_left /= divider[i];
    }
    dhms[3] = t_left;

    int ipos = 3;
    while (dhms[ipos] == 0) --ipos;

    switch (ipos){
        case 3:
            sprintf(buff, "%u-%02u:%02u:%02u", dhms[3], dhms[2], dhms[1], dhms[0]);
            break;
        case 2:
            sprintf(buff, "%u:%02u:%02u", dhms[2], dhms[1], dhms[0]);
            break;
        case 1:
            sprintf(buff, "%u:%02u", dhms[1], dhms[0]);
            break;
        case 0:
            sprintf(buff, "0:%02u", dhms[0]);
            break;
        default:
            sprintf(buff, "---");
            break;
    }
}

void job_str(char *node_name, job_info_msg_t *jobs, char *buff){
    char local_buff[BUFF_LEN_A];
    for (int i = 0; i < jobs->record_count; ++i){
        slurm_job_info_t *pinfo = jobs->job_array + i;
        int cpu_cnt = slurm_job_cpus_allocated_on_node(pinfo->job_resrcs, node_name);

        if (cpu_cnt > 0){
            /* calculate time left */
            time_t now = time(NULL);
            time_t elapsed = now - pinfo->start_time;
            time_t left = pinfo->time_limit * 60 - elapsed;
            char time_left_buff[BUFF_LEN_A];
            time_left_str(left, time_left_buff);

            sprintf(local_buff, "[%d %s %s] ", pinfo->job_id, pinfo->user_name, time_left_buff);
            strcat(buff, local_buff);
        }
    }
}

void get_alloc_tres(void *data, int *cpus, int *mem, int *gpus){
    char *buff = (char*)data;
    char unit;
    /* check whether we have gpu */
    if (strstr(buff, "gpu")){
        sscanf(buff, "cpu=%d,mem=%d%c,gres/gpu=%d", cpus, mem, &unit, gpus);
    } else {
        sscanf(buff, "cpu=%d,mem=%d%c", cpus, mem, &unit);
        *gpus = 0; /* no gpus on this node */
    }
    if (unit == 'G') *mem *= 1024; /* default unit is MiB */
}

int get_conf_gpus(char *data){
    int ret = -1;
    char *sub;
    if (data == NULL)
        return ret;
    if ((sub = strstr(data, "gpu")) != NULL){
         sscanf(sub, "gpu=%d", &ret);
    }
    return ret;
}

int main(){
    int info;
    node_info_msg_t *node_info_msg_ptr;
    job_info_msg_t *job_info_msg_ptr;
    void *alloc_tres_buffer;
    char buff_load[BUFF_LEN_A], buff_cpu[BUFF_LEN_A],
         buff_mem[BUFF_LEN_A], buff_gpu[BUFF_LEN_A], buff_job[BUFF_LEN_B];

    info = slurm_load_node((time_t)NULL, &node_info_msg_ptr, SHOW_ALL);
    if (info != 0){
        fprintf(stderr, "Slurm load node status failed (exit code %d).\n", info);
        return -1;
    }

    info = slurm_load_jobs((time_t)NULL, &job_info_msg_ptr, SHOW_DETAIL);
    if (info != 0){
        fprintf(stderr, "Slurm load job status failed (exit code %d).\n", info);
        return -2;
    }

    /* header */
    printf("\e[1m%8s%10s%10s%12s%10s    %s\e[0m\n", "NODE", "CPU", "LOAD", "MEM", "GPU", "JOB [ID USER TIME_LEFT]");
    for (int i = 0; i < 80; ++i) printf("=");
    printf("\n");

    /* show each node line by line */
    for (int i = 0; i < node_info_msg_ptr->record_count; ++i){
        node_info_t *pnode = node_info_msg_ptr->node_array + i;
        int alloc_cpu = 0, alloc_mem = 0, alloc_gpu = 0;
        info = slurm_get_select_nodeinfo(pnode->select_nodeinfo,
                 SELECT_NODEDATA_TRES_ALLOC_FMT_STR,
                 NODE_STATE_ALLOCATED, &alloc_tres_buffer);
        if (info != 0){
            fprintf(stderr, "Failed to get select nodeinfo for %s (exit code %d).\n", pnode->name, info);
            continue;
        }

        if (alloc_tres_buffer != NULL)
            get_alloc_tres(alloc_tres_buffer, &alloc_cpu, &alloc_mem, &alloc_gpu);

        /* draw cpu, load, mem, gpu, jobs */
        buff_job[0] = '\0';
        load_str(pnode->cpu_load, pnode->cpus, buff_load);
        cpu_str(alloc_cpu, pnode->cpus, buff_cpu);
        mem_str(alloc_mem, pnode->real_memory, buff_mem);
        gpu_str(alloc_gpu, get_conf_gpus(pnode->tres_fmt_str), buff_gpu);
        job_str(pnode->name, job_info_msg_ptr, buff_job);
        printf("%8s%10s%10s%12s%10s    %s\n",
                pnode->name,
                buff_cpu,
                buff_load,
                buff_mem,
                buff_gpu,
                buff_job
              );
        slurm_xfree(&alloc_tres_buffer);
    }
    slurm_free_node_info_msg(node_info_msg_ptr);
    slurm_free_job_info_msg(job_info_msg_ptr);

    return 0;
}
