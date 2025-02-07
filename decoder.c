#include <stdio.h>

#include <unistd.h>
#include <sys/stat.h>   // contem mkdir e stat
#include <sys/statfs.h> // contem statfs
#include <errno.h>      // verifica erros de mkdir
#include <libgen.h>     // basename
#include <dirent.h>

#include <stdlib.h>

#include <string.h>

#define DIRNAMEOUT "output_log"
#define DIRNAMEIN "input_log"


#define FILE_PATH_SIZE              (64)
#define FILE_SIZE                   (4*1024*1024)

#define INSTRUCTION_SIZE            (4)
#define INSTRUCTION_SIZE_REAL       (INSTRUCTION_SIZE + 1)

#define FILE_SIZE_LIMIT             ((FILE_SIZE/INSTRUCTION_SIZE_REAL)*INSTRUCTION_SIZE_REAL)

#define DEFAULT_STATUS_FILE_NAME        "status"
#define DEFAULT_OUT_DIRECTORY_NAME      "output_log"
#define DEFAULT_IN_DIRECTORY_NAME       "input_log"

const char* decodeTableCond[16] = {
	[0b0000] = "EQ",  // Igual (Equal)
	[0b0001] = "NE",  // NC#o igual (Not Equal)
	[0b0010] = "CS",  // Carry Set
	[0b0011] = "CC",  // Carry Clear
	[0b0100] = "MI",  // Negativo (Minus)
	[0b0101] = "PL",  // Positivo (Plus)
	[0b0110] = "VS",  // Overflow
	[0b0111] = "VC",  // No Overflow
	[0b1000] = "HI",  // Unsigned Higher
	[0b1001] = "LS",  // Unsigned Lower or Same
	[0b1010] = "GE",  // Signed Greater or Equal
	[0b1011] = "LT",  // Signed Less Than
	[0b1100] = "GT",  // Signed Greater Than
	[0b1101] = "LE",  // Signed Less or Equal
	[0b1110] = "AL"   // Always
};

// [bit15-12:0000:4][bit11:opcode:1]
char* _decode0000[1][2] = {
	{"LSL_decode0000", "LSR_decode0000"},
};

// [0001][opcode]
char* _decode0001[2][2] = {
	{"ASR_decode0001", NULL},
	{"ADD_decode0001", "SUB_decode0001"}
};

char* _decode0010[1][2] = {
	{"MOV_decode0010", "CMP_decode0010"}
};

char* _decode0011[1][2] = {
	{"ADD_decode0011", "SUB_decode0011"}
};

char* _decode0100[9][4] = {
	{"AND", "EOR", "LSL", "LSR"},
	{"ASR", "ADC", "SBC", "ROR"},
	{"TST", "NEG", "CMP", "CMN"},
	{"ORR", "MUL", "BIC", "MVN"},//NULLx3
	{"CPY", NULL, NULL, NULL },
	{"ADD", "MOV", NULL, NULL }, //x3
	{"CMP", NULL, NULL, NULL },  //x3
	{"BX", "BLX", NULL, NULL },
	{"LDR", NULL, NULL, NULL }
};

// [bit11::1][bit10-9:opcode:2]
char* _decode0101[2][4] = {
	{"STR", "STRH", "STRB", "LDRSB"},
	{"LDR", "LDRH", "LDRB", "LDRSH"}
};


// [:0000:][bit11:opcode:1]
char* _decode0110[1][2] = {
	{"STR", "LDR"}
};


// [:0000:][bit11:opcode:1]
char* _decode0111[1][2] = {
	{"STRB", "LDRB"}
};


// [:0000:][bit11:opcode:1]
char* _decode1000[1][2] = {
	{"STRH", "LDRH"}
};


// [:0000:][bit11:opcode:1]
char* _decode1001[1][2] = {
	{"STR", "LDR"}
};

// [:0000:][bit11:opcode:1]
char* _decode1010[1][2] = {
	{"ADD", "ADD"}
};

char* _decode1011[7][4] = {
	{"ADD,", "SUB,", NULL, NULL  },
	{"SXTH", "SXTB", "UXTH", "UXTB"},
	{"REV", "REV16", "REVSH", NULL  },
	{"PUSH", "POP", NULL, NULL  },
	{"SETEND LE", "SETEND BE", NULL, NULL  },
	{"CPSIE", "CPSID", NULL, NULL  },
	{"BKPT", NULL, NULL, NULL  }
};

// [:0000:][bit11:opcode:1]
char* _decode1100[1][2] = {
	{"STMIA", "LDMIA"}
};

// [bit15-12::4][bit11-8:cond:4]
char* _decode1101[3][1] = {
// 	{"BEQ", "BNE", "BCS", "BCC", "BMI", "BPL", "BVS", "BVC", "BHI", "BLS", "BGE", "BLT", "BGT", "BLE"},
	{"B"        },
	{"Undefined"},
	{"SWI"      }
// 	{"SWI", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL}
};

