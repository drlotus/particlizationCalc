#include <omp.h>
#include <TCanvas.h>
#include <TGraph.h>
#include <TMath.h>
#include <TVector3.h>
#include <TLorentzVector.h>
#include <TRandom3.h>
#include <cmath>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <TF1.h>
#include <TH1D.h>
#include <TSpline.h>
#include <TGraph.h>

#include "DatabasePDG2.h"
#include "gen.h"
#include "particle.h"
#include "const.h"
#include "interpolation.h"

using namespace std;

#define _USE_MATH_DEFINES
const double C_Feq = (pow(0.5 / M_PI / hbarC, 3));

// Levi-Civita symbols
// i,j,k,l = 0...3
int levi(int i, int j, int k, int l)
{
 if((i==j)||(i==k)||(i==l)||(j==k)||(j==l)||(k==l)) return 0;
 else return ( (i-j)*(i-k)*(i-l)*(j-k)*(j-l)*(k-l)/12 );
}

namespace gen {

int Nelem;
double *ntherm, dvMax, dsigmaMax;
TRandom3 *rnd;
DatabasePDG2 *database;
ParticlePDG2 *particle; // chosen particle sort for the polarization calc
int NPART;



struct element {
 double tau, x, y, eta;
 double u[4];
 double dsigma[4];
 double T, mub, muq, mus;
 double dbeta [4][4];
 double dmuCart [4][4]; //derivatives of the 4-velocity in Cartesian coordinates
};

element *surf;
vector<double> pT, phi;
vector<vector<vector<double> > > Pi_num; // numerator of Eq. 34
vector<vector<vector<double> > > Pi_num_navierstokes; // David's contributions
vector<vector<vector<double> > > Pi_num_xi; // additional "xi" term
vector<vector<double> > Pi_den; // denominator of Eq. 34
int nhydros;
TCanvas *plotSymm, *plotAsymm, *plotMod;
TH1D *histMod, *histSymm, *histAsymm;

// Hendrik says that this is a known factor oftenly appearing in distribution functions.
// As example look in SMASH pauli blocking or go an Hendrik's nerves with it
// Found in longer David paper eq. 20
const double c1 = pow(1. / 2. / hbarC / TMath::Pi(), 3.0);

// ######## load the elements
void load(char *filename, int N) {
 double dV, vEff = 0.0, vEffOld = 0.0, dvEff, dvEffOld;
 int nfail = 0, ncut = 0;
 TLorentzVector dsigma;
 Nelem = N;
 surf = new element[Nelem];

 cout << "reading " << N << " lines from  " << filename << "\n";
 ifstream fin(filename);
 if (!fin) {
  cout << "cannot read file " << filename << endl;
  exit(1);
 }
 dvMax = 0.;
 dsigmaMax = 0.;
 // ---- reading loop
 string line;
 istringstream instream;
 for (int n = 0; n < Nelem; n++) {
  getline(fin, line);
  instream.str(line);
  instream.seekg(0);
  instream.clear();  // does not work with gcc 4.1 otherwise
  instream >> surf[n].tau >> surf[n].x >> surf[n].y >> surf[n].eta >>
      surf[n].dsigma[0] >> surf[n].dsigma[1] >> surf[n].dsigma[2] >>
      surf[n].dsigma[3] >> surf[n].u[0] >> surf[n].u[1] >> surf[n].u[2] >>
      surf[n].u[3] >> surf[n].T >> surf[n].mub >> surf[n].muq >> surf[n].mus;
  for(int i=0; i<4; i++)
  for(int j=0; j<4; j++)
   //dbeta is the thermal vorticity 
   instream >> surf[n].dbeta[i][j];

  for(int i=0; i<4; i++)
  for(int j=0; j<4; j++)
   instream >> surf[n].dmuCart[i][j];

  if (instream.fail()) {
   cout << "reading failed at line " << n << "; exiting\n";
   exit(1);
  }
  // calculate in the old way
  dvEffOld =
      surf[n].dsigma[0] * surf[n].u[0] + surf[n].dsigma[1] * surf[n].u[1] +
      surf[n].dsigma[2] * surf[n].u[2] + surf[n].dsigma[3] * surf[n].u[3];
  vEffOld += dvEffOld;
  if (dvEffOld < 0.0) {
   // cout<<"!!! dvOld!=dV " << dvEffOld <<"  " << dV << "  " << surf[n].tau
   // <<endl ;
   nfail++;
  }
  // if(nfail==100) exit(1) ;
  // ---- boost
  // dsigma.SetXYZT(-surf[n].dsigma[1],-surf[n].dsigma[2],-surf[n].dsigma[3],surf[n].dsigma[0])
  // ;
  // dsigma.Boost(-surf[n].u[1]/surf[n].u[0],-surf[n].u[2]/surf[n].u[0],-surf[n].u[3]/surf[n].u[0])
  // ;
  //// ######################################################################
  //// ###  now and later surf.dsigma are boosted to the fluid rest frame  ##
  //// ######################################################################
  // surf[n].dsigma[0] = dsigma.T() ;
  // surf[n].dsigma[1] = -dsigma.X() ;
  // surf[n].dsigma[2] = -dsigma.Y() ;
  // surf[n].dsigma[3] = -dsigma.Z() ;
  // dvEff = surf[n].dsigma[0] ;
  // vEff += dvEff ;
  // if(dvMax<dvEff) dvMax = dvEff ;
  //// maximal value of the weight max(W) = max(dsigma_0+|\vec dsigma_i|)   for
  ///equilibrium DFs
  // if(dsigma.T()+dsigma.Rho()>dsigmaMax) dsigmaMax = dsigma.T()+dsigma.Rho() ;
 }
 // cout<<"..done.\n" ;
 // cout<<"Veff = "<<vEff<<"  dvMax = "<<dvMax<<endl ;
 // cout<<"Veff(old) = "<<vEffOld<<endl ;
 // cout<<"failed elements = "<<nfail<<endl ;
 // cout<<"mu_cut elements = "<<ncut<<endl ;
 //// ---- prepare some stuff to calculate thermal densities
 // NPART=database->GetNParticles() ;
 // cout<<"NPART="<<NPART<<endl ;
 // cout<<"dsigmaMax="<<dsigmaMax<<endl ;
}

void initCalc() {
 for (double _pT = 0.0; _pT < 3.01; _pT += 0.2) {
  pT.push_back(_pT);
 }
 for (double _phi = 0.0; _phi < 2.0*M_PI-1e-5; _phi += M_PI/20) {
  phi.push_back(_phi);
 }
 Pi_num.resize(pT.size());
 Pi_num_navierstokes.resize(pT.size());
 Pi_num_xi.resize(pT.size());
 Pi_den.resize(pT.size());
 for (int i = 0; i < Pi_num.size(); i++) {
  Pi_num[i].resize(phi.size());
  Pi_num_navierstokes[i].resize(phi.size());
  Pi_num_xi[i].resize(phi.size());
  Pi_den[i].resize(phi.size());
  for (int j = 0; j < Pi_num[i].size(); j++) {
   Pi_den[i][j] = 0.0;
   Pi_num[i][j].resize(4);
   Pi_num_navierstokes[i][j].resize(4);
   Pi_num_xi[i][j].resize(4);
   for(int k=0; k<4; k++) {
    Pi_num[i][j][k] = 0.0;
    Pi_num_navierstokes[i][j][k] = 0.0;
    Pi_num_xi[i][j][k] = 0.0;
   }
  }
 }
 nhydros = 0;
 #ifdef PLOTS
 plotSymm = new TCanvas("plotSymm","symmetric derivatives");
 plotAsymm = new TCanvas("plotAsymm","Asymmetric derivatives");
 plotMod = new TCanvas("plotMod","Derivatives");
 histSymm = new TH1D("histSymm", "histSymm", 100, 0., 5.0);
 histAsymm = new TH1D("histAsymm", "histAsymm", 100, -1.2, 0.2);
 histMod = new TH1D("histMod", "histMod", 100, 0., 2.0);
 #endif
}

double ffthermal(double *x, double *par) {
 double &T = par[0];
 double &mu = par[1];
 double &mass = par[2];
 double &stat = par[3];
 return x[0] * x[0] / (exp((sqrt(x[0] * x[0] + mass * mass) - mu) / T) - stat);
}

//double shear_tensor(elememnt* surf, int iel, int mu, int nu)
//double shear_tensor(elememnt surf[], int iel, int mu, int nu)
//double shear_tensor(const elememnt* surf_element, int mu, int nu) -> shear_tensor(&surf_element, int mu, int nu)
double shear_tensor(const element* surf_element, int mu, int nu){
  const double u[4] = {surf_element->u[0], surf_element->u[1], surf_element->u[2], surf_element->u[3]};
  const double u_[4] = {surf_element->u[0], -surf_element->u[1], -surf_element->u[2], -surf_element->u[3]};
  double term_3 = 0., term_4 = 0., term_5 = 0., term_6 = 0., term_7 = 0., term_10 = 0., term_11 = 0.;
  for(int alpha = 0; alpha < 4; alpha++){
    term_3 += u[mu]*u_[alpha]*surf_element->dmuCart[alpha][nu];
    term_4 += u[nu]*u_[alpha]*surf_element->dmuCart[mu][alpha];
    term_5 += u[mu]*u_[alpha]*surf_element->dmuCart[nu][alpha];
    term_6 += u[nu]*u_[alpha]*surf_element->dmuCart[alpha][mu];
    term_10 += gmumu[alpha]*surf_element->dmuCart[alpha][alpha];
    for(int beta = 0; beta < 4; beta++){
      term_7 += 2.*u[mu]*u[nu]*u_[alpha]*u_[beta]*surf_element->dmuCart[alpha][beta];
      term_11 += u_[alpha]*u_[beta]*surf_element->dmuCart[alpha][beta];
    }
  }
  const double shear = 0.5*(surf_element->dmuCart[mu][nu] + surf_element->dmuCart[nu][mu] - term_3 - term_4
                - term_5 - term_6 + term_7) - (1./3.) * (gmunu[mu][nu] - u[mu]*u[nu])
                * (term_10 - term_11);

  return shear;
}

void doCalculations(int pid) {
 const double tvect[4] = {1.,0., 0., 0.};
 particle = database->GetPDGParticle(pid);
 const double mass = particle->GetMass();  // pion
 std::cout << "Lambda mass: " << mass << std::endl;
 const double baryonCharge = particle->GetBaryonNumber();
 const double electricCharge = particle->GetElectricCharge();
 const double strangeness = particle->GetStrangeness();
 cout << "calculations for: " << particle->GetName() << ", charges = "
  << baryonCharge << "  " << electricCharge << "  " << strangeness << endl;
 int nFermiFail = 0; // number of elements where nf>1.0
 int nBadElem = 0;
 double Qx1=0., Qy1=0., Qx2=0., Qy2=0.;
 double z_limits[2];

 // This block is needed to import the coefficient csv file from
 // David and make an interpolation with ROOT
 //**************************************************************
 const std::string coefficient_filename = "/Users/nils/Desktop/Projects/Polarization/Coefficients/coeffData.csv";
 const std::string output_filename = "interpolationTable.txt";
 // Call function to obtain the interpolation TSpline3 object
 const TSpline3* spline = getInterpolationSpline(coefficient_filename);
 if (!spline) {
     std::cerr << "Failed to obtain interpolation spline." << std::endl;
     exit(1);
 }
 saveTableToFile(spline, output_filename);
 //**************************************************************
 for (int iel = 0; iel < Nelem; iel++) {  // loop over all elements
  
  if (iel % 1000 == 0)
   std::cout << "###### iel For-Loop reached element " << iel << " ######\n" << std::endl;

  const double u_[4] = {surf[iel].u[0], -surf[iel].u[1], -surf[iel].u[2], -surf[iel].u[3]};
  const element surf_element = surf[iel];
  const double beta = 1. / surf[iel].T;
  const double z = beta * mass;
  if(z<0.0001 || z>20.0){
    std::cout << "z outside the range [0.0001, 20.0]. Increase interpolation range!!!\n" << std::endl;
  }
  // store the global min/max values of z over all cells
  if(iel == 0) {
    z_limits[0] = z_limits[1] = z;
  } else if(z < z_limits[0]) {
    z_limits[0] = z;
  } else if(z > z_limits[1]) {
    z_limits[1] = z;
  }
  if (iel == Nelem-1){
    std::cout << "Z Range Used During Simulation:" << std::endl;
    std::cout << "-------------------------------\n" << std::endl;
    std::cout << "z_min: " << z_limits[0] << " ,     z_max: " << z_limits[1] << std::endl;  
  }

  // The tuning factor is non-physical and is just to test how large the 
  // xi_delta_coefficient must be in order to match experimental data with 
  // P^z(phi)
  const double tuning_factor = 1.5;
  const double xi_delta_coefficient = spline->Eval(z) * tuning_factor;
  
  if(fabs(surf[iel].dbeta[0][0])>1000.0) nBadElem++;
  //if(fabs(surf[iel].dbeta[0][0])>1000.0) continue;
  for (int ipt = 0; ipt < pT.size(); ipt++)
   for (int iphi = 0; iphi < phi.size(); iphi++) {
    const double mT = sqrt(mass * mass + pT[ipt] * pT[ipt]);
    const double p[4] = {mT, pT[ipt]*cos(phi[iphi]), pT[ipt]*sin(phi[iphi]), 0};
    const double p_[4] = {mT, -pT[ipt]*cos(phi[iphi]), -pT[ipt]*sin(phi[iphi]), 0};
    double pds = 0., pu = 0.;
    for (int mu = 0; mu < 4; mu++) {
     pds += p[mu] * surf[iel].dsigma[mu];
     pu += p[mu] * surf[iel].u[mu] * gmumu[mu];
    }
    const double mutot = surf[iel].mub * baryonCharge
      + surf[iel].muq * electricCharge + surf[iel].mus * strangeness;
    const double nf = c1 / (exp( (pu - mutot) / surf[iel].T) + 1.0);
    if(nf > 1.0) nFermiFail++;
    Pi_den[ipt][iphi] += pds * nf ;
    for(int mu=0; mu<4; mu++)
     for(int nu=0; nu<4; nu++)
      for(int rh=0; rh<4; rh++)
       for(int sg=0; sg<4; sg++) {

        
        //pds = p x dsigma
        //surf[iel].dbeta[ta][rh] = varpi_{mu nu}
        // computing the 'standard' polarization expression
        Pi_num[ipt][iphi][mu] += pds * nf * (1. - nf) * levi(mu, nu, rh, sg)
                                * p_[sg] * surf[iel].dbeta[nu][rh];
        
        //David's formula with extra gmunu because I have shear tensor with upper indices (Euclidean) only
        for(int ta=0; ta<4; ta++) {
          for(int alph=0; alph<4; alph++){
            Pi_num_navierstokes[ipt][iphi][mu] += pds * nf * (1. - nf) * z * xi_delta_coefficient
                                    * beta * levi(mu, nu, rh, sg) * u_[nu] * p_[rh]
                                    * gmunu[sg][alph] * shear_tensor(&surf_element, ta, alph) * p_[ta];
          }
        }
        
        // computing the extra 'xi' term for the polarization
        for(int ta=0; ta<4; ta++)
         Pi_num_xi[ipt][iphi][mu] += pds * nf * (1. - nf) * levi(mu, nu, rh, sg)
                     * p_[sg] * p[ta] / p[0] * tvect[nu]
                     * ( surf[iel].dbeta[rh][ta] + surf[iel].dbeta[ta][rh]);
       }

    Qx1 += p[1] * pds * nf;
    Qy1 += p[2] * pds * nf;
    Qx2 += (p[1]*p[1] - p[2]*p[2])/(pT[ipt]+1e-10) * pds * nf;
    Qy2 += (p[1]*p[2])/(pT[ipt]+1e-10) * pds * nf;
   }
  if(iel%100000==0) cout << "processed " << iel/1000 << "k elements\n";
 }  // loop over all elements
 delete[] surf;
 cout << "doCalculations: total, bad = " << setw(12) << Nelem << setw(12) << nBadElem << endl;
 cout << "number of elements*pT configurations where nf>1.0: " << nFermiFail
  << endl;
 cout << "event_plane_vectors: " << Qx1 << "  " << Qy1 << "  "
   << Qx2 << "  " << Qy2 << endl;

 std::cout << "###### doCalculations finished ######\n" << std::endl;
}

void calcInvariantQuantities() {
 const double tvect[4] = {1.,0., 0., 0.};
 int nBadElem = 0;
 for (int iel = 0; iel < Nelem; iel++) {  // loop over all elements
  if(fabs(surf[iel].dbeta[0][0])>1000.0) nBadElem++;
  double symm_deriv = 0.0, asymm_deriv = 0.0, mod_deriv = 0.0;
  for(int mu=0; mu<4; mu++)
   for(int nu=0; nu<4; nu++) {
    symm_deriv += 0.25*pow(hbarC*(surf[iel].dbeta[mu][nu]+surf[iel].dbeta[nu][mu]),2)
		    *gmumu[mu]*gmumu[nu];
    asymm_deriv += 0.25*pow(hbarC*(surf[iel].dbeta[mu][nu]-surf[iel].dbeta[nu][mu]),2)
		    *gmumu[mu]*gmumu[nu];
    mod_deriv += pow(hbarC*(surf[iel].dbeta[mu][nu]),2)
		    *gmumu[mu]*gmumu[nu];
    //for(int rh=0; rh<4; rh++)
     //for(int sg=0; sg<4; sg++) {
      //// computing the 'standard' polarization expression
      //Pi_num[ipt][iphi][mu] += pds * nf * (1. - nf) * levi(mu, nu, rh, sg)
                              //* p_[sg] * surf[iel].dbeta[nu][rh];
      //// computing the extra 'xi' term for the polarization
      //for(int ta=0; ta<4; ta++)
       //Pi_num_xi[ipt][iphi][mu] += pds * nf * (1. - nf) * levi(mu, nu, rh, sg)
                   //* p_[sg] * p[ta] / p[0] * tvect[nu]
                   //* ( surf[iel].dbeta[rh][ta] + surf[iel].dbeta[ta][rh]);
     //} // mu-nu-rho-sigma loop
    } // mu-nu loop
    if(fabs(surf[iel].eta)<0.5) {
     //sqroot1 = sqrt(sqroot1);
     if(symm_deriv!=symm_deriv)
      cout << "symm_deriv=nan\n";
     histSymm->Fill(symm_deriv);
     histAsymm->Fill(asymm_deriv);
     histMod->Fill(mod_deriv);
   }
 }  // loop over all elements
 histSymm->Scale(1./histSymm->Integral(),"width");
 histAsymm->Scale(1./histAsymm->Integral(),"width");
 histMod->Scale(1./histMod->Integral(),"width");
 plotSymm->cd();
 histSymm->Draw("H");
 plotAsymm->cd();
 histAsymm->Draw("H");
 plotMod->cd();
 histMod->Draw("H");
}

void calcEP1() {
 particle = database->GetPDGParticle(2112);
 const double mass = particle->GetMass();  // pion
 int nBadElem = 0;
 double Qx1=0., Qy1=0., Qx2=0., Qy2=0.;
 const double coshY = cosh(1.0);
 const double sinhY = sinh(1.0);
 const double pT = 1.0;
 int nFFail = 0;
 for (int iel = 0; iel < Nelem; iel++) {  // loop over all elements
  if(fabs(surf[iel].dbeta[0][0])>1000.0) nBadElem++;
  //if(fabs(surf[iel].dbeta[0][0])>1000.0) continue;
  for (int iphi = 0; iphi < phi.size(); iphi++) {
   double mT = sqrt(mass * mass + pT * pT);
   const double sin_phi = sin(phi[iphi]);
   const double cos_phi = cos(phi[iphi]);
   double p1[4] = {mT*coshY, pT*cos_phi, pT*sin_phi, mT*sinhY};
   double p2[4] = {mT*coshY, pT*cos_phi, pT*sin_phi, -mT*sinhY};
   double pds1 = 0., pds2 = 0., pu1 = 0., pu2 = 0.;
   for (int mu = 0; mu < 4; mu++) {
    pds1 += p1[mu] * surf[iel].dsigma[mu];
    pu1 += p1[mu] * surf[iel].u[mu] * gmumu[mu];
    pds2 += p2[mu] * surf[iel].dsigma[mu];
    pu2 += p2[mu] * surf[iel].u[mu] * gmumu[mu];
   }
   const double f1 = c1 * exp( - pu1 / surf[iel].T );
   const double f2 = c1 * exp( - pu2 / surf[iel].T );
   if(f1 > 1.0) nFFail++;
   Qx1 += p1[1] * pds1 * f1;
   Qy1 += p1[2] * pds1 * f1;
   Qx2 += p2[1] * pds2 * f2;
   Qy2 += p2[2] * pds2 * f2;
  }
 }  // loop over all elements
 cout << "EP1_vectors: " << Qx1 << "  " << Qy1 << "  "
   << Qx2 << "  " << Qy2 << endl;
 cout << "EP_angles: " << atan2(Qy1, Qx1) << "  " << atan2(Qy2, Qx2) << endl;
}

void outputPolarization(char *out_file) {
 ofstream fout(out_file);
 if (!fout) {
  cout << "I/O error with " << out_file << endl;
  exit(1);
 }
 for (int ipt = 0; ipt < pT.size(); ipt++)
  for (int iphi = 0; iphi < phi.size(); iphi++) {
   fout << setw(14) << pT[ipt] << setw(14) << phi[iphi]
     << setw(14) << Pi_den[ipt][iphi];
   for(int mu=0; mu<4; mu++)
    fout << setw(14) << Pi_num[ipt][iphi][mu] * hbarC / (8.0 * particle->GetMass());
   for(int mu=0; mu<4; mu++)
    fout << setw(14) << - Pi_num_xi[ipt][iphi][mu] * hbarC / (8.0 * particle->GetMass());
   for(int mu=0; mu<4; mu++)
    fout << setw(14) << - Pi_num_navierstokes[ipt][iphi][mu] * hbarC / 2.0;
   fout << endl;
 }
 fout.close();
 char dim_file [200];
 strcpy(dim_file, out_file);
 strcat(dim_file, ".dim");
 ofstream fdim(dim_file);
 fdim << pT.size() << "  " << phi.size() << endl;
 fdim.close();
}

}  // end namespace gen
