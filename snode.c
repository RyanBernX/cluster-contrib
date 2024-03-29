#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <slurm/slurm.h>

#define BUFF_LEN_A 128
#define BUFF_LEN_B 1024

void slurm_xfree(void **);

void node_str(uint32_t state, const char* name, char *buff){
    uint32_t state_base, state_flag;

    /* obtain state_lo, state_hi */
    state_base = state & NODE_STATE_BASE;
    state_flag = state & NODE_STATE_FLAGS;

    if (state_flag & NODE_STATE_NO_RESPOND){
        sprintf(buff, "**%s", name);
    } else if (state_flag & NODE_STATE_DRAIN){
        if (state_base > NODE_STATE_DOWN && state_base < NODE_STATE_FUTURE){
            sprintf(buff, "-%s", name);
        } else if (state_base == NODE_STATE_DOWN){
            sprintf(buff, "*%s", name);
        }
    } else if (state_flag & NODE_STATE_RES){
        sprintf(buff, "#%s", name);
    } else {
        if (state_base == NODE_STATE_DOWN){
            sprintf(buff, "*%s", name);
        } else {
            sprintf(buff, "%s", name);
        }
    }
}

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

        if (pinfo->job_state == JOB_RUNNING && cpu_cnt > 0){
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
    /* parse tres data */
    char *cpu_str = strstr(buff, "cpu");
    char *mem_str = strstr(buff, "mem");
    char *gpu_str = strstr(buff, "gpu");


    if (cpu_str){
        sscanf(cpu_str, "cpu=%d", cpus);
    } else {
        *cpus = 0;
    }

    if (mem_str){
        double dmem = 0;
        char *endptr;
        dmem = strtod(mem_str+4, &endptr); // mem=xxxx
        unit = *endptr;
        if (unit == 'G') dmem *= 1024; /* default unit is MiB */
        *mem = (int)dmem;
    } else {
        *mem = 0;
    }

    if (gpu_str){
        sscanf(gpu_str, "gpu=%d", gpus);
    } else {
        *gpus = 0; /* no gpus on this node */
    }
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
    char buff_node[BUFF_LEN_A], buff_load[BUFF_LEN_A], buff_cpu[BUFF_LEN_A],
         buff_mem[BUFF_LEN_A], buff_gpu[BUFF_LEN_A], buff_job[BUFF_LEN_B];

#if SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20,11,0)
    slurm_init(NULL);
#endif

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
    printf("\e[1m%10s%10s%10s%12s%10s    %s\e[0m\n", "NODE", "CPU", "LOAD", "MEM", "GPU", "JOB [ID USER TIME_LEFT]");
    for (int i = 0; i < 80; ++i) printf("=");
    printf("\n");

    /* show each node line by line */
    for (int i = 0; i < node_info_msg_ptr->record_count; ++i){
        node_info_t *pnode = node_info_msg_ptr->node_array + i;
        int alloc_cpu = 0, alloc_mem = 0, alloc_gpu = 0;
        void *alloc_tres_buffer = NULL;
        info = slurm_get_select_nodeinfo(pnode->select_nodeinfo,
                 SELECT_NODEDATA_TRES_ALLOC_FMT_STR,
                 NODE_STATE_ALLOCATED, &alloc_tres_buffer);
        if (info != 0){
            fprintf(stderr, "Failed to get select nodeinfo for %s (exit code %d).\n", pnode->name, info);
            continue;
        }

        if (alloc_tres_buffer != NULL){
            get_alloc_tres(alloc_tres_buffer, &alloc_cpu, &alloc_mem, &alloc_gpu);
        }

        /* draw cpu, load, mem, gpu, jobs */
        buff_job[0] = '\0';
        node_str(pnode->node_state, pnode->name, buff_node);
        load_str(pnode->cpu_load, pnode->cpus, buff_load);
        cpu_str(alloc_cpu, pnode->cpus, buff_cpu);
        mem_str(alloc_mem, pnode->real_memory, buff_mem);
        gpu_str(alloc_gpu, get_conf_gpus(pnode->tres_fmt_str), buff_gpu);
        job_str(pnode->name, job_info_msg_ptr, buff_job);
        printf("%10s%10s%10s%12s%10s    %s\n",
                buff_node,
                buff_cpu,
                buff_load,
                buff_mem,
                buff_gpu,
                buff_job
              );
        slurm_xfree(&alloc_tres_buffer);
    }

    /* hline */
    for (int i = 0; i < 80; ++i) printf("=");
    printf("\n");

    /* legend */
    printf("  NODE LEGEND\n");
    printf("     -  draining/drained (not allocating new jobs)\n");
#ifndef CL_NO_RESV
    printf("     #  reserved for teaching\n");
#endif
    printf("     *  down\n");
    printf("    **  poweroff/not responding\n");

    slurm_free_node_info_msg(node_info_msg_ptr);
    slurm_free_job_info_msg(job_info_msg_ptr);

    return 0;
}