// [bit11::1][bit11:opcode:1]
char* _decode1110[2][1] = {
	{"B_decode1110"},
	{"BLX_decode1110"}
};

// [bit11::1][:0000:]
char* _decode1111[2][2] = {
	{"BL_decode1111", "BLX_decode1111"},
	{"BL2_decode1111", NULL}
};


void* decodeTable[] = {
	_decode0000,
	_decode0001,
	_decode0010,
	_decode0011,

	_decode0100,
	_decode0101,
	_decode0110,
	_decode0111,

	_decode1000,
	_decode1001,
	_decode1010,
	_decode1011,

	_decode1100,
	_decode1101,
	_decode1110,
	_decode1111
};

#define mask(a, b, c, d) a##b##c##d

void _0000_immed5_Lm_Ld(const unsigned short int* instruction,
                        unsigned short int* op,
                        FILE* outputFile
                       )
{
	char immed5 = (*instruction & mask(0b0000, 0111, 1100, 0000)) >> 6;
	char Lm     = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
	char Ld     = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0000
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, r%d, #%d\n", entry[(*instruction & 0xF000) >> 12][*op]
	       , Ld
	       , Lm
	       , immed5
	      );
}

void _0001_immed3Lm_Ln_Ld(const unsigned short int* instruction,
                          unsigned short int* op,
                          FILE* outputFile
                         )
{
	char immed3;
	char immed5;
	char Lm;
	char Ln;
	char Ld;

	// Ponteiro para _decode0001
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	if(*instruction & mask(0b0000, 1000, 0000, 0000)) {

		Ln  = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
		Ld  = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;
		*op = (*instruction & mask(0b0000, 0010, 0000, 0000)) >> 9;

		if(*instruction & mask(0b0000, 0100, 0000, 0000)) {
			// immed3
			immed3  = (*instruction & mask(0b0000, 0001, 1100, 0000)) >> 6;

			fprintf(outputFile, "%s r%d, r%d, #%d\n", entry[(*instruction & 0xF000) >> 12][*op]
			       , Ld
			       , Ln
			       , immed3
			      );
		}
		else {
			// Lm
			Lm      = (*instruction & mask(0b0000, 0001, 1100, 0000)) >> 6;

			fprintf(outputFile, "%s r%d, r%d, r%d\n", entry[(*instruction & 0xF000) >> 12][*op]
			       , Ld
			       , Ln
			       , Lm
			      );
		}
	}
	else {
		*op = 0;
		immed5  = (*instruction & mask(0b0000, 0111, 1100, 0000)) >> 6;
		Lm      = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
		Ld      = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

		fprintf(outputFile, "%s r%d, r%d, #%d\n", entry[*instruction & mask(0b0000, 1000, 0000, 0000)][*op]
		       , Ld
		       , Lm
		       , immed5
		      );
	}
}

void _0010_LdLn_immed8(const unsigned short int* instruction,
                       unsigned short int* op,
                       FILE* outputFile
                      )
{
	char Ld;
	char Ln;
	char immed8;

	*op = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0001
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	Ld      = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
	Ln      = (*instruction & mask(0b0000, 0111, 0000, 1000)) >> 8;
	immed8  = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

	fprintf(outputFile, "%s r%d, #%d\n", entry[0][*op]
	       , Ld
	       , immed8
	      );

}

void _0011_Ld_immed8(const unsigned short int* instruction,
                     unsigned short int* op,
                     FILE* outputFile
                    )
{
	char Ld;
	char immed8;

	*op = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0001
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	Ld      = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
	immed8  = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

	fprintf(outputFile, "%s r%d, #%d\n", entry[0][*op]
	       , Ld
	       , immed8
	      );
}

