// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "processManager.h"
#include "interrupts.h"
#include "string.h"
#include "memoryManager.h"
#include "initProcess.h"
#include "pipes.h"
#include "stdio.h"

#define STACK_SIZE (1024 * 4)

static int pidCounter = 1;
static queueADT queue = NULL;
static int currentCycles;

// Datos de proceso inactivo
static pcb *idleProcessPCB = NULL;

// Datos del proceso actual
static pcb *currentProcessPCB = NULL;

// Cantidad de procesos listos
static int readyCount = 0;

// pids de interes
static int userlandPid = -1;
static int initPid = -1;

// Utilizado para otorgar al init la capacidad de limpiar estos procesos
static int sendTaskToInit(int pid);

// Inicializa el bloque de memoria
static pcb *initializeBlock(char *name, int foreground, int *fd);

// Inicializa el stack
static void initializeStack(void (*process)(int, char **), int argc, char **argv, void *rbp);

// Realizo una copia de los argumentos
static int cpyArgs(char **dest, char **from, int count);

// Devuelve los datos del proceso especificado
static pcb *getProcess(queueADT queue, int pid);

// Cambia el estado del proceso especificado. Devuelve el pid del proceso.
static int changeState(int pid, process_state newState);
static void terminateChildren(int pid);

char *stateToStr(process_state state);

// Proceso inactivo
void idleProcess(int argc, char **argv)
{
  while (1)
    _hlt();
}

// Comenzar scheduler
void initScheduler()
{
  queue = initQueue();
  if (queue == NULL)
  {
    return;
  }

  char *idleArgv[] = {"idle"};
  int emptyFd[2] = {0};
  startTask(idleProcess, 1, idleArgv, 0, emptyFd);

  char *initArgs[] = {"Init"};
  initPid = startTask(&init, 1, initArgs, 0, emptyFd);
  readyCount -= 1;
  idleProcessPCB = (pcb *)dequeue(queue);
}

queueADT getProcessQueue()
{
  return queue;
}

pcb *getCallingProcess()
{
  return currentProcessPCB;
}

// Libera la memoria de un proceso particular
void freeProcess(pcb *process)
{
  for (int i = 0; i < process->argc; i += 1)
  {
    free(process->argv[i]);
  }
  // Si fue terminado prematuramente
  if (semValue(process->semId) == 0)
    semPost(process->semId);

  free(process->argv);
  semClose(process->semId);
  if (process->fileDescriptors[1] != STDOUT_PIPENO)
  {
    pipePutchar(process->fileDescriptors[1], -1);
    pipeClose(process->fileDescriptors[1]);
  }
  if (process->fileDescriptors[0] != STDIN_PIPENO)
  {
    pipeClose(process->fileDescriptors[0]);
  }
  
  freeQueue(process->mallocList);

  pidCounter -= currentProcessPCB->pid == (pidCounter - 1);
  // Previamente se hizo malloc en el stack del proceso
  free((void *)((char *)process->rbp - STACK_SIZE + 1));
  free((void *)process);
}

// Programa una tarea a realizar
void *scheduleTask(void *currStackPointer)
{
  if (queue == NULL)
    return currStackPointer;
  // Se fija si hay un proceso actual en existencia. Si no lo hay no hay cambio de contexto.
  if (currentProcessPCB != NULL)
  {
    // Intento correrlo y descuento la cantidad de ciclos
    if (currentProcessPCB->state == READY && currentCycles > 0)
    {
      currentCycles -= 1;
      return currStackPointer;
    }

    // Si se quedo sin ciclos para correr, guardarlo.
    currentProcessPCB->rsp = currStackPointer;

    // Si el proceso actual es el idle ignorarlo.
    if (currentProcessPCB->pid != idleProcessPCB->pid)
      enqueue(queue, (void *)currentProcessPCB);
    else
      idleProcessPCB = currentProcessPCB;
  }

  if (readyCount > 0)
  {
    currentProcessPCB = (pcb *)dequeue(queue);

    // Como readyCount > 0, me aseguro que existe algun proceso
    while (currentProcessPCB->state != READY)
    {
      if (currentProcessPCB->state == TERMINATED)
      {
        freeProcess(currentProcessPCB);
      }
      else
      {
        enqueue(queue, (void *)currentProcessPCB);
      }
      currentProcessPCB = (pcb *)dequeue(queue);
    }
  }
  else
  {
    currentProcessPCB = idleProcessPCB;
  }

  currentCycles = currentProcessPCB->priority;
  return currentProcessPCB->rsp;
}

