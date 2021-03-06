/****************************************************************************\
Copyright (c) 2002, NVIDIA Corporation.

NVIDIA Corporation("NVIDIA") supplies this software to you in
consideration of your agreement to the following terms, and your use,
installation, modification or redistribution of this NVIDIA software
constitutes acceptance of these terms.  If you do not agree with these
terms, please do not use, install, modify or redistribute this NVIDIA
software.

In consideration of your agreement to abide by the following terms, and
subject to these terms, NVIDIA grants you a personal, non-exclusive
license, under NVIDIA's copyrights in this original NVIDIA software (the
"NVIDIA Software"), to use, reproduce, modify and redistribute the
NVIDIA Software, with or without modifications, in source and/or binary
forms; provided that if you redistribute the NVIDIA Software, you must
retain the copyright notice of NVIDIA, this notice and the following
text and disclaimers in all such redistributions of the NVIDIA Software.
Neither the name, trademarks, service marks nor logos of NVIDIA
Corporation may be used to endorse or promote products derived from the
NVIDIA Software without specific prior written permission from NVIDIA.
Except as expressly stated in this notice, no other rights or licenses
express or implied, are granted by NVIDIA herein, including but not
limited to any patent rights that may be infringed by your derivative
works or by other works in which the NVIDIA Software may be
incorporated. No hardware is licensed hereunder. 

THE NVIDIA SOFTWARE IS BEING PROVIDED ON AN "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED,
INCLUDING WITHOUT LIMITATION, WARRANTIES OR CONDITIONS OF TITLE,
NON-INFRINGEMENT, MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
ITS USE AND OPERATION EITHER ALONE OR IN COMBINATION WITH OTHER
PRODUCTS.

IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT,
INCIDENTAL, EXEMPLARY, CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, LOST PROFITS; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) OR ARISING IN ANY WAY
OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE
NVIDIA SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT,
TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF
NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\****************************************************************************/

//
// cgcmain.c
//

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "slglobals.h"

// Profile registration functions:

extern "C" {
int RegisterProfiles_generic(CgStruct *Cg);
int RegisterProfiles_wii(CgStruct *Cg);
int InitHAL_generic(slHAL *fHAL);
}

static int (*RegistrationFunctions[])(CgStruct *Cg) = {
	RegisterProfiles_generic,
	RegisterProfiles_wii,
};

int CommandLineArgs(int argc, char **argv, int pass);
const char *listFileName;
const char *outputFileName;
const char *sourceFileName;

int main(int argc, char **argv)
{
	const char *copyright = "(c) 2001-2002 NVIDIA Corp.";
	int numerrors = 0, ii;

	if (!InitCgStruct())
		return 1;

	if (!CommandLineArgs(argc, argv, 0))
		return 1;
  
	atable = InitAtomTable(0);
	if (!atable) {
		printf(OPENSL_TAG ": atom table initialization failed.\n");
		return 1;
	}

	RegisterProfile(Cg, &InitHAL_generic, "ps3", 0);
	for (ii = 0; ii < sizeof(RegistrationFunctions)/sizeof(RegistrationFunctions[0]); ii++)
		RegistrationFunctions[ii](Cg);

	if (!InitScanner(Cg))
		return 1;
	if (!CommandLineArgs(argc, argv, 1))
		return 1;
	if (!InitTokenStreams(Cg))
		return 1;
	if (!InitSymbolTable(Cg))
		return 1;
	if (!SetInputFile(sourceFileName))
		return 1;
	if (Cg->options.PrintVersion) {
		printf(OPENSL_TAG ": version %d.%d.%04d%s, build date %s %s.\n",
			HSL_VERSION, HSL_SUB_VERSION, HSL_SUB_SUB_VERSION, NDA_STRING, Build_Date, Build_Time);
	}

	Cg->output = CreateOutput(outputFileName, 1);
	Cg->errors = CreateOutput(NULL, 0);

#if defined(CGC_ENABLE_TOOLS)
	if (Cg->options.Tokenize) {
		TokenizeInput(sourceFileName);
	} else
#endif // defined(CGC_ENABLE_TOOLS)
	{
		if (listFileName)
			Cg->list = CreateOutput(listFileName, 0);

//		Cg->theHAL->PrintCodeHeader(Cg->options.outfd);
//		fprintf(Cg->options.outfd, "%s cgc version %d.%d.%04d%s, build date %s  %s\n", Cg->theHAL->comment, HSL_VERSION, HSL_SUB_VERSION, HSL_SUB_SUB_VERSION, NDA_STRING, Build_Date, Build_Time);
//		PrintOptions(argc, argv);

		if (!Cg->options.NoStdlib) {
			if (!ReadFromTokenStream(&stdlib_cg_stream, AddAtom(atable, "<stdlib>"), StartGlobalScope))
				return 1;
		} else {
			StartGlobalScope(Cg);
		}

	#if defined(_DEBUG) || defined(__GNUC__)
		yyparse();
	#else
		__try {
			yyparse();
		} __except(1) {
			FatalError("*** exception during compilation ***");
		}
	#endif

		if (Cg->options.ErrorMode)
			CheckAllErrorsGenerated();

		if (Cg->options.Fx) {
			CompileTechniques(Cg, Cg->tokenLoc, CurrentScope);
		} else {
			Symbol *program = LookUpSymbol(CurrentScope, AddAtom(atable, Cg->options.entryName));
			if (program) {
				CompileProgram2(Cg, Cg->tokenLoc, CurrentScope, program, &Cg->items);
			} else {
				SemanticError(Cg->tokenLoc, ERROR___NO_PROGRAM);
			}
		}
		numerrors = FreeScanner(Cg);
		while (CurrentScope) {
			if (Cg->options.DumpParseTree)
				PrintScopeDeclarations();
			PopScope();
		}
		if (Cg->options.DumpAtomTable)
			PrintAtomTable(atable);
	}
	FreeSymbolTable(Cg);
	FreeAtomTable(atable);
	CloseOutputFiles("End of program");
	return numerrors;
} // main

