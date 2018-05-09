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

/*
 *  InterpND.cc -- N-dimensional polynomial interpolation
 *
 *  homer reid  -- 3/2011 - 1/2018
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vector>

#include <libhrutil.h>
#include <libhmat.h>
#include "libMDInterp.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/***************************************************************/
/***************************************************************/
/***************************************************************/
bool Increment(iVec &n, iVec &N)
{ 
  for(unsigned d=0; d<n.size(); d++)
   { if ( ++n[d] < N[d] )
      return false;
     n[d]=0;
   }
  return true;
}

double Monomial(dVec XVec, iVec pVec)
{ double Value=1.0;
  for(unsigned d=0; d<pVec.size(); d++)
   for(int p=0; p<pVec[d]; p++)
    Value*=XVec[d];
  return Value;
}

size_t InterpND::GetPointIndex(iVec nVec)
{ size_t Index=0;
  for(int d=0; d<D; d++) 
   Index += nVec[d]*PointStride[d];
  return Index;
}

iVec InterpND::GetPoint(size_t Index)
{ iVec nVec(D);
  for(int d=0; d<D; d++)
   { nVec[d] = Index % (NVec[d]+1);
     Index /= (NVec[d]+1);
   }
  return nVec;
}

size_t InterpND::GetCellIndex(iVec nVec)
{ size_t Index=0;
  for(int d=0; d<D; d++) 
   Index += nVec[d]*CellStride[d];
  return Index;
}

iVec InterpND::GetCell(size_t Index)
{ iVec nVec(D);
  for(int d=0; d<D; d++)
   { nVec[d] = Index % NVec[d];
     Index /= NVec[d];
   }
  return nVec;
}

dVec InterpND::X2X0(dVec XVec)
{ dVec X0Vec = FixedCoordinates;
  for(int d0=0, d=0; d0<D0; d0++)
   if (isinf(X0Vec[d0])) 
    X0Vec[d0] = XVec[d++];
  return X0Vec;
}

dVec InterpND::X02X(dVec X0Vec)
{
  dVec XVec;
  for(int d0=0; d0<D0; d0++)
   if (isinf(FixedCoordinates[d0]))
    XVec.push_back(X0Vec[d0]);
   else if (!EqualFloat(X0Vec[d0],FixedCoordinates[d0]))
    ErrExit("%s:%i: X0Vec[%i]=%e != %e",__FILE__,__LINE__,d0,X0Vec[d0],FixedCoordinates[d0]);
  return XVec;
}

dVec InterpND::n2X(iVec nVec)
{ dVec XVec(D);
  for(int d=0; d<D; d++)
   { int n=nVec[d];
     XVec[d] = (XGrids.size()>0 ? XGrids[d][n] : XMin[d] + n*DX[d]);
   }
  return XVec;
}

dVec InterpND::n2X0(iVec nVec)
{ return X2X0(n2X(nVec)); }
 
/****************************************************************/
/* layout of PhiVDTable:                                        */
/*  Phi values, derivatives for function #1  at point #1        */
/*  Phi values, derivatives for function #2  at point #1        */
/*  ...                                                         */
/*  Phi values, derivatives for function #NF at point #1        */
/*  Phi values, derivatives for function #1  at point #2        */
/*  Phi values, derivatives for function #2  at point #2        */
/*  ...                                                         */
/*  Phi values, derivatives for function #NF at point #NPoint   */
/* layout of PhiVDTable:                                        */
/*  Phi values, derivatives for function #1  at point #1        */
/****************************************************************/
size_t InterpND::GetPhiVDTableOffset(int nPoint, int nFun)
{ return nPoint*NF*NVD0 + nFun*NVD0; }

size_t InterpND::GetPhiVDTableOffset(iVec nVec, iVec tauVec, int nFun)
{
  iVec npVec(D);
  for(int d=0; d<D; d++)
   npVec[d]=nVec[d] + tauVec[d];
  return GetPhiVDTableOffset(GetPointIndex(npVec),nFun);
}

