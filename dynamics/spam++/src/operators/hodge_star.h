
#ifndef _HODGE_STARS_H_
#define _HODGE_STARS_H_

#include "common.h"
#include "geometry.h"


void YAKL_INLINE H(SArray<real,1,ndims> &var, SArray<real,2,ndims,1> const &velocity, SArray<real,2,ndims,1> const &Hgeom) {

  for (int d=0; d<ndims; d++) {
        var(d) = Hgeom(d,0) * velocity(d,0);
}
}

void YAKL_INLINE H(SArray<real,1,ndims> &var, SArray<real,2,ndims,3> const &velocity, SArray<real,2,ndims,3> const &Hgeom) {

  for (int d=0; d<ndims; d++) {
        var(d) = -1./24.* Hgeom(d,0) * velocity(d,0) + 26./24.* Hgeom(d,1) * velocity(d,1) - 1./24.* Hgeom(d,2) * velocity(d,2);
}
}

void YAKL_INLINE H(SArray<real,1,ndims> &var, SArray<real,2,ndims,5> const &velocity, SArray<real,2,ndims,5> const &Hgeom) {

  for (int d=0; d<ndims; d++) {
  var(d) = 9./1920.*Hgeom(d,0) * velocity(d,0) - 116./1920.*Hgeom(d,1) * velocity(d,1) + 2134./1920.* Hgeom(d,2) * velocity(d,2) - 116./1920.* Hgeom(d,3) * velocity(d,3) + 9./1920.* Hgeom(d,4) * velocity(d,4);
}
}


template<uint ndofs, uint ord, uint off=ord/2 -1> void YAKL_INLINE compute_H(SArray<real,1,ndims> &u, const real4d vvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,2,ndims,ord-1> v;
  SArray<real,2,ndims,ord-1> Hgeom;
  for (int p=0; p<ord-1; p++) {
  for (int d=0; d<ndims; d++) {
    if (d==0) {
    v(d,p) = vvar(d, k+ks, j+js, i+is+p-off);
    Hgeom(d,p) = dgeom.get_area_lform(ndims-1, d, k+ks, j+js, i+is+p-off) / pgeom.get_area_lform(1, d, k+ks, j+js, i+is+p-off);
  }
  if (d==1) {
  v(d,p) = vvar(d, k+ks, j+js+p-off, i+is);
  Hgeom(d,p) = dgeom.get_area_lform(ndims-1, d, k+ks, j+js+p-off, i+is) / pgeom.get_area_lform(1, d, k+ks, j+js+p-off, i+is);
  }
  }}
  H(u, v, Hgeom);

}

template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_H(real4d uvar, const real4d vvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndims> u;
  compute_H<ndofs, ord, off> (u, vvar, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int d=0; d<ndims; d++) { uvar(d, k+ks, j+js, i+is) = u(d);}}
  if (addmode == ADD_MODE::ADD) {for (int d=0; d<ndims; d++) { uvar(d, k+ks, j+js, i+is) += u(d);}}
}















//Note the indexing here, this is key
real YAKL_INLINE compute_Hv(const real4d wvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  // THIS IS 2ND ORDER AT BEST...
  return wvar(0, k+ks-1, j+js, i+is) * dgeom.get_area_10entity(k+ks, j+js, i+is) / pgeom.get_area_01entity(k+ks-1, j+js, i+is);
}

