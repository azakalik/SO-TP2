// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <math.h>
#include "commands.h"
#include "ustdlib.h"
#include "stdint.h"
#include "RTCID.h"
#include "usersyscalls.h"
#include "fibonacci.h"
#include "prime.h"
#include "inforeg.h"
#include "tests.h"
#include "phylo.h"
#include "colors.h"
#include "printinfo.h"

static color_t ERROR_COLOURS[] = {RED, BLACK};
static color_t CHEESE_COLOURS[] = {YELLOW, BLACK};

static command_t commands[] = {
    {"cheese", &printCheese, "mmmmm... queso..."},
    {"brand", &printCheeseOs, "Imprime el nombre del sistema operativo"},
    {"date&time", &dateAndTime, "Imprime en pantalla la fecha del ano corriente y horario en que fue llamado."},
    {"fibonacci", &fibonacci, "Imprime la secuencia de fibonacci infinitamente hasta que se pause o se termine su ejecucion. "},
    {
        "hello",
        &holaMundo,
        "Imprime un saludo al usuario. ",
    },
    {"help", &help, "Imprime una lista detallada de los comandos  y modulos ejecutables del programa. "},
    {"inforeg", &infoReg, "Imprime los registros capturados al presionar ctrl + r. "},
    {"prime", &primes, "Imprime numeros primos infinitamente hasta que se pause o se termine su ejecucion. "},
    {"printmem", &printmem, "Recibe como argumento una direccion de memoria no superior a 80000000h y luego imprime los proximos 32bytes de memoria adyacentes a la direccion dada. "},
    {"clear", &clearScreen, "Limpia la pantalla. "},
    {"mem", &memDump, "Imprime el estado de memoria. "},
    {"ps", &ps, "Imprime el estado de los procesos. "},
    {"kill", &killTask, "Recibe un pid y mata al proceso. "},
    {"block", &blockTask, "Recibe un pid y bloquea al proceso. "},
    {"resume", &resumeTask, "Recibe un pid y resume el proceso. "},
    {"nice", &niceTask, "Recibe un pid y un valor de prioridad y modifica la prioridad del pid. "},
    {"loop", &loop, "Te saluda cada cierto tiempo. "},
    {"sem", &printSemInfo, "Imprime informacion sobre los semaforos. "},
    {"pipes", &printPipeInfo, "Imprime estado de los pipes. "},
    {"test_mm", &test_mm, "Recibe una cantidad de memoria y empieza a testear. "},
    {"test_prio", &test_prio, "Testea las prioridades del scheduler. "},
    {"test_processes", &test_processes, "Testea el scheduler. "},
    {"test_sync", &test_sync, "Testea semaforos y race conditions. "},
    {"phylo", &phylo, "Filosofos comensales. "},
    {"cat", &cat, "Imprime el stdin tal como lo recibe. "},
    {"wc", &wc, "Cuenta la cantidad de lineas escritas en stdin."},
    {"filter", &filter, "Filtra las vocales del input."},
    {"shortcuts", &printShortcuts, "Imprime los shortcuts que la terminal provee."},
    {"ejemplos", &ejemplos, "Imprime ejemplos de comandos posibles"},
    {"echo", &echo, "Imprime los argumentos a salida estandar"},
    {"", NULL, ""},
};

int getCommandsLength()
{
  int i;
  for (i = 0; commands[i].runner != NULL; i += 1)
    ;
  return i;
}

void printmem(unsigned int argc, char *argv[])
{
  if (argc < 1)
  {
    char *error_message = "No se dio una direccion de memoria\n";
    write(error_message, _strlen(error_message), ERROR_COLOURS);
    return;
  }
  char *address = argv[1];

  // Chequear que strToHex devuelva -1 si no es hex
  uint64_t memDir = strToHex(address);
  if (memDir == -1 || memDir >= LAST_MEM)
  {
    char *error_message = "\nLa direccion ingresada no es alcanzable \n";
    write(error_message, _strlen(error_message), ERROR_COLOURS);
    return;
  }
  _fprintf("\nDump de 32 bytes a partir de la direccion: %s\n\n", address);

  uint8_t buffer[DUMP_SIZE];
  printMem(memDir, buffer, DUMP_SIZE);
  for (int i = 0; i < DUMP_SIZE; i++)
  {
    if (i % 8 == 0)
    {
      _fprintf("\n x%d:  ", i);
    }
    _fprintf("0x%x  ", buffer[i]);
  }
  _fprintf("\n");
}