void InterpND::FillPhiVDBuffer(double *PhiVD, double *PhiVD0)
{
  int nvd=0;
  iVec Twos(D0,2);
  LOOP_OVER_IVECS(nvd0, dx0Vec, Twos)
   { bool Skip=false;
     for(int d0=0; d0<D0; d0++)
      if ( !isinf(FixedCoordinates[d0]) && dx0Vec[d0]!=0 ) Skip=true;
     if (Skip) continue;
     PhiVD[ nvd++ ] = PhiVD0[ nvd0 ];
   }
}

/****************************************************************/
/* layout of CTable:                                            */
/*  Coefficients for function #1 in grid cell #1                */
/*  Coefficients for function #2 in grid cell #1                */
/*  ...                                                         */
/*  Coefficients for function #NF in grid cell #1               */
/*  Coefficients for function #1  in grid cell #2               */
/*  ...                                                         */
/*  Coefficients for function #NF in grid cell #NCell           */
/****************************************************************/
size_t InterpND::GetCTableOffset(int nCell, int nFun)
{ return nCell*NF*NCoeff+ nFun*NCoeff; }

size_t InterpND::GetCTableOffset(iVec nVec, int nFun)
{ return GetCTableOffset(GetCellIndex(nVec),nFun); }

/****************************************************************/
/* **************************************************************/
/****************************************************************/

/****************************************************************/
/* class constructor 1: construct the class from a user-supplied*/
/* function and uniform grids                                   */
/****************************************************************/
InterpND::InterpND(dVec X0Min, dVec X0Max, iVec N0Vec, int _NF,
                   PhiVDFunc UserFunc, void *UserData, bool Verbose)
 : NF(_NF)
{ 
  D0=X0Min.size();
  for(int d0=0; d0<D0; d0++)
   if (N0Vec[d0] < 2 || EqualFloat(X0Min[d0],X0Max[d0]) )
    FixedCoordinates.push_back( X0Min[d0] );
   else
    { FixedCoordinates.push_back( HUGE_VAL );
      NVec.push_back( N0Vec[d0] );
      XMin.push_back( X0Min[d0] );
      DX.push_back(  (X0Max[d0] - X0Min[d0]) / N0Vec[d0] );
    }

  Initialize(UserFunc, UserData, Verbose);
}

/****************************************************************/
/* class constructor 2: construct the class from a user-supplied*/
/* function and user-supplied grids for each variable           */
/****************************************************************/
InterpND::InterpND(vector<dVec> &X0Grids, int _NF,
                   PhiVDFunc UserFunc, void *UserData, bool Verbose)
  : NF(_NF)
 
{ 
  D0=X0Grids.size();
  for(int d0=0; d0<D0; d0++)
   if ( X0Grids[d0].size() == 1 )
    FixedCoordinates.push_back( X0Grids[d0][0] );
   else
    { FixedCoordinates.push_back( HUGE_VAL );
      XGrids.push_back(X0Grids[d0]);
      NVec.push_back( X0Grids[d0].size() - 1 );
    }

  Initialize(UserFunc, UserData, Verbose);
}

/****************************************************************/
/****************************************************************/
/****************************************************************/
InterpND::~InterpND()
{ 
  if (CTable)
   free(CTable);
}