// Indexing issues since we go from p01 to d10, and d10 has an "extended boundary" ie boundary vert edges
// Since we index over d10, need to subtract 1 from k when indexing p01
// ie the kth edge flux corresponds with the k-1th edge velocity
// Also should be called with k=[1,...,ni-2] ie skip the first and last fluxes, which are set diagnostically (=0 for no-flux bcs)
template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_Hv(real4d uwvar, const real4d wvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  real uw = compute_Hv(wvar, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) { uwvar(0, k+ks, j+js, i+is) = uw;}
  if (addmode == ADD_MODE::ADD) { uwvar(0, k+ks, j+js, i+is) += uw;}
}
template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_Hv(SArray<real,1,1> &uwvar, const real4d wvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  real uw = compute_Hv(wvar, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) { uwvar(0) = uw;}
  if (addmode == ADD_MODE::ADD) { uwvar(0) += uw;}
}
  
  //BROKEN FOR 2D+1D EXT
  //MAINLY IN THE AREA CALCS...
  template<uint ndofs, uint ord, uint off=ord/2 -1> void YAKL_INLINE compute_Hext(SArray<real,1,ndims> &u, const real4d vvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
  {
    SArray<real,2,ndims,ord-1> v;
    SArray<real,2,ndims,ord-1> Hgeom;
    for (int p=0; p<ord-1; p++) {
    for (int d=0; d<ndims; d++) {
      if (d==0) {
      v(d,p) = vvar(d, k+ks, j+js, i+is+p-off);
      Hgeom(d,p) = dgeom.get_area_01entity(k+ks, j+js, i+is+p-off) / pgeom.get_area_10entity(k+ks, j+js, i+is+p-off);
    }
    if (d==1) {
    v(d,p) = vvar(d, k+ks, j+js+p-off, i+is);
    Hgeom(d,p) = dgeom.get_area_01entity(k+ks, j+js+p-off, i+is) / pgeom.get_area_10entity(k+ks, j+js+p-off, i+is);
    }
    }}
    H(u, v, Hgeom);
  }

// No indexing issues since we go from p10 to d01, and d01 has no "extended boundary"
template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_Hext(real4d uvar, const real4d vvar, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndims> u;
  compute_Hext<ndofs, ord, off> (u, vvar, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int d=0; d<ndims; d++) { uvar(d, k+ks, j+js, i+is) = u(d);}}
  if (addmode == ADD_MODE::ADD) {for (int d=0; d<ndims; d++) { uvar(d, k+ks, j+js, i+is) += u(d);}}
}












template<uint ndofs> void YAKL_INLINE I(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,1> const &dens, SArray<real,2,ndims,1> const &Igeom) {

  for (int l=0; l<ndofs; l++) {
        var(l) = Igeom(0,0) * dens(l,0,0);
        }
}

template<uint ndofs> void YAKL_INLINE I(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,3> const &dens, SArray<real,2,ndims,3> const &Igeom) {
  for (int l=0; l<ndofs; l++) {
    var(l) = Igeom(0,1) * dens(l,0,1);
    for (int d=0; d<ndims; d++) {
        var(l) += -1./48.* Igeom(d,0) * dens(l,d,0) + 2./48.* Igeom(d,1) * dens(l,d,1) - 1./48.* Igeom(d,2) * dens(l,d,2);
      }
    }
}

template<uint ndofs> void YAKL_INLINE I(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,5> const &dens, SArray<real,2,ndims,5> const &Igeom) {
  for (int l=0; l<ndofs; l++) {
    var(l) = Igeom(0,2) * dens(l,0,2);
    for (int d=0; d<ndims; d++) {
          var(l) += 1./576.*Igeom(d,0) * dens(l,d,0) - 16./576.*Igeom(d,1) * dens(l,d,1) + 30./576.* Igeom(d,2) * dens(l,d,2) - 16./576.* Igeom(d,3) * dens(l,d,3) + 1./576.* Igeom(d,4) * dens(l,d,4);
}
}
}

template<uint ndofs, uint ord, uint off=ord/2 -1> void YAKL_INLINE compute_I(SArray<real,1,ndofs> &x0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
SArray<real,3,ndofs,ndims,ord-1> x;
SArray<real,2,ndims,ord-1> Igeom;
for (int p=0; p<ord-1; p++) {
for (int l=0; l<ndofs; l++) {
for (int d=0; d<ndims; d++) {
  if (d==0)
  {
x(l,d,p) = var(l, k+ks, j+js, i+is+p-off);
Igeom(d,p) = pgeom.get_area_lform(0, 0, k+ks, j+js, i+is+p-off) / dgeom.get_area_lform(ndims, 0, k+ks, j+js, i+is+p-off);
}
if (d==1)
{
x(l,d,p) = var(l, k+ks, j+js+p-off, i+is);
Igeom(d,p) = pgeom.get_area_lform(0, 0, k+ks, j+js+p-off, i+is) / dgeom.get_area_lform(ndims, 0, k+ks, j+js+p-off, i+is);
}
}}}
I<ndofs>(x0, x, Igeom);
}

