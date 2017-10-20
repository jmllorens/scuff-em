/* Copyright (C) 2005-2011 M. T. Homer Reid
 *
 * This file is part of SCUFF-EM.
 *
 * SCUFF-EM is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * SCUFF-EM is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <fenv.h>
#include <unistd.h>

#include "libhrutil.h"
#include "libSubstrate.h"
#include "libscuff.h"

// infinite silicon half-space
const char SISubstrateFile[]=
 "0.0 CONST_EPS_11.7\n";

using namespace scuff;

#define II cdouble(0.0,1.0)

/***************************************************************/
namespace scuff{
void CalcGC(double R[3], cdouble Omega,
            cdouble EpsR, cdouble MuR,
            cdouble GMuNu[3][3], cdouble CMuNu[3][3],
            cdouble GMuNuRho[3][3][3], cdouble CMuNuRho[3][3][3]);
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
int main(int argc, char *argv[])
{
  InstallHRSignalHandler();
  InitializeLog(argv[0]);

  /*--------------------------------------------------------------*/
  /*- process command-line arguments -----------------------------*/
  /*--------------------------------------------------------------*/
  char *GeoFile=0;
  char *SubstrateFile=0;
  cdouble Omega=0.1;
  char *EPFile=0;
  double XDS[6]={1.0, 0.0, 1.0, 0.0, 0.0, 0.5}; int nXDS=1;
  bool FreeSpace=false;
  bool OmitFreeSpace=false;
  int EntryOnly=-1;
  bool EEOnly=false; 
  bool XYOnly=false;
  /* name, type, #args, max_instances, storage, count, description*/
  OptStruct OSArray[]=
   { {"Geometry",      PA_STRING,  1, 1, (void *)&GeoFile,    0, ".scuffgeo file"},
     {"SubstrateFile", PA_STRING,  1, 1, (void *)&SubstrateFile, 0, ".substrate file"},
     {"Omega",         PA_CDOUBLE, 1, 1, (void *)&Omega,      0, "angular frequency"},
     {"EPFile",        PA_STRING,  1, 1, (void *)&EPFile,     0, "list of evaluation points"},
     {"XDS",           PA_DOUBLE,  6, 1, (void *)XDS,         &nXDS, ""},
     {"FreeSpace",     PA_BOOL,    1, 1, (void *)&FreeSpace,  0, ""},
     {"OmitFreeSpace", PA_BOOL,    0, 1, (void *)&OmitFreeSpace, 0, ""},
     {"EntryOnly",     PA_INT,     1, 1, (void *)&EntryOnly,  0, ""},
     {"EEOnly",        PA_BOOL,    0, 1, (void *)&EEOnly,     0, ""},
     {"XYOnly",        PA_BOOL,    0, 1, (void *)&XYOnly,     0, ""},
     {0,0,0,0,0,0,0}
   };
  ProcessOptions(argc, argv, OSArray);

  /***************************************************************/
  /* read substrate                                              */
  /***************************************************************/
  bool OwnsSubstrateFile=false;
  LayeredSubstrate *S=0;
  RWGGeometry *G=0;
  if (GeoFile)
   { G=new RWGGeometry(GeoFile);
     S=G->Substrate;
     printf("Read substrate definition from file %s\n",GeoFile);
   }
  else 
   { 
     if (!SubstrateFile)
      { SubstrateFile=strdup("XXXXXX");
        if ( mkstemp(SubstrateFile) == -1 )
         ErrExit("could not create temporary file");
        FILE *f=fopen(SubstrateFile,"w");
        fprintf(f,SISubstrateFile);
        fclose(f);
        OwnsSubstrateFile=true;
        printf("Using build-in substrate definition.\n");
      };
     S=new LayeredSubstrate(SubstrateFile);
     if (S->ErrMsg)
      ErrExit(S->ErrMsg);
     printf("Read substrate definition from file %s\n",SubstrateFile);
     if (OwnsSubstrateFile)unlink(SubstrateFile);
   };
  S->Describe();

  if (FreeSpace) 
   { S->ForceFreeSpace=true;
     printf("Doing the free-space case foryaf.\n");
   };

  S->EntryOnly = EntryOnly;
  S->EEOnly    = EEOnly;
  S->XYOnly    = XYOnly;
  
  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  HMatrix *XMatrix;
  if (EPFile)
   XMatrix = new HMatrix(EPFile);
  else if (nXDS)
   { 
     XMatrix = new HMatrix(1,6);
     XMatrix->SetEntriesD(0,":",XDS);
     printf("Rho={%e,%e}, ZDest=%e, ZSource=%e\n",XDS[0]-XDS[3],XDS[1]-XDS[4],XDS[2],XDS[5]);
   }
  else
   { 
     #define NUMPTS  150
     #define XMIN   -1.5
     #define XMAX    1.5
     #define DX      (XMAX - XMIN)/(NUMPTS-1)
     XMatrix = new HMatrix(NUMPTS, 6);
     double XXP[6] = {0.0, 0.0, 0.0, 0.25, 0.5, 1.0};
     for(int nx=0; nx<NUMPTS; nx++)
      { XXP[2] = XMIN + ((double)nx)*DX;
        XMatrix->SetEntriesD(nx, "0:5", XXP);
      };
   };

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  FILE *ff=fopen("tFullWaveSubstrate.gp","w");
  fprintf(ff,"XD(x)=(column(1))\n");
  fprintf(ff,"YD(x)=(column(2))\n");
  fprintf(ff,"ZD(x)=(column(3))\n");
  fprintf(ff,"XS(x)=(column(4))\n");
  fprintf(ff,"YS(x)=(column(5))\n");
  fprintf(ff,"ZS(x)=(column(6))\n");
  const char *EM="EM";
  const char *xyz="xyz";
  for(int pMu=0, nc=7; pMu<6; pMu++)
   for(int qNu=0; qNu<6; qNu++, nc+=2)
    { 
      int p  = pMu/3;
      int Mu = pMu%3;
      int q  = qNu/3;
      int Nu = qNu%3;

      int Offset=0;
      fprintf(ff,"rG%c%c%c%cSL(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc);
      fprintf(ff,"iG%c%c%c%cSL(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc+1);
      fprintf(ff,"mG%c%c%c%cSL(x)=(D2($%i,$%i))\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc,Offset+nc+1);

      Offset+=72;
      fprintf(ff,"rG%c%c%c%cSC(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc);
      fprintf(ff,"iG%c%c%c%cSC(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc+1);
      fprintf(ff,"mG%c%c%c%cSC(x)=(D2($%i,$%i))\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc,Offset+nc+1);

      Offset+=72;
      fprintf(ff,"rG%c%c%c%cFS(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc);
      fprintf(ff,"iG%c%c%c%cFS(x)=($%i)\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc+1);
      fprintf(ff,"mG%c%c%c%cFS(x)=(D2($%i,$%i))\n",EM[p],EM[q],xyz[Mu],xyz[Nu],Offset+nc,Offset+nc+1);

    };
  fclose(ff);

  /***************************************************************/
  /***************************************************************/
  /***************************************************************/
  SetConsoleLogging();

  HMatrix *GSL= S->GetSubstrateDGF(Omega, XMatrix, STATIC_LIMIT);

  HMatrix *GFS = new HMatrix(GSL->NR, GSL->NC, LHM_COMPLEX);
  GFS->Zero();
  //if (!FreeSpace && !OmitFreeSpace)
  // AddScriptG0(S, Omega, XMatrix, GFS);

  HMatrix *GSC = S->GetSubstrateDGF(Omega, XMatrix, SURFACE_CURRENT);

  FILE *f=fopen("tFullWaveSubstrate.out","w");
  for(int nx=0; nx<XMatrix->NR; nx++)
   { 
     double XXP[6];
     XMatrix->GetEntriesD(nx,"0:5",XXP);
     fprintVec(f,XXP,6);

     cdouble GVector[36];

     if (FreeSpace)
      { 
        cdouble GMuNu[3][3], CMuNu[3][3], GMuNuRho[3][3][3], CMuNuRho[3][3][3];
        double R[3]; 
        VecSub(XXP+0, XXP+3, R);
        cdouble EpsRel=1.0, MuRel=1.0;
        scuff::CalcGC(R, Omega, EpsRel, MuRel, GMuNu, CMuNu, GMuNuRho, CMuNuRho);
        for(int Mu=0; Mu<3; Mu++)
         for(int Nu=0; Nu<3; Nu++)
          { GVector[(0*3 + Mu)*6 + (0*3 + Nu)]=II*Omega*ZVAC*MuRel*GMuNu[Mu][Nu];
            GVector[(0*3 + Mu)*6 + (1*3 + Nu)]=+1.0*II*Omega*CMuNu[Mu][Nu];
            GVector[(1*3 + Mu)*6 + (0*3 + Nu)]=-1.0*II*Omega*CMuNu[Mu][Nu];
            GVector[(1*3 + Mu)*6 + (1*3 + Nu)]=II*Omega*EpsRel*GMuNu[Mu][Nu]/ZVAC;
          };
      }
     else 
      GSL->GetEntries(nx,":",GVector);
     fprintVec(f,GVector,36);

     GSC->GetEntries(nx,":",GVector);
     fprintVec(f,GVector,36);

     GFS->GetEntries(nx,":",GVector);
     fprintVec(f,GVector,36);
     fprintf(f,"\n");

     if (XMatrix->NR == 1)
      { SetDefaultCD2SFormat("{%+.2e,%+.2e}");
        for(int P=0; P<2; P++)
         for(int Q=0; Q<2; Q++)
          { printf("\n ** Quadrant %c%c:\n\n",EM[P],EM[Q]);
            for(int i=0; i<3; i++)
             printf("%s %s %s || %s %s %s \n",
                    CD2S( GSC->GetEntry(0, (P*3+i)*6 + (Q*3+0) )+GFS->GetEntry(0, (P*3+i)*6 + (Q*3+0) )),
                    CD2S( GSC->GetEntry(0, (P*3+i)*6 + (Q*3+1) )+GFS->GetEntry(0, (P*3+i)*6 + (Q*3+1) )),
                    CD2S( GSC->GetEntry(0, (P*3+i)*6 + (Q*3+2) )+GFS->GetEntry(0, (P*3+i)*6 + (Q*3+2) )),
                    CD2S( GSL->GetEntry(0, (P*3+i)*6 + (Q*3+0) )),
                    CD2S( GSL->GetEntry(0, (P*3+i)*6 + (Q*3+1) )),
                    CD2S( GSL->GetEntry(0, (P*3+i)*6 + (Q*3+2) ))
                   );
          };
      };

   };
  fclose(f);
  printf("Thank you for your support.\n");

}