/****************************************************************/
/****************************************************************/
/****************************************************************/
void InterpND::Initialize(PhiVDFunc UserFunc, void *UserData, bool Verbose)
{
   /*--------------------------------------------------------------*/
   /*- compute some statistics ------------------------------------*/
   /*--------------------------------------------------------------*/
   D = NVec.size();
   CellStride.reserve(D);
   PointStride.reserve(D);
   CellStride[0]=PointStride[0]=1;
   size_t NCell  = NVec[0];        // # grid cells
   size_t NPoint = NVec[0]+1;      // # grid points
   for(int d=1; d<D; d++)
    { CellStride[d] = NCell;
      PointStride[d] = NPoint;
      NCell  *= NVec[d];
      NPoint *= NVec[d]+1;
      if (NVec[d]<1) 
       ErrExit("%s:%i: invalid number of points (%i) in dimension %i",__FILE__,__LINE__,NVec[d],d);
    }

   NVD    = (1<<D);     // # function vals, derivs per grid point
   NCoeff = NVD*NVD;    // # polynomial coefficients per grid cell

   D0     = FixedCoordinates.size();
   NVD0   = (1<<D0);

   size_t PhiVDTableSize = (NPoint * NF * NVD0) * sizeof(double);
   double *PhiVDTable = (double *)mallocEC(PhiVDTableSize);
   size_t CTableSize = (NCell * NF * NCoeff) * sizeof(double);
   CTable=(double *)mallocEC(CTableSize);
   if (!PhiVDTable || !CTable)
    ErrExit("%s:%i:out of memory",__FILE__,__LINE__);

   /*--------------------------------------------------------------*/
   /*- call user's function to populate table of function values   */
   /*- and derivatives at grid points                              */
   /*--------------------------------------------------------------*/
   int ThreadTaskThreshold = 16; CheckEnv("SCUFF_INTERP_MIN_TASKS",&ThreadTaskThreshold);
   int NumThreads = ( ((int)NPoint) <= ThreadTaskThreshold ? 1 :  GetNumThreads() );
   if (Verbose)
    Log("Evaluating interpolation function at %i points (%i threads)",NPoint,NumThreads);
#ifdef USE_OPENMP
#pragma omp parallel for num_threads(NumThreads)
#endif
   for(size_t nPoint=0; nPoint<NPoint; nPoint++)
    {
      if (Verbose) LogPercent(nPoint,NPoint);
      size_t Offset = GetPhiVDTableOffset(nPoint,0);
      dVec X0Vec  = n2X0(GetPoint(nPoint));
      UserFunc(&(X0Vec[0]), UserData, PhiVDTable + Offset);
    }

   /*--------------------------------------------------------------*/
   /*- construct the NCoeff x NCoeff matrix that operates on a     */
   /*- vector of polynomial coefficients to yield a vector of      */
   /*- function values and derivatives at grid points bounding     */
   /*- a single grid cell                                          */
   /*--------------------------------------------------------------*/
   int NEq    = NCoeff;     // # matching equations per grid cell
   HMatrix *M = new HMatrix(NEq, NCoeff);
   M->Zero();
   iVec Twos(D,2), Fours(D,4);
   // yPowers[0,1][p] = (\pm 1)^p
   double yPowers[2][4]={ {1.0,-1.0,1.0,-1.0}, {1.0, 1.0, 1.0, 1.0} };
  {
   LOOP_OVER_IVECS(nTau, tauVec, Twos)
    { LOOP_OVER_IVECS(nVD, sigmaVec, Twos)
       { int nEq = nTau*NVD + nVD; // index of equation [0,4^D-1]
         LOOP_OVER_IVECS(nCoeff, pVec, Fours)
          { double Entry=1.0;
            for(int d=0; d<D; d++)
             { int tau=tauVec[d], sigma=sigmaVec[d], p=pVec[d];
               if (sigma)
                Entry *= (p==0 ? 0.0 : p*yPowers[tau][p-1]);
               else
                Entry *= yPowers[tau][p];
             }
            M->SetEntry(nEq, nCoeff, Entry);
          }
       }
    }
  }
   M->LUFactorize();

   /*--------------------------------------------------------------*/
   /*- loop over grid cells; for each grid cell and each component */
   /*- of the function vector, populate a vector of function values*/
   /*- and derivatives at the grid-cell corners, then solve linear */
   /*- system to compute polynomial coefficients for this grid cell*/
   /*--------------------------------------------------------------*/
   double *PhiVDBuffer = (D==D0 ? 0 : (double *)mallocEC(NVD*sizeof(double)) );
   LOOP_OVER_IVECS(nCell, nVec, NVec)
    for(int nf=0; nf<NF; nf++)
     { 
       // vector of scaling factors to accomodate grid-cell dimensions
       dVec D2Vec(D); // DeltaOver2 vector
       for(int d=0; d<D; d++)
        if (XGrids.size()==0)
         D2Vec[d] = 0.5*DX[d];
        else 
         { size_t n=nVec[d], np1=n+1;
           if (np1 >= XGrids[d].size() )
            { np1 = XGrids[d].size() - 1;
              n   = np1-1;
            }
           D2Vec[d] = 0.5*(XGrids[d][np1] - XGrids[d][n]);
         }

       // populate RHS vector with function values and derivatives
       // at all corners of grid cell
       HVector RHS(NCoeff, LHM_REAL, CTable + GetCTableOffset(nCell,nf));
       LOOP_OVER_IVECS(nTau, tauVec, Twos)
        { 
          double *PhiVD0 = PhiVDTable + GetPhiVDTableOffset(nVec, tauVec, nf);
          double *PhiVD  = (D==D0) ? PhiVD0 : PhiVDBuffer;
          if (D!=D0)
           FillPhiVDBuffer(PhiVD, PhiVD0);

          LOOP_OVER_IVECS(nSigma, sigmaVec, Twos)
           RHS.SetEntry(nTau*NVD + nSigma, Monomial(D2Vec, sigmaVec)*PhiVD[nSigma]);
        }

       // operate with inverse M matrix to yield C coefficients
       M->LUSolve(&RHS);
     }

   free(PhiVDTable);
   if (PhiVDBuffer) free(PhiVDBuffer);
   delete M;
}