template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_I(real4d var0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndofs> x0;
  compute_I<ndofs, ord, off> (x0, var, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) = x0(l);}}
  if (addmode == ADD_MODE::ADD) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) += x0(l);}}

}







//BROKEN FOR 2D+1D EXT
//JUST IN THE AREA FORM CALCS...
template<uint ndofs, uint hord,  uint vord, uint hoff=hord/2 -1, uint voff=vord/2 -1> void YAKL_INLINE compute_Iext(SArray<real,1,ndofs> &x0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
SArray<real,3,ndofs,ndims,hord-1> x;
SArray<real,2,ndims,hord-1> Igeom;
for (int p=0; p<hord-1; p++) {
for (int l=0; l<ndofs; l++) {
for (int d=0; d<ndims; d++) {
  if (d==0)
  {
x(l,d,p) = var(l, k+ks, j+js, i+is+p-hoff);
Igeom(d,p) = pgeom.get_area_00entity(k+ks, j+js, i+is+p-hoff) / dgeom.get_area_11entity(k+ks, j+js, i+is+p-hoff);
}
if (d==1)
{
x(l,d,p) = var(l, k+ks, j+js+p-hoff, i+is);
Igeom(d,p) = pgeom.get_area_00entity(k+ks, j+js+p-hoff, i+is) / dgeom.get_area_11entity(k+ks, j+js+p-hoff, i+is);
}
}}}
I<ndofs>(x0, x, Igeom);
//EVENTUALLY BE MORE CLEVER IN THE VERTICAL HERE
// BUT THIS IS 2nd ORDER RIGHT NOW!
}

//Indexing here is fine, since we going from d11 to p00 and there is no "extended" boundary in p00
template<uint ndofs, uint hord,  uint vord, ADD_MODE addmode=ADD_MODE::REPLACE, uint hoff=hord/2 -1, uint voff=vord/2 -1> void YAKL_INLINE compute_Iext(real4d var0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndofs> x0;
  compute_Iext<ndofs, hord, vord, hoff, voff> (x0, var, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) = x0(l);}}
  if (addmode == ADD_MODE::ADD) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) += x0(l);}}
}











template<uint ndofs> void YAKL_INLINE J(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,1> const &dens, SArray<real,2,ndims,1> const &Jgeom) {

  for (int l=0; l<ndofs; l++) {
        var(l) = Jgeom(0,0) * dens(l,0,0);
        }
}

template<uint ndofs> void YAKL_INLINE J(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,3> const &dens, SArray<real,2,ndims,3> const &Jgeom) {
  for (int l=0; l<ndofs; l++) {
    var(l) = Jgeom(0,1) * dens(l,0,1);
    for (int d=0; d<ndims; d++) {
        var(l) += -1./48.* Jgeom(d,0) * dens(l,d,0) + 2./48.* Jgeom(d,1) * dens(l,d,1) - 1./48.* Jgeom(d,2) * dens(l,d,2);
      }
    }
}

template<uint ndofs> void YAKL_INLINE J(SArray<real,1,ndofs> &var, SArray<real,3,ndofs,ndims,5> const &dens, SArray<real,2,ndims,5> const &Jgeom) {
  for (int l=0; l<ndofs; l++) {
    var(l) = Jgeom(0,2) * dens(l,0,2);
    for (int d=0; d<ndims; d++) {
          var(l) += 1./576.*Jgeom(d,0) * dens(l,d,0) - 16./576.*Jgeom(d,1) * dens(l,d,1) + 30./576.* Jgeom(d,2) * dens(l,d,2) - 16./576.* Jgeom(d,3) * dens(l,d,3) + 1./576.* Jgeom(d,4) * dens(l,d,4);
}
}
}