void dateAndTime()
{
  uint64_t date = getRTC(DAY_RTC_ID);
  uint64_t month = getRTC(MONTH_RTC_ID);
  uint64_t year = getRTC(YEAR_RTC_ID);
  _fprintf("La fecha de hoy es: %d/%d/%d", date, month, year);
  uint64_t hour = getRTC(HOUR_RTC_ID);
  uint64_t minute = getRTC(MINUTE_RTC_ID);
  uint64_t second = getRTC(SECOND_RTC_ID);
  _fprintf("\n Y el horario de este momento es: %d:%d:%d\n", hour, minute, second);
}

void help(unsigned int argc, char *argv[])
{
  long page = 0;

  if (argc > 1)
  {
    char *description = getDescriptions(argv[1]);
    if (description != NULL)
    {
      _print(description);
      return;
    }
    else
    {
      if (isNumber(argv[1]))
      {
        page = _atoi(argv[1]);
      }
      else
      {
        _fprintf("%s no existe\n", argv[1]);
        return;
      }
    }
  }

  int length = getCommandsLength();
  if (page > length / HELP_PAGE)
  {
    _fprintf("No existe esta pagina de comandos: \n");
    return;
  }
  _fprintf("* Lista de posibles comandos: \n");
  for (uint8_t i = 0; i < HELP_PAGE && commands[i + page * HELP_PAGE].runner != NULL; i++)
  {
    _fprintf("- %s: %s\n", commands[i + page * HELP_PAGE].name, commands[i + page * HELP_PAGE].help);
  }
  _fprintf("\nPagina nro %d de %d, help [pagina] para mas comandos.\n", page, length / HELP_PAGE);
  _fprintf("Para observar ejemplos de comandos utilizar el comando 'ejemplos'.\n");
  _fprintf("Para conocer los shortcuts del sistema, utilizar el comando shortcuts.\n");
}

void infoReg()
{
  static uint64_t registers[TOTAL_REGISTERS];
  inforeg(registers);

  for (int i = 0; i < TOTAL_REGISTERS; i++)
  {
    _fprintf("%s", registerNames[i]);
    _fprintf("%x\n", registers[i]);
  }
}

void holaMundo()
{
  _print("hola mundo\n");
}

char *getDescriptions(char *function)
{
  command_t *foundCommand = getCommand(function);
  if (foundCommand == NULL)
    return NULL;
  return foundCommand->help;
}

command_t *getCommand(char *commandName)
{
  for (uint8_t i = 0; _strcasecmp(commands[i].name, "") != 0; i++)
  {
    if (_strcasecmp(commands[i].name, commandName) == 0)
      return &commands[i];
  }

  return NULL;
}

void killTask(unsigned int argc, char *argv[])
{
  if (argc == 1)
  {
    _fprintf("No se ingreso un pid\n");
    return;
  }
  int res = _atoi(argv[1]);
  kill(res);
}

void blockTask(unsigned int argc, char *argv[])
{
  if (argc == 1)
  {
    _fprintf("No se ingreso un pid\n");
    return;
  }
  int res = _atoi(argv[1]);
  block(res);
}

void resumeTask(unsigned int argc, char *argv[])
{
  if (argc == 1)
  {
    _fprintf("No se ingreso un pid\n");
    return;
  }
  int res = _atoi(argv[1]);
  resume(res);
}

void niceTask(unsigned int argc, char *argv[])
{
  if (argc == 1)
  {
    _fprintf("No se ingreso un pid\n");
    return;
  }
  if (argc == 2)
  {
    _fprintf("No se ingreso un valor de nice\n");
  }
  nice(_atoi(argv[1]), _atoi(argv[2]));
}

void cat(unsigned int argc, char *argv[])
{
  if (argc != 1)
  {
    _fprintf("Uso incorrecto de comando");
    return;
  }
  int c;
  while ((c = getChar()) != -1)
    _putc(c);
}

void wc(unsigned int argc, char *argv[])
{
  if (argc != 1)
  {
    _fprintf("Uso incorrecto de comando");
    return;
  }
  int c, counter = 1;
  while ((c = getChar()) != -1)
  {
    if (c == '\n')
      counter++;
  }
  _fprintf("La cantidad de lineas escritas fueron: %d", counter);
  return;
}

void filter(unsigned int argc, char *argv[])
{
  if (argc != 1)
  {
    _fprintf("Uso incorrecto de comando");
    return;
  }
  int c;
  while ((c = getChar()) != -1)
  {
    c = toLower(c);
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
    {
    }
    else
    {
      _putc(c);
    }
  }
}