void PrintHelp()
{
	slProfile *lProfile;
	int ii;

	printf("usage: cgc [-quiet] [-nocode] [-nostdlib] [-longprogs] [-v] [-Dmacro[=value]] \n           [-profile id] [-entry id] [-o ofile] [file.cg]\n");
#if defined(CGC_DEBUG_THE_COMPILER)
	printf("           [-atom] [-scan] [-tree] [-node] [-final] [-trap] [-comments] [-cdbg0]\n");
#endif
	printf("supported profiles:\n");
	ii = 0;
	while ((lProfile = EnumerateProfiles(ii++)))
		printf("    \"%s\"\n", GetAtomString(atable, lProfile->name));
} // PrintHelp

int CommandLineArgs(int argc, char **argv, int pass)
{
	int ii;

	for (ii = 1; ii < argc; ii++) {
		if (argv[ii][0] == '-') {
			if (!strcmp(argv[ii], "-debug")) {
				if (pass == 0)
					Cg->options.DebugMode = 1;
			} else if (!strcmp(argv[ii], "-nowarn")) {
				if (pass == 0)
					Cg->options.NoWarnings = 1;
			} else if (!strcmp(argv[ii], "-parseonly")) {
				if (pass == 0)
					Cg->options.ParseOnly = 1;
			} else if (!strcmp(argv[ii], "-nostdlib")) {
				if (pass == 0)
					Cg->options.NoStdlib = 1;
			} else if (!strcmp(argv[ii], "-error")) {
				if (pass == 0)
					Cg->options.ErrorMode = 1;
			} else if (!strcmp(argv[ii], "-longprogs")) {
				if (pass == 0)
					Cg->options.AllowLongPrograms = 1;
			} else if (!strcmp(argv[ii], "-posinv")) {
				if (pass == 0)
					Cg->options.PositionInvariant = 1;
			} else if (!strcmp(argv[ii], "-implicitdown")) {
				if (pass == 0)
					Cg->options.AllowImplicitDowncasts = 1;
			} else if (!strcmp(argv[ii], "-v")) {
				if (pass == 0)
					Cg->options.PrintVersion = 1;
			} else if (!strcmp(argv[ii], "-fx")) {
				if (pass == 0)
					Cg->options.Fx = 1;
#if defined(CGC_ENABLE_TOOLS)
			} else if (!strcmp(argv[ii], "-tokenize")) {
				if (pass == 0)
					Cg->options.Tokenize = 1;
#endif // defined(CGC_ENABLE_TOOLS)
#if defined(CGC_DEBUG_THE_COMPILER)
			} else if (!strcmp(argv[ii], "-stages")) {
				if (pass == 0)
					Cg->options.DumpStages = 1;
			} else if (!strcmp(argv[ii], "-dumpgx")) {
				if (pass == 0)
					Cg->options.DumpCode = 1;
			} else if (!strcmp(argv[ii], "-atom")) {
				if (pass == 0)
					Cg->options.DumpAtomTable = 1;
			} else if (!strcmp(argv[ii], "-scan")) {
				if (pass == 0)
					Cg->options.TraceScanner = 1;
			} else if (!strcmp(argv[ii], "-tree")) {
				if (pass == 0)
					Cg->options.DumpParseTree = 1;
			} else if (!strcmp(argv[ii], "-node")) {
				if (pass == 0)
					Cg->options.DumpNodeTree = 1;
			} else if (!strcmp(argv[ii], "-final")) {
				if (pass == 0)
					Cg->options.DumpFinalTree = 1;
			} else if (!strcmp(argv[ii], "-trap")) {
				if (pass == 0)
					Cg->options.TrapOnError = 1;
			} else if (!strcmp(argv[ii], "-comments")) {
				if (pass == 0)
					Cg->options.Comments = 1;
#endif // defined(CGC_DEBUG_THE_COMPILER)
			} else if (!strcmp(argv[ii], "-o")) {
				ii++;
				if (pass == 0) {
					if (ii < argc) {
						outputFileName = argv[ii];
					} else {
						printf(OPENSL_TAG ": missing output file after \"-o\"\n");
						return 0;
					}
				}
			} else if (!strcmp(argv[ii], "-l")) {
				ii++;
				if (pass == 0) {
					if (ii < argc) {
						listFileName = argv[ii];
					} else {
						printf(OPENSL_TAG ": missing listing file after \"-l\"\n");
						return 0;
					}
				}
			} else if (!strcmp(argv[ii], "-help") || !strcmp(argv[ii], "-h")) {
				if (pass == 1)
					PrintHelp();
			} else if (!strcmp(argv[ii], "-profile")) {
				ii++;
				if (pass == 0) {
					if (ii < argc) {
						if (Cg->options.profileString) {
							printf(OPENSL_TAG ": multiple profiles\n");
							return 0;
						}
						Cg->options.profileString = argv[ii];
					} else {
						printf(OPENSL_TAG ": missing profile name after \"-profile\"\n");
						return 0;
					}
				}
			} else if (!strcmp(argv[ii], "-entry")) {
				ii++;
				if (pass == 0) {
					if (ii < argc) {
						if (Cg->options.entryName) {
							printf(OPENSL_TAG ": multiple entries\n");
							return 0;
						}
						Cg->options.entryName = argv[ii];
					} else {
						printf(OPENSL_TAG ": missing entry name after \"-entry\"\n");
						return 0;
					}
				}
			} else if (argv[ii][1] == 'D') {
				if (pass == 1) {
					if (!PredefineMacro(argv[ii]+2)) {
						printf(OPENSL_TAG ": bad argument: \"%s\"\n", argv[ii]);
						return 0;
					}
				}
			} else {
				if (pass == 1) {
					printf(OPENSL_TAG ": bad argument: \"%s\"\n", argv[ii]);
					PrintHelp();
					return 0;
				}
			}
		} else {
			if (pass == 1) {
				if (sourceFileName) {
					printf(OPENSL_TAG ": multiple input files: \"%s\" \"%s\" ...\n", sourceFileName, argv[ii]);
					return 0;
				} else {
					sourceFileName = argv[ii];
				}
			}
		}
	}
	if (pass == 0) {
		if (Cg->options.profileString == NULL) {
			Cg->options.profileString = "generic";
		}
		if (Cg->options.entryName == NULL)
			Cg->options.entryName = "main";
	}
	if (pass == 1) {
		if (!InitHAL(Cg->options.profileString)) {
			printf(OPENSL_TAG ": hal initialization failure.\n");
			return 0;
		}
		char	buffer[64] = "PROFILE_", *t = buffer + strlen(buffer);
		for (const char  *f = Cg->options.profileString; (isalnum(*f) || *f == '_') && t < buffer + sizeof(buffer) - 1; *t++ = toupper(*f++));
		*t = 0;
		PredefineMacro(buffer);
	}
	return 1;
} // CommandLineArgs

