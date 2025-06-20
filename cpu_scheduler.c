#include <stdio.h>
#include <stdlib.h>

#define MAX_PROC 10

typedef enum {FCFS=0, RR=1, PRIOR=2} ALG;

typedef struct PCB {
    int pid;
    int base_prio, dyn_prio;
    int arrival_time, burst_time, remaining_time;
    int start_time, finish_time;
    int responded;
    int waiting_time, turnaround_time;
    int timeslice;
    struct PCB *next;
} PCB;

void enqueue(PCB **head, PCB *p){
  p->next = NULL;
  if(!*head){
    *head = p;
  }else{
    PCB *cur = *head;
    while(cur->next) cur = cur->next;
    cur->next = p;
  }
}

PCB* dequeue_head(PCB **head) {
  if (!*head) return NULL;
  PCB *p = *head;
  *head = p->next;
  p->next = NULL;
  return p;
}

void dequeue_target(PCB **head, PCB *t) {
  if (!*head) return;
  if (*head == t) {
    *head = t->next;
  }else{
    PCB *cur = *head;
    while(cur->next && cur->next != t)
      cur = cur->next;
    if(cur->next)
      cur->next = t->next;
  }
  t->next = NULL;
}

void aging_update(PCB *head, double alpha){
  for(PCB *p = head; p; p = p->next){
    p->waiting_time++;
    p->dyn_prio = p->base_prio + (int)(alpha * p->waiting_time);
  }
}

PCB* select_process(ALG algo, PCB **head) {
  if(!*head) return NULL;
  if(algo == FCFS || algo == RR){
    return dequeue_head(head);
  }else{
    PCB *best = *head, *cur = *head;
    while(cur){
      if(cur->dyn_prio > best->dyn_prio)
        best = cur;
      cur = cur->next;
    }
    dequeue_target(head, best);
    return best;
  }
}

const char* alg_name(ALG a){
  return (a==FCFS) ? "FCFS"
       : (a==RR) ? "RR"
                 : "Preemptive Priority";
}

void simulate(PCB jobs[], int n, ALG algo, int time_quantum, double alpha, FILE *out){
  for(int i=0; i<n; i++){
    jobs[i].remaining_time = jobs[i].burst_time;
    jobs[i].dyn_prio = jobs[i].base_prio;
    jobs[i].responded = 0;
    jobs[i].waiting_time = 0;
    jobs[i].timeslice = 0;
    jobs[i].start_time = -1;
    jobs[i].finish_time = -1;
  }

  fprintf(out, "Scheduling: %s\n", alg_name(algo));
  fprintf(out, "==================================\n");

  PCB *ready = NULL;
  int completed = 0, t = 0, busy = 0;

  while(completed < n) {
    for(int i = 0; i < n; i++){
      if(jobs[i].finish_time == t){
        fprintf(out, "<time %d> process %d is finished\n", t, jobs[i].pid);
        fprintf(out, "-------------------------------- (Context-Switch)\n");
      }
    }
    for(int i = 0; i < n; i++){
      if(jobs[i].arrival_time == t){
        enqueue(&ready, &jobs[i]);
        fprintf(out, "<time %d> [new arrival] process %d\n", t, jobs[i].pid);
      }
    }
    PCB *cur = select_process(algo, &ready);

    if(cur) {
      if(!cur->responded){
        cur->start_time = t;
        cur->responded = 1;
      }
      fprintf(out, "<time %d> process %d is running\n", t, cur->pid);
      busy++;
      cur->remaining_time--;
      cur->timeslice++;

      if(cur->remaining_time == 0){
        cur->finish_time = t+1;
        completed++;
        cur->timeslice = 0;
      }else{
        if(algo==RR){
          if(cur->timeslice == time_quantum){
            cur->timeslice=0;
            enqueue(&ready, cur);
            fprintf(out, "------------------------------ (Context-Switch)\n");
          }else{
            cur->next = ready;
            ready = cur;
          }
        }
        else if(algo == FCFS){
          cur->next = ready;
          ready = cur;
        }
        else{
          cur->timeslice=0;
          enqueue(&ready, cur);
          fprintf(out, "------------------------------ (Context-Switch)\n");
        }
      }
    }else{
      fprintf(out, "<time %d> ---- system is idle ----\n", t);
    }

    if(algo==PRIOR)
      aging_update(ready, alpha);
    t++;
  }
  fprintf(out, "<time %d> all processes finish\n", t);
  fprintf(out, "===================================\n");

  double tot_wt = 0, tot_rt = 0, tot_tt = 0;
  for(int i=0; i<n; i++){
    jobs[i].turnaround_time = jobs[i].finish_time - jobs[i].arrival_time;
    jobs[i].waiting_time = jobs[i].turnaround_time - jobs[i].burst_time;
    double rt = jobs[i].start_time - jobs[i].arrival_time;
    tot_wt += jobs[i].waiting_time;
    tot_rt += rt;
    tot_tt += jobs[i].turnaround_time;
  }
  double util = (busy / (double)t) * 100.0;

  fprintf(out, "Average cpu usage       : %.2f %%\n", util);
  fprintf(out, "Average waiting time    : %.2f\n", tot_wt/n);
  fprintf(out, "Average response time   : %.2f\n", tot_rt/n);
  fprintf(out, "Average turnaround time : %.2f\n", tot_tt/n);
  fprintf(out, "\n");
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr,
            "Usage: %s input.dat output.txt <RR_quantum> <alpha>\n",
            argv[0]);
        return 1;
    }
    const char *infile  = argv[1];
    const char *outfile = argv[2];
    int    tq    = atoi(argv[3]);
    double alpha = atof(argv[4]);

    PCB jobs[MAX_PROC];
    int n = 0;

    // 입력 파일 열기
    FILE *fin = fopen(infile, "r");
    if (!fin) {
        perror("Error opening input file");
        return 1;
    }
    while (n < MAX_PROC &&
           fscanf(fin, "%d %d %d %d",
                  &jobs[n].pid,
                  &jobs[n].base_prio,
                  &jobs[n].arrival_time,
                  &jobs[n].burst_time) == 4) {
        n++;
    }
    fclose(fin);

    FILE *fout = fopen(outfile, "w");
    if (!fout) {
        perror("Error opening output file");
        return 1;
    }
    simulate(jobs, n, FCFS, tq, alpha, fout);
    simulate(jobs, n, RR, tq, alpha, fout);
    simulate(jobs, n, PRIOR, tq, alpha, fout);

    fclose(fout);
    return 0;
}