/****************************************************************/
/****************************************************************/
/****************************************************************/
bool InterpND::PointInGrid(double *X0Vec, int *nVec, double *XBarVec)
{ 
  for(int d0=0, d=0; d0<D0; d0++)
   if ( !isinf(FixedCoordinates[d0]) )
    {
      if (!EqualFloat(X0Vec[d0], FixedCoordinates[d0])) return false;
    }
   else
    { 
      double *XdGrid = (XGrids.size() > 0 ? &(XGrids[d][0]) : 0);
      double XdMin   = (XGrids.size() > 0 ? 0.0              : XMin[d] );
      double DXd     = (XGrids.size() > 0 ? 0.0              : DX[d] );
      int nd;
      double XdBar;
      if (!FindInterval(X0Vec[d0], XdGrid, NVec[d], XdMin, DXd, &nd, &XdBar))
       return false;
      if (nVec) nVec[d]=nd;
      if (XBarVec) XBarVec[d] = 2.0*(XdBar-0.5);
      d++;
    }
  return true;
}

/****************************************************************/
/****************************************************************/
/****************************************************************/
bool InterpND::Evaluate(double *X0, double *Phi)
{
  /****************************************************************/
  /****************************************************************/
  /****************************************************************/
  iVec nVec(D);
  dVec XBarVec(D);
  if ( !PointInGrid(X0, &(nVec[0]), &(XBarVec[0])) ) return false;
  
  /****************************************************************/
  /* tabulate powers of the scaled/shifted coordinates            */
  /****************************************************************/
  double XBarPowers[MAXDIM][4];
  for(int d=0; d<D; d++)
   { XBarPowers[d][0]=1.0;
     XBarPowers[d][1]=XBarVec[d];
     XBarPowers[d][2]=XBarPowers[d][1]*XBarVec[d];
     XBarPowers[d][3]=XBarPowers[d][2]*XBarVec[d];
   }

  /****************************************************************/
  /* construct the NCOEFF-element vector of monomial values that  */
  /* I dot with the NCOEFF-element vector of coefficients to get  */
  /* the value of the interpolating polynomial                    */
  /****************************************************************/
  memset(Phi, 0, NF*sizeof(double));
  iVec Fours(D,4);
  LOOP_OVER_IVECS(nCoeff,pVec,Fours)
   { double XFactor=1.0;
     for(int d=0; d<D; d++)
      XFactor*=XBarPowers[d][pVec[d]];
     for(int nf=0; nf<NF; nf++)
      { double *C = CTable + GetCTableOffset(nVec, nf);
        Phi[nf] += C[nCoeff]*XFactor;
      }
   }
  return true;
}