template<uint ndofs, uint ord, uint off=ord/2 -1> void YAKL_INLINE compute_J(SArray<real,1,ndofs> &x0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
SArray<real,3,ndofs,ndims,ord-1> x;
SArray<real,2,ndims,ord-1> Jgeom;
for (int p=0; p<ord-1; p++) {
for (int l=0; l<ndofs; l++) {
for (int d=0; d<ndims; d++) {
  if (d==0)
  {
x(l,d,p) = var(l, k+ks, j+js, i+is+p-off);
Jgeom(d,p) = dgeom.get_area_lform(0, 0, k+ks, j+js, i+is+p-off) / pgeom.get_area_lform(ndims, 0, k+ks, j+js, i+is+p-off);
}
if (d==1)
{
x(l,d,p) = var(l, k+ks, j+js+p-off, i+is);
Jgeom(d,p) = dgeom.get_area_lform(0, 0, k+ks, j+js+p-off, i+is) / pgeom.get_area_lform(ndims, 0, k+ks, j+js+p-off, i+is);
}
}}}
J<ndofs>(x0, x, Jgeom);
}

template<uint ndofs, uint ord, ADD_MODE addmode=ADD_MODE::REPLACE, uint off=ord/2 -1> void YAKL_INLINE compute_J(real4d var0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndofs> x0;
  compute_J<ndofs,ord,off> (x0, var, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) = x0(l);}}
  if (addmode == ADD_MODE::ADD) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) += x0(l);}}
}













//BROKEN FOR 2D+1D EXT
//JUST IN THE AREA CALCS...
template<uint ndofs, uint hord, uint vord, uint hoff=hord/2 -1, uint voff=vord/2 -1> void YAKL_INLINE compute_Jext(SArray<real,1,ndofs> &x0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
SArray<real,3,ndofs,ndims,hord-1> x;
SArray<real,2,ndims,hord-1> Jgeom;
for (int p=0; p<hord-1; p++) {
for (int l=0; l<ndofs; l++) {
for (int d=0; d<ndims; d++) {
  if (d==0)
  {
x(l,d,p) = var(l, k+ks-1, j+js, i+is+p-hoff);
Jgeom(d,p) = dgeom.get_area_00entity(k+ks, j+js, i+is+p-hoff) / pgeom.get_area_11entity(k+ks-1, j+js, i+is+p-hoff);
}
if (d==1)
{
x(l,d,p) = var(l, k+ks+1, j+js+p-hoff, i+is);
Jgeom(d,p) = dgeom.get_area_00entity(k+ks, j+js+p-hoff, i+is) / pgeom.get_area_11entity(k+ks-1, j+js+p-hoff, i+is);
}
}}}
J<ndofs>(x0, x, Jgeom);
//EVENTUALLY BE MORE CLEVER IN THE VERTICAL HERE
// BUT THIS IS 2nd ORDER RIGHT NOW!
}

//Indexing is tricky, since we going from p11 to d00 and there is an "extended" boundary in d00
//Since the i,j,k here are for d00, we must subtract 1 from k when indexing p11 quantities
// ie the kth dual vertex corresponds with the k-1th primal cell
// Also, should just compute over k=[1,..,ni-2] ie skip the first and last vertices!
// These values should be set as boundary conditions... = 0 for no flux I think!
template<uint ndofs, uint hord,  uint vord, ADD_MODE addmode=ADD_MODE::REPLACE, uint hoff=hord/2 -1, uint voff=vord/2 -1> void YAKL_INLINE compute_Jext(real4d var0, const real4d var, Geometry<1,1,1> &pgeom, Geometry<1,1,1> &dgeom, int is, int js, int ks, int i, int j, int k)
{
  SArray<real,1,ndofs> x0;
  compute_Jext<ndofs,hord,vord,hoff,voff> (x0, var, pgeom, dgeom, is, js, ks, i, j, k);
  if (addmode == ADD_MODE::REPLACE) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) = x0(l);}}
  if (addmode == ADD_MODE::ADD) {for (int l=0; l<ndofs; l++) {var0(l, k+ks, j+js, i+is) += x0(l);}}
}
#endif