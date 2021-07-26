/* uLisp STM32 Version 3.0b - www.ulisp.com
   David Johnson-Davies - www.technoblogy.com - 11th January 2020

   Licensed under the MIT license: https://opensource.org/licenses/MIT
*/
#define _VARIANT_ARDUINO_STM32_

#include <Arduino.h>

// Lisp Library
const char LispLibrary[] PROGMEM = "";

// Compile options

// #define resetautorun
#define printfreespace
#define serialmonitor
// #define printgcs
// #define sdcardsupport
// #define lisplibrary

// Includes

// #include "LispLibrary.h"
#include <setjmp.h>
#include <SPI.h>
#include <Wire.h>
#include <limits.h>
#include <EEPROM.h>

#if defined(sdcardsupport)
#include <SD.h>
#define SDSIZE 172
#else
#define SDSIZE 0
#endif

// C Macros

#define nil                NULL
#define car(x)             (((object *) (x))->car)
#define cdr(x)             (((object *) (x))->cdr)

#define first(x)           (((object *) (x))->car)
#define second(x)          (car(cdr(x)))
#define cddr(x)            (cdr(cdr(x)))
#define third(x)           (car(cdr(cdr(x))))

#define push(x, y)         ((y) = cons((x),(y)))
#define pop(y)             ((y) = cdr(y))

#define integerp(x)        ((x) != NULL && (x)->type == NUMBER)
#define floatp(x)          ((x) != NULL && (x)->type == FLOAT)
#define symbolp(x)         ((x) != NULL && (x)->type == SYMBOL)
#define stringp(x)         ((x) != NULL && (x)->type == STRING)
#define characterp(x)      ((x) != NULL && (x)->type == CHARACTER)
#define streamp(x)         ((x) != NULL && (x)->type == STREAM)

#define mark(x)            (car(x) = (object *)(((uintptr_t)(car(x))) | MARKBIT))
#define unmark(x)          (car(x) = (object *)(((uintptr_t)(car(x))) & ~MARKBIT))
#define marked(x)          ((((uintptr_t)(car(x))) & MARKBIT) != 0)
#define MARKBIT            1

#define setflag(x)         (Flags = Flags | 1<<(x))
#define clrflag(x)         (Flags = Flags & ~(1<<(x)))
#define tstflag(x)         (Flags & 1<<(x))

// Constants

const int TRACEMAX = 3; // Number of traced functions
enum type { ZERO=0, SYMBOL=2, NUMBER=4, STREAM=6, CHARACTER=8, FLOAT=10, STRING=12, PAIR=14 };  // STRING and PAIR must be last
enum token { UNUSED, BRA, KET, QUO, DOT };
enum stream { SERIALSTREAM, I2CSTREAM, SPISTREAM, SDSTREAM };