/****************************************************************/
/****************************************************************/
/****************************************************************/
double InterpND::PlotInterpolationError(PhiVDFunc UserFunc, void *UserData, char *OutFileName, bool CentersOnly)
{
  FILE *f=fopen(OutFileName,"w");
  if (!f) return 0.0;

  double *PhiExact  = new double[NVD0*NF];
  double *PhiInterp = new double[NF];

  iVec Threes(D, 3);
  double MaxRelErr=0.0;
  LOOP_OVER_IVECS(nCell, nVec, NVec)
   { LOOP_OVER_IVECS(nTau, tauVec, Threes)
      {
        //
        bool Skip=false;
        if (CentersOnly)
         for(int d=0; !Skip && d<D; d++)
          if (tauVec[d]!=1) Skip=true;
        for(int d=0; !Skip && d<D; d++)
         if (tauVec[d]==2 && nVec[d]!=(NVec[d]-1) )
          Skip=true;
        if (Skip) continue;

        dVec X0 = FixedCoordinates;
        for(int d0=0, d=0; d0<D0; d0++)
         if (isinf(X0[d0]))
          { if (XGrids.size() == 0 )
             X0[d0] = XMin[d] + (nVec[d] + 0.5*tauVec[d])*DX[d]; 
            else
             { size_t n = nVec[d], np1=n+1;
               if (np1 >= XGrids[d].size() )
                { n   = XGrids[d].size() - 2;
                  np1 = XGrids[d].size() - 1;
                }
               double Delta = XGrids[d][np1] - XGrids[d][n];
               X0[d0] = XGrids[d][n] + 0.5*tauVec[d]*Delta;
             }
            d++;
          }

        UserFunc(&(X0[0]), UserData, PhiExact);
        Evaluate(&(X0[0]), PhiInterp);

        for(int nf=0; nf<NF; nf++)
         { double Exact  = PhiExact[nf*NVD0+0];
           double Interp = PhiInterp[nf];
           if (fabs(Exact) > 0.0)
            MaxRelErr=fmax(MaxRelErr, fabs(Exact-Interp)/fabs(Exact));
         }

        fprintVec(f,&(X0[0]),D0);
        for(int nf=0; nf<NF; nf++)
         fprintf(f,"%e ",PhiExact[nf*NVD0+0]);
        fprintVecCR(f,PhiInterp,NF);
      }
   }
  fclose(f);
  delete[] PhiExact;
  delete[] PhiInterp;
  return MaxRelErr;
}