// Comienza una tarea
int startTask(void (*process)(int argc, char **argv), int argc, char **argv, int foreground, int *fd)
{
  if (process == NULL)
    return -1;

  pcb *newProcess = initializeBlock(argv[0], foreground, fd == NULL ? currentProcessPCB->fileDescriptors : fd);

  if (newProcess == NULL)
  {
    return -1;
  }

  char **args = malloc(sizeof(char *) * argc);

  if (cpyArgs(args, argv, argc) == -1)
  {
    freeProcess(newProcess);
    return -1;
  }

  newProcess->argc = argc;
  newProcess->argv = args;

  initializeStack(process, argc, args, newProcess->rbp);

  enqueue(queue, (void *)newProcess);
  readyCount += 1;

  // Si el proceso es un proceso background del Userland(shell), le doy al init que lo evalue
  if (currentProcessPCB->pid == userlandPid && !foreground)
  {
    newProcess->ppid = initPid;
  }
  newProcess->mallocList = initQueue();
  return newProcess->pid;
}

processInfo toProcessInfo(pcb *aux)
{
  processInfo process;
  process.foreground = aux->foreground;
  strcpy(process.name, aux->name);
  process.pid = aux->pid;
  process.ppid = aux->ppid;
  process.rbp = (uint64_t)aux->rbp;
  process.rsp = (uint64_t)aux->rsp;
  strcpy(process.state, stateToStr(aux->state));
  process.priority = aux->priority;
  return process;
}

schedulerInfo *getSchedulerInfo()
{
  schedulerInfo *schedulerData = malloc(sizeof(schedulerInfo));
  if (schedulerData == NULL)
    return NULL;

  schedulerData->length = getQueueSize(queue) + 1;
  schedulerData->array = malloc(sizeof(processInfo) * schedulerData->length);
  if (schedulerData->array == NULL)
  {
    free(schedulerData);
    return NULL;
  }

  iteratorADT it = toBegin(queue);

  int index = 1;
  schedulerData->array[0] = toProcessInfo(currentProcessPCB);
  while (hasNext(it))
  {
    schedulerData->array[index++] = toProcessInfo((pcb *)next(it));
  }
  free(it);
  return schedulerData;
}

// Se usa para identificar al userland
int setUserlandPid(int pid)
{
  userlandPid = pid;
  return 0;
}

// Funcion que retorna un string del estado de un proceso
char *stateToStr(process_state state)
{
  switch (state)
  {
  case READY:
    return "READY";
  case BLOCKED:
    return "BLOCKED";
  case TERMINATED:
    return "TERMINATED";
  case EXITED:
    return "EXITED";
  default:
    return "TROYAN HORSE INCOMING!";
  }
}

// retorna el id del proceso actual
int getpid()
{
  return (currentProcessPCB != NULL) ? currentProcessPCB->pid : 0;
}

// Mata el proceso especificado. Ahora puede ser liberado.
int killTask(int pid)
{
  if (pid == userlandPid || pid == initPid)
    return -1;
  int id = changeState(pid, EXITED);
  // No se encontro el proceso
  if (id == -1)
    return -1;

  if (id == currentProcessPCB->pid)
  {
    _callTimerTick();
  }
  return id;
}