enum function { NIL, TEE, NOTHING, OPTIONAL, AMPREST, LAMBDA, LET, LETSTAR, CLOSURE, SPECIAL_FORMS, QUOTE,
DEFUN, DEFVAR, SETQ, LOOP, RETURN, PUSH, POP, INCF, DECF, SETF, DOLIST, DOTIMES, TRACE, UNTRACE,
FORMILLIS, WITHSERIAL, WITHI2C, WITHSPI, WITHSDCARD, TAIL_FORMS, PROGN, IF, COND, WHEN, UNLESS, CASE, AND,
OR, FUNCTIONS, NOT, NULLFN, CONS, ATOM, LISTP, CONSP, SYMBOLP, STREAMP, EQ, CAR, FIRST, CDR, REST, CAAR,
CADR, SECOND, CDAR, CDDR, CAAAR, CAADR, CADAR, CADDR, THIRD, CDAAR, CDADR, CDDAR, CDDDR, LENGTH, LIST,
REVERSE, NTH, ASSOC, MEMBER, APPLY, FUNCALL, APPEND, MAPC, MAPCAR, MAPCAN, ADD, SUBTRACT, MULTIPLY,
DIVIDE, MOD, ONEPLUS, ONEMINUS, ABS, RANDOM, MAXFN, MINFN, NOTEQ, NUMEQ, LESS, LESSEQ, GREATER, GREATEREQ,
PLUSP, MINUSP, ZEROP, ODDP, EVENP, INTEGERP, NUMBERP, FLOATFN, FLOATP, SIN, COS, TAN, ASIN, ACOS, ATAN,
SINH, COSH, TANH, EXP, SQRT, LOG, EXPT, CEILING, FLOOR, TRUNCATE, ROUND, CHAR, CHARCODE, CODECHAR,
CHARACTERP, STRINGP, STRINGEQ, STRINGLESS, STRINGGREATER, SORT, STRINGFN, CONCATENATE, SUBSEQ,
READFROMSTRING, PRINCTOSTRING, PRIN1TOSTRING, LOGAND, LOGIOR, LOGXOR, LOGNOT, ASH, LOGBITP, EVAL, GLOBALS,
LOCALS, MAKUNBOUND, BREAK, READ, PRIN1, PRINT, PRINC, TERPRI, READBYTE, READLINE, WRITEBYTE, WRITESTRING,
WRITELINE, RESTARTI2C, GC, ROOM, SAVEIMAGE, LOADIMAGE, CLS, PINMODE, DIGITALREAD, DIGITALWRITE,
ANALOGREAD, ANALOGWRITE, DELAY, MILLIS, SLEEP, NOTE, EDIT, PPRINT, PPRINTALL, REQUIRE, LISTLIBRARY, ENDFUNCTIONS };

// Typedefs

typedef unsigned int symbol_t;

typedef struct sobject {
  union {
    struct {
      sobject *car;
      sobject *cdr;
    };
    struct {
      unsigned int type;
      union {
        symbol_t name;
        int integer;
        float single_float;
      };
    };
  };
} object;

typedef object *(*fn_ptr_type)(object *, object *);

typedef struct {
  const char *string;
  fn_ptr_type fptr;
  uint8_t min;
  uint8_t max;
} tbl_entry_t;

typedef int (*gfun_t)();
typedef void (*pfun_t)(char);
typedef int PinMode;

// Workspace
#define PERSIST __attribute__((section(".text")))
#define WORDALIGNED __attribute__((aligned (4)))
#define BUFFERSIZE 34  // Number of bits+2

#if defined(_BOARD_MAPLE_MINI_H_)
  #define WORKSPACESIZE 1150-SDSIZE       /* Cells (8*bytes) */
  #define EEPROMSIZE 10240                /* Bytes */
  #define SYMBOLTABLESIZE 512             /* Bytes - must be even*/
  uint8_t _end;

#elif defined(_VARIANT_ARDUINO_STM32_)
  #define WORKSPACESIZE 1472-SDSIZE       /* Cells (8*bytes) */
  #define EEPROMSIZE 10240                /* Bytes */
  #define SYMBOLTABLESIZE 512             /* Bytes - must be even*/
  uint8_t _end;

#endif

object Workspace[WORKSPACESIZE] WORDALIGNED;
char SymbolTable[SYMBOLTABLESIZE];

// Global variables

jmp_buf exception;
unsigned int Freespace = 0;
object *Freelist;
char *SymbolTop = SymbolTable;
unsigned int I2CCount;
unsigned int TraceFn[TRACEMAX];
unsigned int TraceDepth[TRACEMAX];

object *GlobalEnv;
object *GCStack = NULL;
object *GlobalString;
int GlobalStringIndex = 0;
char BreakLevel = 0;
char LastChar = 0;
char LastPrint = 0;

// Flags
enum flag { PRINTREADABLY, RETURNFLAG, ESCAPE, EXITEDITOR, LIBRARYLOADED, NOESC };
volatile char Flags = 0b00001; // PRINTREADABLY set by default

// Forward references
object *tee;
object *tf_progn (object *form, object *env);
object *eval (object *form, object *env);
// object *read ();
void repl(object *env);
void printobject (object *form, pfun_t pfun);
char *lookupbuiltin (symbol_t name);
intptr_t lookupfn (symbol_t name);
int builtin (char* n);
void error (symbol_t fname, PGM_P string, object *symbol);
void error2 (symbol_t fname, PGM_P string);