/****************************************************************/
/* If PhiVEMatrix is non-null, it should be an NFx2 matrix      */
/* which will be filled in as follows on return:                */
/* Column 1: mean value of Phi at sample points                 */
/* Column 2: mean absolute error in Phi at sample points        */
/****************************************************************/
double GetInterpolationError(PhiVDFunc UserFunc, void *UserData, int NF,
                             dVec XVec, dVec dXVec, bool Verbose,
                             double *MeanRelError, double *MeanAbsError)
{ 
  int D = XVec.size();
  dVec XMax = XVec;
  for(int d=0; d<D; d++) XMax[d]+=2.0*dXVec[d];
  iVec NVec(D,2);

  InterpND Interp(XVec, XMax, NVec, NF, UserFunc, UserData);

  int NVD = (1<<D);     // # function vals, derivs per grid point

  double *PhiExact  = new double[NVD*NF];
  double *PhiInterp = new double[NF];

  bool OwnBuffers=false;
  if (MeanRelError==0 || MeanAbsError==0)
   { MeanRelError = new double[D*NF];
     MeanAbsError = new double[D*NF];
     OwnBuffers=true;
   }
  memset(MeanRelError, 0, D*NF*sizeof(double));
  memset(MeanAbsError, 0, D*NF*sizeof(double));

  double NumSamples = (double)(1<<(D-1));
  iVec Twos(D,2);
  for(int d=0; d<D; d++)
   {
     LOOP_OVER_IVECS(nTau, tauVec, Twos)
      {
        if (tauVec[d]!=0) continue;

        dVec XSample(D);
        for(int dd=0; dd<D; dd++)
         XSample[dd] = XVec[dd] + (dd==d ? 0.5 : tauVec[dd])*dXVec[dd];

        UserFunc(&(XSample[0]), UserData, PhiExact);
        Interp.Evaluate(&(XSample[0]), PhiInterp);

        for(int nf=0; nf<NF; nf++)
         { double Error = fabs(PhiExact[nf*NVD+0] - PhiInterp[nf])/NumSamples;
           MeanAbsError[d*NF + nf] += Error;
           if ( fabs(PhiExact[nf*NVD+0])!=0.0 )
            MeanRelError[d*NF + nf] += Error/fabs(PhiExact[nf*NVD+0]);
         }
      }
   }

  double MaxRE = 0.0;
  for(int nfd=0; nfd<NF*D; nfd++)
   MaxRE = fmax(MaxRE, MeanRelError[nfd]);

  delete[] PhiExact;
  delete[] PhiInterp;
  if (OwnBuffers)
   { delete[] MeanAbsError;
     delete[] MeanRelError;
   }

  return MaxRE;
}

/***************************************************************/
/***************************************************************/
/***************************************************************/
dVec GetXdGrid(PhiVDFunc UserFunc, void *UserData, int NF, dVec X0, int d,
               double XdMin, double XdMax, double DesiredMaxRE, bool Verbose)
{ 
  bool ExtraVerbose = CheckEnv("SCUFF_INTERPOLATION_EXTRA_VERBOSE");
  Log("Autotuning interpolation grid for coordinate %i, range {%e,%e}",d,XdMin,XdMax);
  if (X0.size() != 1)
   { Log(" X0 = { ");
     for(size_t d0=0; d0<X0.size(); d0++)
      { if ( ((int)d0) == d ) 
         LogC("xx");
        else 
         LogC("%e",X0[d0]);
        LogC("%c",d0==X0.size()-1 ? '}' : ',');
      }
   }
  
  double DeltaMin = (XdMax - XdMin) / (1000.0);
  double DeltaMax = (XdMax - XdMin) / 2.0;
  double Delta    = (XdMax - XdMin) / (10.0); // initial guess
 
  double Xd = XdMin;
  dVec XdGrid(1,Xd);
  while( Xd<XdMax )
   { 
     dVec X0Vec = X0;  X0Vec[d] = Xd;
     bool TooBig=false, JustRight=false;
     while(!JustRight)
      { dVec DeltaVec(X0.size(), 0.0); DeltaVec[d]=Delta;
        double Err=GetInterpolationError(UserFunc, UserData, NF, X0Vec, DeltaVec, false);
        if (ExtraVerbose) Log("   x%i=%+f, Delta=%f: MRE %.2e",d,Xd,Delta,Err);
        if ( Err > 10.0*DesiredMaxRE )
         { TooBig=true; Delta *= 0.2; }
        else if ( Err > DesiredMaxRE )
         { TooBig=true; Delta *= 0.75; }
        else if ( Err<0.1*DesiredMaxRE )
         { if (TooBig) 
            JustRight=true; // prevent oscillatory behavior
           else
            Delta *= 2.0; 
         }
        else
         JustRight = true;

        if (Delta<DeltaMin || Delta>DeltaMax)
         { Delta = (Delta<DeltaMin ? DeltaMin : DeltaMax);
           if (ExtraVerbose) Log("   setting Delta=%f",Delta);
           JustRight = true;
         }
      }
     Xd+=Delta;
     if (Xd>XdMax) Xd=XdMax;
     XdGrid.push_back(Xd);
   }
  Log(" ...%i grid points",XdGrid.size()); 
  return XdGrid;
}