void printCheeseOs()
{
  write("  ______   __    __  ________   ______           ______    ______  \n", 68, CHEESE_COLOURS);
  write(" /      \\ |  \\  |  \\|        \\ /      \\         /      \\  /      \\ \n", 68, CHEESE_COLOURS);
  write("|  $$$$$$\\| $$  | $$| $$$$$$$$|  $$$$$$\\       |  $$$$$$\\|  $$$$$$\\\n", 68, CHEESE_COLOURS);
  write("| $$  | $$| $$  | $$| $$__    | $$___\\$$______ | $$  | $$| $$___\\$$\n", 68, CHEESE_COLOURS);
  write("| $$  | $$| $$  | $$| $$  \\    \\$$    \\|      \\| $$  | $$ \\$$    \\ \n", 68, CHEESE_COLOURS);
  write("| $$ _| $$| $$  | $$| $$$$$    _\\$$$$$$\\$$$$$$| $$  | $$ _\\$$$$$$\\\n", 67, CHEESE_COLOURS);
  write("| $$/ \\ $$| $$__/ $$| $$_____ |  \\__| $$       | $$__/ $$|  \\__| $$\n", 68, CHEESE_COLOURS);
  write(" \\$$ $$ $$ \\$$    $$| $$     \\ \\$$    $$        \\$$    $$ \\$$    $$\n", 68, CHEESE_COLOURS);
  write("  \\$$$$$$\\  \\$$$$$$  \\$$$$$$$$  \\$$$$$$          \\$$$$$$   \\$$$$$$\n", 67, CHEESE_COLOURS);
  write("      \\$$$\n", 11, CHEESE_COLOURS);
  _fprintf("\n");
}

void printCheese()
{
  _fprintf("\n");
  write("    _--\"-.           \n", 22, CHEESE_COLOURS);
  write(" .-\"      \"-.        \n", 22, CHEESE_COLOURS);
  write("|\"\"--..      \'-.     \n", 22, CHEESE_COLOURS);
  write("|      \"\"--..   \'-.  \n", 22, CHEESE_COLOURS);
  write("|.-. .-\".    \"\"--..\".\n", 22, CHEESE_COLOURS);
  write("|\'./  -\'  .-.      | \n", 22, CHEESE_COLOURS);
  write("|      .-. \'.-\'   .-\'\n", 22, CHEESE_COLOURS);
  write("\'--..  \'.\'    .-  -. \n", 22, CHEESE_COLOURS);
  write("     \"\"--..   \'_\'   :\n", 22, CHEESE_COLOURS);
  write("           \"\"--..   |\n", 22, CHEESE_COLOURS);
  write("                 \"\"-\'\n", 22, CHEESE_COLOURS);
  _fprintf("\n");
}

void printShortcuts()
{
  write("\nShortcut 1: ", 13, CHEESE_COLOURS);
  _fprintf("ctrl + c para terminar cualquier proceso foreground.\n");
  write("Shortcut 2: ", 13, CHEESE_COLOURS);
  _fprintf("ctrl + d para enviar un EOF al proceso actual(util para los procesosfilter, wc y cat).\n");
  write("Shortcut 3: ", 13, CHEESE_COLOURS);
  _fprintf("ctrl + r para capturar los registros en el momento.\n");
}

void ejemplos()
{
  write("\nEjemplo 1: ", 12, CHEESE_COLOURS);
  _fprintf("Se recomienda utilizar los comandos filter, wc y cat con el operador pipe, como por ejemplo: 'sem | filter', 'pipe | wc'.");
  _fprintf(" De esta manera se puede\nobservar de una mejor manera su comportamiento.\n");
  write("Ejemplo 2: ", 12, CHEESE_COLOURS);
  _fprintf("Para correr un proceso background, incluir al final del comando el \nsimbolo &, por ejemplo:'fibonacci &'.");
  _fprintf(" Recomendacion, ejecutar el comando ps para ver que el proceso esta corriendo en background (kill 'pid') para terminarlo.\n");
}

void echo(unsigned int argc, char *argv[])
{
  for (int i = 1; i < argc; i++)
  {
    _fprintf("%s ", argv[i]);
  }
  _fprintf("\n");
}

void loop()
{
  /*while (1)
  {
    _fprintf("Hola soy %d\n", getpid());
    sleep(3);
  }*/
  // implementacion de proceso que no hace free para defensa

  memDump();

  void *firstMalloc = malloc(9100);
  if(firstMalloc == NULL)
    return;

  memDump();
  

}