void initworkspace ();
object *myalloc ();
void myfree (object *obj);
object *number (int n);
object *makefloat (float f);
object *character (char c);
object *cons (object *arg1, object *arg2);
object *symbol (symbol_t name);
object *newsymbol (symbol_t name);
object *stream (unsigned char streamtype, unsigned char address);
void markobject (object *obj);
void sweep ();
void gc (object *form, object *env);
void movepointer (object *from, object *to);
int compactimage (object **arg);
char *MakeFilename (object *arg);
void FlashSetup ();
void FlashWrite16 (unsigned int *addr, uint16_t data);
void FlashWriteInt (unsigned int *addr, int data);
int saveimage (object *arg);
uint16_t FlashRead16 (unsigned int *addr);
int FlashReadInt (unsigned int *addr);
int loadimage (object *arg);
void autorunimage ();
void errorsub (symbol_t fname, PGM_P string);
// void error (symbol_t fname, PGM_P string, object *symbol);
// void error2 (symbol_t fname, PGM_P string);
boolean tracing (symbol_t name);
void trace (symbol_t name);
void untrace (symbol_t name);
boolean consp (object *x);
boolean atom (object *x);
boolean listp (object *x);
boolean improperp (object *x);
int toradix40 (char ch);
int fromradix40 (int n);
int pack40 (char *buffer);
boolean valid40 (char *buffer);
int digitvalue (char d);
char *symbolname (symbol_t x);
int checkinteger (symbol_t name, object *obj);
float checkintfloat (symbol_t name, object *obj);
int checkchar (symbol_t name, object *obj);
int isstream (object *obj);
int issymbol (object *obj, symbol_t n);
void checkargs (symbol_t name, object *args);
int eq (object *arg1, object *arg2);
int listlength (symbol_t name, object *list);
object *assoc (object *key, object *list);
object *delassoc (object *key, object **alist);
void indent (int spaces, pfun_t pfun);
void buildstring (char ch, int *chars, object **head);
object *readstring (char delim, gfun_t gfun);
int stringlength (object *form);
char nthchar (object *string, int n);
object *value (symbol_t n, object *env);
object *findvalue (object *var, object *env);
object *closure (int tc, symbol_t name, object *state, object *function, object *args, object **env);
object *apply (symbol_t name, object *function, object *args, object *env);
object **place (symbol_t name, object *args, object *env);
object *carx (object *arg);
object *cdrx (object *arg);
void I2Cinit (bool enablePullup);
uint8_t I2Cread ();
bool I2Cwrite (uint8_t data);
bool I2Cstart (uint8_t address, uint8_t read);
bool I2Crestart (uint8_t address, uint8_t read);
void I2Cstop (uint8_t read);
int spiread ();
int serial1read ();
void serialbegin (int address, int baud);
void serialend (int address);
gfun_t gstreamfun (object *args);
void spiwrite (char c);
void serial1write (char c);
pfun_t pstreamfun (object *args);
void checkanalogread (int pin);
void checkanalogwrite (int pin);
void tone (int pin, int note);
void noTone (int pin);
void playnote (int pin, int note, int octave);
void nonote (int pin);
void initsleep ();
void sleep (int secs);
object *sp_quote (object *args, object *env);
object *sp_defun (object *args, object *env);
object *sp_defvar (object *args, object *env);
object *sp_setq (object *args, object *env);
object *sp_loop (object *args, object *env);
object *sp_return (object *args, object *env);
object *sp_push (object *args, object *env);
object *sp_pop (object *args, object *env);
object *sp_incf (object *args, object *env);
object *sp_decf (object *args, object *env);
object *sp_setf (object *args, object *env);
object *sp_dolist (object *args, object *env);
object *sp_dotimes (object *args, object *env);
object *sp_trace (object *args, object *env);
object *sp_untrace (object *args, object *env);
object *sp_formillis (object *args, object *env);
object *sp_withserial (object *args, object *env);
object *sp_withi2c (object *args, object *env);
object *sp_withspi (object *args, object *env);
object *sp_withsdcard (object *args, object *env);
// object *tf_progn (object *args, object *env);
object *tf_if (object *args, object *env);
object *tf_cond (object *args, object *env);
object *tf_when (object *args, object *env);
object *tf_unless (object *args, object *env);
object *tf_case (object *args, object *env);
object *tf_and (object *args, object *env);
object *tf_or (object *args, object *env);
object *fn_not (object *args, object *env);
object *fn_cons (object *args, object *env);
object *fn_atom (object *args, object *env);
object *fn_listp (object *args, object *env);
object *fn_consp (object *args, object *env);
object *fn_symbolp (object *args, object *env);
object *fn_streamp (object *args, object *env);
object *fn_eq (object *args, object *env);
object *fn_car (object *args, object *env);
object *fn_cdr (object *args, object *env);
object *fn_caar (object *args, object *env);
object *fn_cadr (object *args, object *env);
object *fn_cdar (object *args, object *env);
object *fn_cddr (object *args, object *env);
object *fn_caaar (object *args, object *env);
object *fn_caadr (object *args, object *env);
object *fn_cadar (object *args, object *env);
object *fn_caddr (object *args, object *env);
object *fn_cdaar (object *args, object *env);
object *fn_cdadr (object *args, object *env);
object *fn_cddar (object *args, object *env);
object *fn_cdddr (object *args, object *env);
object *fn_length (object *args, object *env);
object *fn_list (object *args, object *env);
object *fn_reverse (object *args, object *env);
object *fn_nth (object *args, object *env);
object *fn_assoc (object *args, object *env);
object *fn_member (object *args, object *env);
object *fn_apply (object *args, object *env);
object *fn_funcall (object *args, object *env);
object *fn_append (object *args, object *env);
object *fn_mapc (object *args, object *env);
object *fn_mapcar (object *args, object *env);
object *fn_mapcan (object *args, object *env);
object *add_floats (object *args, float fresult);
object *fn_add (object *args, object *env);
object *subtract_floats (object *args, float fresult);
object *negate (object *arg);
object *fn_subtract (object *args, object *env);
object *multiply_floats (object *args, float fresult);
object *fn_multiply (object *args, object *env);
object *divide_floats (object *args, float fresult);
object *fn_divide (object *args, object *env);
object *fn_mod (object *args, object *env);
object *fn_oneplus (object *args, object *env);
object *fn_oneminus (object *args, object *env);
object *fn_abs (object *args, object *env);
object *fn_random (object *args, object *env);
object *fn_maxfn (object *args, object *env);
object *fn_minfn (object *args, object *env);
object *fn_noteq (object *args, object *env);
object *fn_numeq (object *args, object *env);
object *fn_less (object *args, object *env);
object *fn_lesseq (object *args, object *env);
object *fn_greater (object *args, object *env);
object *fn_greatereq (object *args, object *env);
object *fn_plusp (object *args, object *env);
object *fn_minusp (object *args, object *env);
object *fn_zerop (object *args, object *env);
object *fn_oddp (object *args, object *env);
object *fn_evenp (object *args, object *env);
object *fn_integerp (object *args, object *env);
object *fn_numberp (object *args, object *env);
object *fn_floatfn (object *args, object *env);
object *fn_floatp (object *args, object *env);
object *fn_sin (object *args, object *env);
object *fn_cos (object *args, object *env);
object *fn_tan (object *args, object *env);
object *fn_asin (object *args, object *env);
object *fn_acos (object *args, object *env);
object *fn_atan (object *args, object *env);
object *fn_sinh (object *args, object *env);
object *fn_cosh (object *args, object *env);
object *fn_tanh (object *args, object *env);
object *fn_exp (object *args, object *env);
object *fn_sqrt (object *args, object *env);
object *fn_log (object *args, object *env);
int intpower (int base, int exp);
object *fn_expt (object *args, object *env);
object *fn_ceiling (object *args, object *env);
object *fn_floor (object *args, object *env);
object *fn_truncate (object *args, object *env);
int myround (float number);
object *fn_round (object *args, object *env);
object *fn_char (object *args, object *env);
object *fn_charcode (object *args, object *env);
object *fn_codechar (object *args, object *env);
object *fn_characterp (object *args, object *env);
object *fn_stringp (object *args, object *env);
bool stringcompare (symbol_t name, object *args, bool lt, bool gt, bool eq);
object *fn_stringeq (object *args, object *env);
object *fn_stringless (object *args, object *env);
object *fn_stringgreater (object *args, object *env);
object *fn_sort (object *args, object *env);
object *fn_stringfn (object *args, object *env);
object *fn_concatenate (object *args, object *env);
object *fn_subseq (object *args, object *env);
int gstr ();
object *fn_readfromstring (object *args, object *env);
void pstr (char c);
object *fn_princtostring (object *args, object *env);
object *fn_prin1tostring (object *args, object *env);
object *fn_logand (object *args, object *env);
object *fn_logior (object *args, object *env);
object *fn_logxor (object *args, object *env);
object *fn_lognot (object *args, object *env);
object *fn_ash (object *args, object *env);
object *fn_logbitp (object *args, object *env);
object *fn_eval (object *args, object *env);
object *fn_globals (object *args, object *env);
object *fn_locals (object *args, object *env);
object *fn_makunbound (object *args, object *env);
object *fn_break (object *args, object *env);
object *fn_read (object *args, object *env);
object *fn_prin1 (object *args, object *env);
object *fn_print (object *args, object *env);
object *fn_princ (object *args, object *env);
object *fn_terpri (object *args, object *env);
object *fn_readbyte (object *args, object *env);
object *fn_readline (object *args, object *env);
object *fn_writebyte (object *args, object *env);
object *fn_writestring (object *args, object *env);
object *fn_writeline (object *args, object *env);
object *fn_restarti2c (object *args, object *env);
object *fn_gc (object *obj, object *env);
object *fn_room (object *args, object *env);
object *fn_saveimage (object *args, object *env);
object *fn_loadimage (object *args, object *env);
object *fn_cls (object *args, object *env);
object *fn_pinmode (object *args, object *env);
object *fn_digitalread (object *args, object *env);
object *fn_digitalwrite (object *args, object *env);
object *fn_analogread (object *args, object *env);
object *fn_analogwrite (object *args, object *env);
object *fn_delay (object *args, object *env);
object *fn_millis (object *args, object *env);
object *fn_sleep (object *args, object *env);
object *fn_note (object *args, object *env);
object *fn_edit (object *args, object *env);
object *edit (object *fun);
void pcount (char c);
int atomwidth (object *obj);
boolean quoted (object *obj);
int subwidth (object *obj, int w);
int subwidthlist (object *form, int w);
void superprint (object *form, int lm, pfun_t pfun);
void supersub (object *form, int lm, int super, pfun_t pfun);
object *fn_pprint (object *args, object *env);
object *fn_pprintall (object *args, object *env);
object *fn_require (object *args, object *env);
object *fn_listlibrary (object *args, object *env);
// int builtin (char* n);
int longsymbol (char *buffer);
// intptr_t lookupfn (symbol_t name);
uint8_t lookupmin (symbol_t name);
uint8_t lookupmax (symbol_t name);
// char *lookupbuiltin (symbol_t name);
char *lookupsymbol (symbol_t name);
void deletesymbol (symbol_t name);
void testescape ();
// object *eval (object *form, object *env);
int maxbuffer (char *buffer);
void pserial (char c);
void pcharacter (char c, pfun_t pfun);
void pstring (char *s, pfun_t pfun);
void printstring (object *form, pfun_t pfun);
void pfstring (const char *s, pfun_t pfun);
void pint (int i, pfun_t pfun);
void pmantissa (float f, pfun_t pfun);
void pfloat (float f, pfun_t pfun);
void pln (pfun_t pfun);
void pfl (pfun_t pfun);
// void printobject (object *form, pfun_t pfun);
int glibrary ();
void loadfromlibrary (object *env);
int gserial ();
object *nextitem (gfun_t gfun);
object *readrest (gfun_t gfun);
object *read (gfun_t gfun);
void initenv ();
void setup ();
// void repl (object *env);
void loop ();