// Cambia la prioridad del proceso especificado
int nice(int pid, int newPriority)
{
  if (newPriority < 0)
    newPriority = 0;
  if (newPriority > MAX_PRIO)
    newPriority = MAX_PRIO;

  pcb *process = getProcess(queue, pid);

  if (process == NULL)
  {
    return -1;
  }

  process->priority = newPriority;
  return 0;
}

// Bloquea el proceso especificado
int blockTask(int pid)
{
  int id = changeState(pid, BLOCKED);
  if (id == -1)
    return -1;
  if (id == currentProcessPCB->pid)
    _callTimerTick();
  return 0;
}

// Desbloque el proceso especificado. Devuelve -1 si no se encontro el proceso.
int resumeTask(int pid)
{
  return changeState(pid, READY);
}

// Renuncia al CPU
int yield()
{
  currentCycles = 0;
  _callTimerTick();
  return 0;
}

// Termina el proceso actual
int killCurrent()
{
  return killTask(currentProcessPCB->pid);
}

int currentForegroundCondition(pcb *process, void *_)
{
  return process->foreground && process->pid != initPid && process->pid != userlandPid;
}

int killCurrentForeground()
{
  if (currentProcessPCB->foreground && (currentProcessPCB->pid != userlandPid) && (currentProcessPCB->pid != initPid))
  {
    terminateChildren(currentProcessPCB->pid);
    return killCurrent();
  }
  pcb *foregroundProcess = (pcb *)find(queue, (comparator)currentForegroundCondition, NULL);
  if (!foregroundProcess)
    return -1;
  terminateChildren(foregroundProcess->pid);
  return killTask(foregroundProcess->pid);
}

int waitpid(int pid)
{
  pcb *childProcess = currentProcessPCB->pid == pid ? currentProcessPCB : getProcess(queue, pid);

  if (childProcess == NULL)
  {
    return -1;
  }
  // Espero a que termine
  semWait(childProcess->semId);
  return changeState(childProcess->pid, TERMINATED);
}

static void processWrapper(void (*process)(int, char **), int argc, char **argv)
{
  process(argc, argv);
  killCurrent();
}

static void initializeStack(void (*process)(int, char **), int argc, char **argv, void *rbp)
{
  // Ubico el stackframe correctamente
  stackFrame *sf = (stackFrame *)rbp - 1;

  // Cargo el stackframe inicial
  sf->gs = 0x001;
  sf->fs = 0x002;
  sf->r15 = 0x003;
  sf->r14 = 0x004;
  sf->r13 = 0x005;
  sf->r12 = 0x006;
  sf->r11 = 0x007;
  sf->r10 = 0x008;
  sf->r9 = 0x009;
  sf->r8 = 0x00A;
  sf->rsi = (uint64_t)argc;
  sf->rdi = (uint64_t)process;
  sf->rbp = 0x00B;
  sf->rdx = (uint64_t)argv;
  sf->rcx = 0x00C;
  sf->rbx = 0x00D;
  sf->rax = 0x00E;
  sf->rip = (uint64_t)processWrapper;
  sf->cs = 0x008;
  sf->eflags = 0x202;
  sf->rsp = (uint64_t)(&sf->base);
  sf->ss = 0x000;
  sf->base = 0x000;
}