void _0100_LmLs_Ld__Lm_LdLn__Lm_Ld_Hm7_Ld__Lm_Hd7__Hm7_Hd7__Hm7_Ln__Lm_Hn7__Hm7_Hn7(const unsigned short int* instruction,
                                                                                    unsigned short int* op,
                                                                                    FILE* outputFile
                                                                                   )
{
	char Ld;
	char Ls;
	int bit8a10 = 0;
	int bit6a8 = 0;

	//*op = (*instruction & mask(0b0000, 0000, 1100, 0000)) >> 6;
	bit6a8 = (*instruction & mask(0b0000, 0001, 1100, 0000)) >> 6;

	// Ponteiro para _decode0100
	char* (*entry)[4] = decodeTable[(*instruction & 0xF000) >> 12];

	if(*instruction & mask(0b0000, 1000, 0000, 0000)) {
		// bit11 == 1 LDR
		Ld          = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
		char immed8 = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;
		*op         = 0;

		fprintf(outputFile, "%s r%d, [pc, #%d]\n", entry[8][*op]
		       , Ld
		       , (unsigned char)immed8*4
		      );
	}
	else {
		// bit11 == 0
		bit8a10 = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
		if(
		    ((bit8a10 >= 0) && (bit8a10 <= 3)) ||
		    ((bit8a10 == 6) && !(*instruction & mask(0b0000, 0000, 1100, 0000)))
		)
		{
			char LmLs_Lm = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
			char Ld_LdLn = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

			*op          = (*instruction & mask(0b0000, 0000, 1100, 0000)) >> 6;

			fprintf(outputFile, "0x4 %s r%d, r%d\n", entry[bit8a10 == 6 ? 4 : bit8a10][*op]
			       , Ld_LdLn
			       , LmLs_Lm
			      );
		}
		else {
			if(
			    (*instruction & mask(0b0000, 0100, 0000, 0000)) && ((bit6a8 >= 1) && (bit6a8 <= 3))
			)
			{
				char Hm7_Lm_Hm7 = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
				char Ld_Hd7_Hd7 = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

				*op             = (*instruction & mask(0b0000, 0010, 0000, 0000)) >> 9;

				fprintf(outputFile, "0x4 %s r%d, r%d\n", entry[5][*op]
				       , Ld_Hd7_Hd7
				       , Hm7_Lm_Hm7
				      );
			}
			else {
				if(
				    (((*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8) == 5) &&
				    (((*instruction & mask(0b0000, 0000, 1100, 0000)) >> 6) >= 1) &&
				    (((*instruction & mask(0b0000, 0000, 1100, 0000)) >> 6) <= 3)
				)
				{
					char Hm7_Lm_Hm7 = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
					char Ln_Hn7_Hn7 = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

					*op             = 0;

					fprintf(outputFile, "0x4 %s r%d, r%d\n", entry[6][*op]
					       , Ln_Hn7_Hn7
					       , Hm7_Lm_Hm7
					      );
				}
				if(
				    (((*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8) == 7) &&
				    (!(*instruction & mask(0b0000, 0000, 0000, 0111)))
				)
				{
					char Rm = (*instruction & mask(0b0000, 0000, 0111, 1000)) >> 3;

					*op     = (*instruction & mask(0b0000, 0000, 1000, 0000)) >> 7;

					fprintf(outputFile, "0x4 %s r%d\n", entry[7][*op]
					       , Rm
					      );
				}
			}
		}
	}
};

void _0101_Lm_LnLd(const unsigned short int* instruction,
                   unsigned short int* op,
                   FILE* outputFile
                  )
{
	char Lm = (*instruction & mask(0b0000, 0001, 1100, 0000)) >> 6;
	char Ln = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
	char Ld = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

	*op = (*instruction & mask(0b0000, 0110, 0000, 0000)) >> 9;

	// Ponteiro para _decode0101
	char* (*entry)[4] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, [r%d, r%d]\n", entry[(*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11][*op]
	       , Ld
	       , Ln
	       , Lm
	      );
}

void _0110_immed5_Ln_Ld(const unsigned short int* instruction,
                        unsigned short int* op,
                        FILE* outputFile
                       )
{
	char immed5 = (*instruction & mask(0b0000, 0111, 1100, 0000)) >> 6;
	char Ln     = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
	char Ld     = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0110
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, [r%d, #%d]\n", entry[0][*op]
	       , Ld
	       , Ln
	       , immed5*4
	      );
}

void _0111_immed5_Ln_Ld(const unsigned short int* instruction,
                        unsigned short int* op,
                        FILE* outputFile
                       )
{
	char immed5 = (*instruction & mask(0b0000, 0111, 1100, 0000)) >> 6;
	char Ln     = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
	char Ld     = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0111
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, [r%d, #%d]\n", entry[0][*op]
	       , Ld
	       , Ln
	       , immed5
	      );
}

void _1000_immed5_Ln_Ld(const unsigned short int* instruction,
                        unsigned short int* op,
                        FILE* outputFile
                       )
{
	char immed5 = (*instruction & mask(0b0000, 0111, 1100, 0000)) >> 6;
	char Ln     = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
	char Ld     = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0111
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, [r%d, #%d]\n", entry[0][*op]
	       , Ld
	       , Ln
	       , immed5*2
	      );
}

void _1001_Ld_immed8(const unsigned short int* instruction,
                     unsigned short int* op,
                     FILE* outputFile
                    )
{
	char Ld     = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
	char immed8 = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0111
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, [sp, #%d]\n", entry[0][*op]
	       , Ld
	       , immed8*4
	      );
}

void _1010_Ld_immed8(const unsigned short int* instruction,
                     unsigned short int* op,
                     FILE* outputFile
                    )
{
	char Ld     = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
	char immed8 = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode0111
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	fprintf(outputFile, "%s r%d, %s, #%d\n", entry[0][*op]
	       , Ld
	       , (*op == 0) ? "pc" : "sp"
	       , immed8*4
	      );
}

// falta terminar [status: imcompleto]
void _1011_immed7__Lm_Ld__registerList__op__aif__immed8(const unsigned short int* instruction,
                                                        unsigned short int* op,
                                                        FILE* outputFile
                                                       )
{
    char cpsrBit = 0;
    char a       = 0;
    char i       = 0;
    char f       = 0;
//     char immed7;
//     char Lm;
//     char Ld;
	// char registerList;

// 	char Ld     = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
// 	char immed8 = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

// 	*op         = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode1011
	char* (*entry)[4] = decodeTable[(*instruction & 0xF000) >> 12];

	int bit9a10 = (*instruction & mask(0b0000, 0110, 0000, 0000)) >> 9;

	if((bit9a10 == 0) && !(*instruction & mask(0b0000, 1001, 0000, 0000))) {
		char immed7 = (*instruction & mask(0b0000, 0000, 0111, 1111)) >> 0;

		*op         = (*instruction & mask(0b0000, 0000, 1000, 0000)) >> 7;

		fprintf(outputFile, "%s %s, #%d\n", entry[0][*op]
		       , "sp"
		       , immed7*4
		      );
	}
	if(bit9a10 == 1) {
		if(
		    !(*instruction & mask(0b0000, 1001, 0000, 0000)) ||
		    ((*instruction & mask(0b0000, 1001, 0000, 0000)) == mask(0b0000, 1000, 0000, 0000))
		)
		{
			char Lm = (*instruction & mask(0b0000, 0000, 0011, 1000)) >> 3;
			char Ld = (*instruction & mask(0b0000, 0000, 0000, 0111)) >> 0;

			*op = (*instruction & mask(0b0000, 0000, 1100, 0000)) >> 6;

			fprintf(outputFile, "%s r%d, r%d\n"
			       , entry[((*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11)+
			                                                                ((*instruction & mask(0b0000, 0110, 0000, 0000)) >> 9)
			              ][*op]
			       , Ld
			       , Lm
			      );
		}
	}
	if (bit9a10 == 2) {
		char R;
		char registerList;

		*op          = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

		R            = (*instruction & mask(0b0000, 0001, 0000, 0000)) >> 8;
		registerList = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

		fprintf(outputFile, "%s {", entry[3][*op]
		      );

		char registerNumber = 0;
		char showComma = 0;
		while(registerList)
		{
			if(showComma) {
				fprintf(outputFile, ", ");
			}
			showComma = 0;

			if(registerList & mask(0b0000, 0000, 0000, 0001)) {
				fprintf(outputFile, "r%d", registerNumber);
				showComma = 1;
			}

			registerList >>= 1;
			registerNumber++;
		}

		if(showComma && R) {
			fprintf(outputFile, ", %s", *op == 0 ? "lr" : "pc");
		}

		fprintf(outputFile, "}\n");
	}
	if (bit9a10 == 3) {
		if((*instruction & mask(0b0000, 1001, 0000, 0000)) == mask(0b0000, 1000, 0000, 0000)) {
			char immed8 = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;
			*op         = 0;
			// saida incorreta, motivo desconhecido
			fprintf(outputFile, "%s #%d\n", entry[6][*op]
			       , immed8
			      );
		}
		if(!(*instruction & mask(0b0000, 1001, 0000, 0000))) {
			if((*instruction & mask(0b0000, 0000, 1111, 0111)) == mask(0b0000, 0000, 0101, 0000)) {
				*op = (*instruction & mask(0b0000, 0000, 0000, 1000)) >> 3;

				fprintf(outputFile, "%s\n", entry[4][*op]
				      );
			}
			if((*instruction & mask(0b0000, 0000, 1110, 1000)) == mask(0b0000, 0000, 0110, 0000)) {
				*op = (*instruction & mask(0b0000, 0000, 0001, 0000)) >> 4;
				
                a |= (*instruction & mask(0b0000, 0000, 0000, 0100)) << 6; // 1<<8
                i |= (*instruction & mask(0b0000, 0000, 0000, 0010)) << 6; // 1<<7
                f |= (*instruction & mask(0b0000, 0000, 0000, 0001)) << 6; // 1<<6
                
                cpsrBit = (cpsrBit & ~mask(0b0000, 0001, 1100, 0000))
                        | a | i | f;
                        // | (*instruction & mask(0b0000, 0000, 0000, 0111));
                        
				fprintf(outputFile, "%s\n", entry[5][*op]
				      );
			}
		}
	}
}

void _1100_Ln_registerList(const unsigned short int* instruction,
                           unsigned short int* op,
                           FILE* outputFile
                          )
{
	char Ln;
	unsigned char registerList;


	*op = (*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11;

	// Ponteiro para _decode1100
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	Ln              = (*instruction & mask(0b0000, 0111, 0000, 0000)) >> 8;
	registerList    = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;

	fprintf(outputFile, "%s r%d!, {", entry[0][*op]
	       , Ln
	      );

	unsigned char registerNumber = 0;
	char showComma = 0;
	while(registerList)
	{
		if(showComma) {
			fprintf(outputFile, ", ");
		}
		showComma = 0;

		if(registerList & mask(0b0000, 0000, 0000, 0001)) {
			fprintf(outputFile, "r%d", registerNumber);
			showComma = 1;
		}

		registerList >>= 1;
		registerNumber++;
	}

	fprintf(outputFile, "}\n");
}


void _1101_cond_8bitoffset_x_immed8(const unsigned short int* instruction,
                                    unsigned short int* op,
                                    FILE* outputFile
                                   )
{
	char cond;
	char offset;
	char x;
	char immed8;
	char instructionAddress = 0;

	*op = 0;

	// Ponteiro para _decode1101
	char* (*entry)[1] = decodeTable[(*instruction & 0xF000) >> 12];

	if(((*instruction & mask(0b0000, 1111, 0000, 0000)) >> 8) < 0xE) {
		// B<cond>.
		cond    = (*instruction & mask(0b0000, 1111, 0000, 0000)) >> 8;
		offset  = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;
		fprintf(outputFile, "%s%s #%d\n", entry[((*instruction & mask(0b0000, 1111, 0000, 0000)) >> 8) >= 0xE][*op]
		       , decodeTableCond[cond]
		       , instructionAddress + 4 + ((unsigned char)offset*2)
		      );
	}
	else {
		if(((*instruction & mask(0b0000, 1111, 0000, 0000)) >> 8) == 0xE) {
			x = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0 ;
			// Undefined
			fprintf(outputFile, "%s #%d\n", entry[((*instruction & mask(0b0000, 0001, 0000, 0000)) >> 8) + 1][*op]
			       , 0 + 4 + ((unsigned char)x*2)
			      );
		}
		else {
			// (((*instruction & mask(0b0000, 1111, 0000, 0000)) >> 8) == 0xF)
			immed8  = (*instruction & mask(0b0000, 0000, 1111, 1111)) >> 0;
			fprintf(outputFile, "%s #%d\n", entry[2][*op]
			       , 0 + 4 + ((unsigned char)immed8*2)
			      );
		}
	}
}


void _1110_11bitsoffset(const unsigned short int* instruction,
                        unsigned short int* op,
                        FILE* outputFile
                       )
{
	short int offset;
	short int poff = 0;
	char instructionAddress = 0;

	offset  = (*instruction & mask(0b0000, 0111, 1111, 1111)) >> 0;
	*op     = 0;

	// Ponteiro para _decode1110
	char* (*entry)[1] = decodeTable[(*instruction & 0xF000) >> 12];

	if((*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11) {
		// bit11 == 1 unsigned 10-bit offset
		// offset &= mask(0b0000, 0111, 1111, 1110);
		// incompleto
		// offset &= ~0x1;
		fprintf(outputFile, "%s #%u\n", entry[(*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11][*op]
		       , ((instructionAddress + 4 + (poff << 12) + ((unsigned short int)offset*4)) & ~3)
		      );
	}
	else {
		// bit11 == 0 B signed 11-bit offset
		fprintf(outputFile, "%s #%d\n", entry[(*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11][*op]
		       , instructionAddress + 4 + (offset*2)
		      );
	}
}

void _1111_11bitprefixsoffset(const unsigned short int* instruction,
                              unsigned short int* op,
                              FILE* outputFile
                             )
{
	short int offset;
	short int poff = 0;
	char instructionAddress = 0;

	offset  = (*instruction & mask(0b0000, 0111, 1111, 1111)) >> 0;
	*op     = 0;

	// Ponteiro para _decode1111
	char* (*entry)[2] = decodeTable[(*instruction & 0xF000) >> 12];

	if((*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11) {
		// bit11 == 1 usigned  11-bit offset
		// incompleto
		fprintf(outputFile, "%s #%u\n", entry[(*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11][*op]
		       , instructionAddress + 4 + (poff << 12) + (offset*2)
		      );
	}
	else {
		// bit11 == 0 B signed 11-bit prefix offset poff
		// incompleto nao sei como funciona
		fprintf(outputFile, "%s #%d\n", entry[(*instruction & mask(0b0000, 1000, 0000, 0000)) >> 11][*op]
		       , instructionAddress + 4 + (poff << 12) + (offset*2)
		      );
	}
}

// char* (*decodeTable[])[2] = {
void* decode_handler[16] = {
	&_0000_immed5_Lm_Ld,
	&_0001_immed3Lm_Ln_Ld,
	&_0010_LdLn_immed8,
	&_0011_Ld_immed8,

	&_0100_LmLs_Ld__Lm_LdLn__Lm_Ld_Hm7_Ld__Lm_Hd7__Hm7_Hd7__Hm7_Ln__Lm_Hn7__Hm7_Hn7,
	&_0101_Lm_LnLd,
	&_0110_immed5_Ln_Ld,
	&_0111_immed5_Ln_Ld,

	&_1000_immed5_Ln_Ld,
	&_1001_Ld_immed8,
	&_1010_Ld_immed8,
	&_1011_immed7__Lm_Ld__registerList__op__aif__immed8,

	&_1100_Ln_registerList,
	&_1101_cond_8bitoffset_x_immed8,
	&_1110_11bitsoffset,
	&_1111_11bitprefixsoffset
};

void hex_printf(unsigned short int instruction) {
	char i = 4*3;
	char number = 0;
	printf("0x");

	while(i >= 0) {

		number = (instruction >> i) & mask(0b0000, 0000, 0000, 1111);

		printf("%c", number > 9 && number < 16 ? number -10 + 'a' : number + '0');

		i-=4;
	}

	printf("\t");
}

char* decoder(unsigned short int* instruction, unsigned short int* op, FILE* outputFile) {
	// 0000             0000 00000 00000
	// deslocamento     valor & mascara

	typedef void (*func)(unsigned short int*, unsigned short int*, FILE*);
	func entry = decode_handler[(*instruction & 0xF000) >> 12];
// 	hex_printf(instruction);
	fprintf(outputFile, "0x%x\t", *instruction);
	(*entry)(instruction, op, outputFile);
	fprintf(outputFile, "entry: %d, op: %d\n", (*instruction & 0xF000) >> 12, *op);

	return NULL;
}

int readInstruction(FILE** inputFile, unsigned short int* instruction){
    // char line[6]; // por segurança 8
    // return fgets(line, sizeof(line), *inputFile) != NULL? sscanf(line, "%4hx", instruction) : 0;
    return fscanf(*inputFile, "%4hx", instruction) != EOF;
}

unsigned long int sizeFile(FILE* file){
    struct stat buf;
    fstat(fileno(file), &buf);
    return buf.st_size;
}

// Return non-zero if the directory exists
int existingDirectory(const char* pathname) {
    struct stat statbuf;
    return (stat(pathname, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
}

// On success, non-zero is returned
int createDirectory(const char* pathname) {
    return existingDirectory(pathname) ? 0 : (mkdir(pathname, 0755) == 0);
}

int findAndCreateFolder(const char* pathname, char* dir) {
    DIR* directory = opendir(".");
    struct dirent* entry;

    if (directory == NULL) {
        perror("Não foi possível abrir o diretório");
        return 0;
    }

    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_type == DT_DIR) {
            if (strstr(entry->d_name, pathname) != NULL) {
                strcpy(dir, entry->d_name);
                closedir(directory);
                return 1;
            }
        }
    }
    
    createDirectory(pathname);
    strcpy(dir, pathname);
    
    closedir(directory);    
    
    return 0;
}

int checkFilesInDirectory(FILE* file, int count){
    fread(&count, sizeof(int), 1, file);
    return count;
}

int countFilesInDirectory(const char* pathname) {
    DIR* directory = opendir(pathname);
    struct dirent* entry;
    int fileCount = 0;

    if (directory == NULL) {
        perror("Não foi possível abrir o diretório");
        return -1; // Retorna -1 para indicar erro
    }

    while ((entry = readdir(directory)) != NULL) {
        // Verifica se é um arquivo regular
        if (entry->d_type == DT_REG) {
            fileCount++;
        }
    }

    closedir(directory);
    return fileCount;
}

int copyFileB(char* fReadPath, char* fWritePath, unsigned long int startPos, unsigned long int n) {
    FILE* readingFile = fopen(fReadPath, "rb");
    FILE* writingFile = fopen(fWritePath, "wb");
    
    if (readingFile == NULL) {
        return 1;
    }
    
    if (writingFile == NULL) {
        fclose(readingFile);
        return 2;
    }
    
    unsigned long int inputFileSize = sizeFile(readingFile);
    
    if (startPos >= inputFileSize) {
        fclose(readingFile);
        fclose(writingFile);
        return 3;
    }
    
    startPos = ((startPos + 1) / INSTRUCTION_SIZE_REAL) * INSTRUCTION_SIZE_REAL;
    
    fseek(readingFile, startPos, SEEK_SET);
    
    char buffer[5];
    size_t bytesRead;
    unsigned long int bytesToWrite = (n > 0) ? n : inputFileSize - startPos;
    unsigned long int totalBytesWritten = 0;
    
    while (totalBytesWritten < bytesToWrite && (bytesRead = fread(buffer, 1, sizeof(buffer), readingFile)) > 0) {
        if (totalBytesWritten + bytesRead > bytesToWrite) {
            bytesRead = bytesToWrite - totalBytesWritten;
        }
        fwrite(buffer, 1, bytesRead, writingFile);
        totalBytesWritten += bytesRead;
    }
    
    fclose(readingFile);
    fclose(writingFile);
    
    return 0;
}

void toggleStandardIO(int restore) {
    static int originalStdin = -1;
    static int originalStdout = -1;
    static int originalStderr = -1;

    if (!restore) {
        // Salva os descritores de arquivo padrão redirecionados na primeira chamada
        if (originalStdin == -1) originalStdin = dup(STDIN_FILENO);
        if (originalStdout == -1) originalStdout = dup(STDOUT_FILENO);
        if (originalStderr == -1) originalStderr = dup(STDERR_FILENO);

        // Restaura os padrões para o terminal
        freopen("/dev/tty", "r", stdin);
        freopen("/dev/tty", "w", stdout);
        freopen("/dev/tty", "w", stderr);
    } else {
        // Restaura os descritores de arquivo redirecionados salvos
        if (originalStdin != -1) {
            dup2(originalStdin, STDIN_FILENO);
            close(originalStdin);
            originalStdin = -1; // Reset para permitir alternância múltipla
        }
        if (originalStdout != -1) {
            dup2(originalStdout, STDOUT_FILENO);
            close(originalStdout);
            originalStdout = -1; // Reset para permitir alternância múltipla
        }
        if (originalStderr != -1) {
            dup2(originalStderr, STDERR_FILENO);
            close(originalStderr);
            originalStderr = -1; // Reset para permitir alternância múltipla
        }
    }
}

int updateFileCount(FILE* file, int* numberFilesDirectory) {
    fseek(file, 0, SEEK_SET);
    fprintf(file, "%d\n", numberFilesDirectory[0]);
    fprintf(file, "%d\n", numberFilesDirectory[1]);
    // fwrite(&count, sizeof(int), 2, file);
    return fflush(file);
}

typedef struct {
    char stdinPath[FILE_PATH_SIZE];
    char stdoutPath[FILE_PATH_SIZE];
    char stderrPath[FILE_PATH_SIZE];
    unsigned char redirectionStatus;
} FileIOPaths;

typedef struct {
    FILE* stdinFile;
    FILE* stdoutFile;
    FILE* stderrFile;
	FILE* inputFile;
	FILE* outputFile;
	FILE* errorFile;
    FILE* statusFile;
} FileDescriptors;

int configureRedirectionStatus(FileIOPaths* _paths){
    _paths->redirectionStatus = (!isatty(STDERR_FILENO) << 2)
                              | (!isatty(STDOUT_FILENO) << 1)
                              | (!isatty(STDIN_FILENO) << 0);

    return _paths->redirectionStatus;
}

int updateRedirectionPaths(FileIOPaths* _paths){
    char path[64];
    const char* streamType[] = {
        "IN",
        "OUT",
        "ERROR"
    };
    
    char* target[] = {_paths->stdinPath, _paths->stdoutPath, _paths->stderrPath};
    
    ssize_t len;
    
    printf("IN %s, OUT %s, ERROR %s\n", _paths->redirectionStatus & 1 ? "redirected" : "stdin"
                                      , _paths->redirectionStatus & 2 ? "redirected" : "stdout"
                                      , _paths->redirectionStatus & 4 ? "redirected" : "stderr"
          );
    
    for(int i = 0; i < 3; i++) {
        if(_paths->redirectionStatus & (1 << i)){
            snprintf(path, sizeof(path), "/proc/self/fd/%d", i);
            len = readlink(path, path, sizeof(path) - 1);
            if (len != -1) {
                path[len] = '\0'; // Termina a string
                printf("%s: Folder %s in PATH: %s\n", streamType[i], basename(path), path);
                strcpy(target[i], path);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    // #line 0
    int numberFilesDirectory[2];
	unsigned short int instruction;
    unsigned short int op;
    
    char indirectory[32];
    char outdirectory[32];
    char errordirectory[32];
    
    FileIOPaths _paths = {
        0,
        0,
        0,
        0,
    };
    
	FILE* inputFile;
	FILE* outputFile;
	FILE* errorFile;
    FILE* statusFile;

    findAndCreateFolder(DEFAULT_IN_DIRECTORY_NAME, indirectory);
    findAndCreateFolder(DEFAULT_OUT_DIRECTORY_NAME, outdirectory);
    
    // numberFilesInDirectory;
    numberFilesDirectory[0] = countFilesInDirectory(indirectory);
    // numberFilesOutDirectory;
    numberFilesDirectory[1] = countFilesInDirectory(outdirectory);
    
    statusFile = fopen(DEFAULT_STATUS_FILE_NAME,"w+");
    
    updateFileCount(statusFile, &numberFilesDirectory[0]);
	
	configureRedirectionStatus(&_paths);
	
    updateRedirectionPaths(&_paths);

    // printf("Input file path %s\n", _paths.stdinPath);
    // printf("Output file path %s\n", _paths.stdoutPath);
    // printf("Error file path %s\n", _paths.stderrPath);
    
    unsigned long int fileIndex;
    unsigned long int inputFileSize;
    unsigned long int inputFileSepareted;
    unsigned long int bytesRead;
    
    for(
        fileIndex = (argc -1) + ((_paths.redirectionStatus & 1) >> 0), inputFileSize = FILE_SIZE_LIMIT;
        fileIndex > 0;
        fileIndex--
       )
    {
        
        if(inputFileSize <= FILE_SIZE_LIMIT){
            if(_paths.redirectionStatus & 1){
                inputFile = stdin;
                _paths.redirectionStatus &= ~1;
            }else{
                snprintf(_paths.stdinPath, sizeof(_paths.stdinPath), "%s", *(argv + fileIndex));
                inputFile = fopen(_paths.stdinPath, "r+");
            }
            
            inputFileSize = sizeFile(inputFile);
            
            inputFileSepareted = (inputFileSize+FILE_SIZE_LIMIT-1)/FILE_SIZE_LIMIT;
            fileIndex += inputFileSepareted - 1;
        }
        
        char destinationPath[FILE_PATH_SIZE];
        {
            toggleStandardIO(0);
            printf("Size in bytes: %lu\n", inputFileSize);
            printf("Files separeted: %lu\n", inputFileSepareted);
            
            snprintf(destinationPath, sizeof(destinationPath), "%s/""input%d.out", indirectory, numberFilesDirectory[0]);
            printf("Files in folder %s via file %s\n", destinationPath,  _paths.stdinPath != 0 ? _paths.stdinPath : *(argv + fileIndex));
            
            copyFileB(_paths.stdinPath, destinationPath, ftell(inputFile), FILE_SIZE_LIMIT);
            
            snprintf(destinationPath, sizeof(destinationPath), "%s/""output%d.out", outdirectory, numberFilesDirectory[1]);
            printf("Files in folder %s via file %s\n", destinationPath,  _paths.stdinPath != 0 ? _paths.stdinPath : *(argv + fileIndex));
            
            toggleStandardIO(1);
            
            outputFile = fopen(destinationPath, "w+");
        }
        
        {
            numberFilesDirectory[0] += (inputFile != NULL);
            numberFilesDirectory[1] += (outputFile != NULL);

            updateFileCount(statusFile, &numberFilesDirectory[0]);
        }
        
        // printf("stdin = %p\n", stdin);
        // printf("inputFilet = %p\n", inputFile);
        // printf("stdout = %p\n", stdout);
        // printf("outputFile = %p\n", outputFile);
        
        // printf("%d Files in folder %s via file %s\n", checkFilesInDirectory(statusFile, numberFilesDirectory[0]), indirectory, "statusFile");
        // printf("%d Files in folder %s via file %s\n", checkFilesInDirectory(statusFile, numberFilesDirectory[1]), outdirectory, "statusFile");
        // printf("Cursor position %lu in file %s\n", ftell(statusFile), "statusFile");
        // printf("Cursor position %lu in file %s\n", ftell(inputFile), "inputFile");
        // printf("Cursor position %lu in file %s\n", ftell(outputFile), "outputFile");
        
        if((inputFileSize > 0) && !feof(inputFile) && !feof(outputFile))
        {
            bytesRead = ftell(inputFile);
            // bytesRead = 0;
            
            while(
                    ((ftell(inputFile) + 1 - bytesRead)  < FILE_SIZE_LIMIT) &&
                    readInstruction(&inputFile, &instruction)
                    // (bytesRead  < FILE_SIZE_LIMIT) &&
                 )
            {
                decoder(
                        (unsigned short int*)(&instruction),
                        (unsigned short int*)(&op),
                        (FILE*)(outputFile)
                       );       // SaC-da: 42
                
                // bytesRead += INSTRUCTION_SIZE_REAL;
            }
            
            rewind(outputFile);
            
            char outputCharacter[128];
            while(fgets(outputCharacter, sizeof(outputCharacter), outputFile))
            {
                printf("%s", outputCharacter);
            }
            
        }
        
        
        if((ftell(inputFile) + 1) >= (inputFileSize)){
            inputFile != NULL ? fclose(inputFile) : printf("inputFile: Nenhum arquivo aberto\n");
            inputFileSize = FILE_SIZE_LIMIT;
            
            {
                toggleStandardIO(0);
                printf("File reading %s completed\n", _paths.stdinPath != 0 ? _paths.stdinPath : *(argv + fileIndex));
                toggleStandardIO(1);
            }
        }
        outputFile != NULL ? fclose(outputFile) : printf("outputFile: Nenhum arquivo aberto\n");
    }
    
    toggleStandardIO(0);
    
    printf("\n========================================\n");
    printf(" line %d \n", __LINE__);
    printf(" Application: %s\n", *argv);
    printf(" File       : %s\n", __FILE__);
    printf(" Compiled   : %s at %s\n", __DATE__, __TIME__);
    printf(" Standard   : %s\n", __STDC__ == 1 ? "ANSI C" : "Not ANSI C");
    printf("========================================\n\n");

    toggleStandardIO(1);

    return 0;
}