static pcb *initializeBlock(char *name, int foreground, int *fd)
{
  if (foreground > 1)
    return NULL;
  pcb *newProcess = malloc(sizeof(pcb));

  if (newProcess == NULL)
    return NULL;
  newProcess->ppid = (currentProcessPCB != NULL) ? currentProcessPCB->pid : 0;

  strcpy(newProcess->name, name);

  newProcess->pid = pidCounter++;
  newProcess->foreground = foreground; // chequear que el proceso actual sea foreground
  newProcess->state = READY;

  newProcess->priority = (foreground) ? FOREGROUND_PRIORITY : BACKGROUND_PRIORITY;
  void *tempRbp = malloc(sizeof(char) * STACK_SIZE);

  if (tempRbp == NULL)
  {
    // memoryDump();
    free(newProcess);
    return NULL;
  }

  newProcess->fileDescriptors[0] = fd[0];
  newProcess->fileDescriptors[1] = fd[1];

  if (fd[0] != STDIN_PIPENO)
  {
    pipeOpen(fd[0]);
  }

  if (fd[1] != STDOUT_PIPENO)
  {
    pipeOpen(fd[1]);
  }

  newProcess->semId = semInit(0);

  if (newProcess->semId == -1)
  {
    free(tempRbp);
    free(newProcess);
    return NULL;
  }

  newProcess->rbp = (void *)((char *)tempRbp + STACK_SIZE - 1);
  newProcess->rsp = (void *)((stackFrame *)newProcess->rbp - 1);

  return newProcess;
}

static int changeState(int pid, process_state newState)
{
  pcb *process = (pid == currentProcessPCB->pid) ? currentProcessPCB : getProcess(queue, pid);
  if (process == NULL || (process->state == EXITED && newState != TERMINATED) || process->state == TERMINATED)
    return -1;

  if (newState == EXITED)
    semPost(process->semId);

  if (newState == TERMINATED)
    sendTaskToInit(process->pid);

  if (process->state != READY && newState == READY)
    readyCount += 1;

  if (process->state == READY && newState != READY)
    readyCount -= 1;

  process->state = newState;
  return process->pid;
}

static int cpyArgs(char **dest, char **from, int count)
{
  for (int i = 0; i < count; i += 1)
  {
    dest[i] = malloc(sizeof(char) * (strlen(from[i]) + 1));
    if (dest[i] == NULL)
    {
      i--;
      // Libero la memoria existente hasta ahora
      while (i >= 0)
      {
        free(dest[i]);
        i -= 1;
      }
      return -1;
    }
    strcpy(dest[i], from[i]);
  }
  return 0;
}

static pcb *getProcess(queueADT queue, int pid)
{
  iteratorADT it = toBegin(queue);
  while (hasNext(it))
  {
    pcb *process = (pcb *)next(it);
    if (pid == process->pid)
    {
      free(it);
      return process;
    }
  }
  free(it);
  return NULL;
}

static void terminateChildren(int pid)
{
  iteratorADT it = toBegin(queue);
  pcb *current = currentProcessPCB;
  do
  {
    if (current->ppid == pid)
    {
      changeState(current->pid, TERMINATED);
      current->ppid = initPid;
    }
    current = (pcb *)next(it);
  } while (hasNext(it));
  free(it);
}

static int sendTaskToInit(int pid)
{
  iteratorADT it = toBegin(queue);
  int hasParent = 0;
  pcb *killedProcess = (pcb *)getProcess(queue, pid);
  pcb *process = currentProcessPCB;
  if(process == NULL)
    return -1;
  do
  {
    // Si su proceso padre esta muerto, se envia a init
    if (process->pid == killedProcess->ppid && (process->state != TERMINATED && process->state != EXITED))
      hasParent = 1;

    // Envia sus procesos hijos no terminados al init para ser procesados
    if (process->ppid == pid && process->state != TERMINATED)
    {
      process->ppid = initPid;
    }
    process = (pcb *)next(it);
  } while (hasNext(it));
  free(it);
  // Proceso es huerfano
  if (!hasParent)
  {
    killedProcess->ppid = initPid;
  }
  return 0;
}

void addMalloc(void *mem)
{
  if (mem != NULL)
  {
    enqueue(currentProcessPCB->mallocList, mem);
  }
}

int removeMallocCondition(void *listElement, void *checkedElement)
{
  return listElement == checkedElement;
}

void freeMalloc(void *mem)
{
  removeElement(currentProcessPCB->mallocList, removeMallocCondition, mem);
}