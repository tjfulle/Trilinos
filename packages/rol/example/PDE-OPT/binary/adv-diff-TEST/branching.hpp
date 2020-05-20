// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

#ifndef ROL_PDEOPT_BRANCHING_PEBBL_H
#define ROL_PDEOPT_BRANCHING_PEBBL_H

#include "ROL_PEBBL_Interface.hpp"
#include "../../TOOLS/pdevector.hpp"
#include "hilbert.hpp"

template<class Real>
class ADVDIFF_BranchSub;

template<class Real>
class ADVDIFF_Branching : public ROL::ROL_PEBBL_Branching<Real> {
private:
  const int method_;
  const bool useTpetra_;
  ROL::Ptr<ROL::Vector<Real>> z0_;

  using ROL::ROL_PEBBL_Branching<Real>::verbosity_;
  using ROL::ROL_PEBBL_Branching<Real>::outStream_;
  using ROL::ROL_PEBBL_Branching<Real>::parlist_;

public:
  ADVDIFF_Branching(const ROL::Ptr<BinaryAdvDiffFactory<Real>>            &factory,
                    const ROL::Ptr<ROL::ParameterList>                    &parlist,
                    const ROL::Ptr<ROL::BranchHelper_PEBBL<Real>>         &bHelper,
                    int                                                    verbosity = 0,
                    const ROL::Ptr<std::ostream>                          &outStream = ROL::nullPtr,
                    int                                                    method = 0)
    : ROL::ROL_PEBBL_Branching<Real>::ROL_PEBBL_Branching(factory,parlist,bHelper,verbosity,outStream),
      method_(method), useTpetra_(factory->controlType()) {
    z0_ = factory->buildSolutionVector();
  }

  pebbl::branchSub* blankSub() {
    return new ADVDIFF_BranchSub<Real>(useTpetra_,ROL::makePtrFromRef<ADVDIFF_Branching<Real>>(*this),verbosity_,outStream_,method_);
  }

//  pebbl::solution* iniitalGuess() {
//
//  }
}; // ADVDIFF_Branching

template <class Real>
class ADVDIFF_BranchSub : public ROL::ROL_PEBBL_BranchSub<Real> {
private:
  const bool useTpetra_;
  const int method_;
  std::string methodName_;

  using ROL::ROL_PEBBL_BranchSub<Real>::anyChild;
  using ROL::ROL_PEBBL_BranchSub<Real>::index_;
  using ROL::ROL_PEBBL_BranchSub<Real>::branching_;
  using ROL::ROL_PEBBL_BranchSub<Real>::problem0_;
  using ROL::ROL_PEBBL_BranchSub<Real>::solution_;
  using ROL::ROL_PEBBL_BranchSub<Real>::rndSolution_;
  using ROL::ROL_PEBBL_BranchSub<Real>::verbosity_;
  using ROL::ROL_PEBBL_BranchSub<Real>::outStream_;

  void round(ROL::Vector<Real> &rx, const ROL::Vector<Real> &x, Real t) const {
    rx.set(x);
    Teuchos::ArrayView<Real> data = getData(rx);
    Real val(0);
    for (auto it = data.begin(); it != data.end(); ++it) {
      val = *it;
      *it = (val < t ? std::floor(val) : std::ceil(val));
    }
  }

  Teuchos::ArrayView<Real> getData(ROL::Vector<Real> &x) const {
    if (useTpetra_) {
      try {
        return (dynamic_cast<ROL::TpetraMultiVector<Real>&>(x).getVector()->getDataNonConst(0))();
      }
      catch (std::exception &e) {
        return (dynamic_cast<PDE_OptVector<Real>&>(x).getField()->getVector()->getDataNonConst(0))();
      }
    }
    else {
      int size = x.dimension();
      try {
        return Teuchos::ArrayView<Real>(dynamic_cast<ROL::StdVector<Real>&>(x).getVector()->data(),size);
      }
      catch (std::exception &e) {
        return Teuchos::ArrayView<Real>(dynamic_cast<PDE_OptVector<Real>&>(x).getParameter()->getVector()->data(),size);
      }
    }
  }

public:
  ADVDIFF_BranchSub(bool useTpetra,
                    const ROL::Ptr<ROL::ROL_PEBBL_Branching<Real>> &branching,
                    int verbosity = 0,
                    const ROL::Ptr<std::ostream> &outStream = ROL::nullPtr,
                    int method = 0)
    : ROL::ROL_PEBBL_BranchSub<Real>(branching, verbosity, outStream), useTpetra_(useTpetra), method_(method) {
    switch (method_) {
      case 1:  methodName_ = "Mass Preserving Rounding"; break;
      case 2:  methodName_ = "Objective Gap Rounding";   break;
      case 3:  methodName_ = "Sum Up Rounding";          break;
      default: methodName_ = "Naive Rounding";
    }
  }

  ADVDIFF_BranchSub(const ADVDIFF_BranchSub &rpbs)
    : ROL::ROL_PEBBL_BranchSub<Real>(rpbs), useTpetra_(rpbs.useTpetra_),
      method_(rpbs.method_), methodName_(rpbs.methodName_) {}

  void incumbentHeuristic() {
    Real tol = std::sqrt(ROL::ROL_EPSILON<Real>());
    Real t(0), val(0);
    if (method_==1) { // mass preserving rounding
      const Real zero(0), one(1);
      rndSolution_->set(*solution_);
      Teuchos::ArrayView<Real> data = getData(*rndSolution_);
      std::map<Real,size_t> sdata;
      Real sum(0);
      for (size_t i = 0; i < static_cast<size_t>(data.size()); ++i) {
        sdata.insert(std::pair<Real,size_t>(data[i],i));
        sum += data[i];
      }
      int rsum = static_cast<int>(std::round(sum)), psum(0);
      for (auto it = sdata.rbegin(); it != sdata.rend(); ++it) {
        data[it->second] = (psum < rsum ? one : zero);
        t = (psum < rsum ? it->first : t);
        psum++;
      }
    }
    else if (method_==2) { // objective gap rounding
      const Real invphi(0.5*(std::sqrt(5.0)-1.0));
      const Real invphi2(0.5*(3.0-std::sqrt(5.0)));
      const Real itol(std::sqrt(ROL::ROL_EPSILON<Real>()));
      Real a(0), b(1), c(0), d(1), h(b-a), fc(0), fd(0);
      // Evaluate at c
      c = a + invphi2 * h;
      round(*rndSolution_,*solution_,c);
      problem0_->getObjective()->update(*rndSolution_,ROL::UPDATE_TEMP);
      fc = problem0_->getObjective()->value(*rndSolution_,tol);
      // Evaluate at d
      d = a + invphi * h;
      round(*rndSolution_,*solution_,d);
      problem0_->getObjective()->update(*rndSolution_,ROL::UPDATE_TEMP);
      fd = problem0_->getObjective()->value(*rndSolution_,tol);
      while (std::abs(c-d) > itol) {
        h *= invphi;
        if (fc < fd) {
          b  = d;
          d  = c;
          fd = fc;
          c  = a + invphi2 * h;
          round(*rndSolution_,*solution_,c);
          problem0_->getObjective()->update(*rndSolution_,ROL::UPDATE_TEMP);
          fc = problem0_->getObjective()->value(*rndSolution_,tol);
        }
        else {
          a  = c;
          c  = d;
          fc = fd;
          d  = a + invphi * h;
          round(*rndSolution_,*solution_,d);
          problem0_->getObjective()->update(*rndSolution_,ROL::UPDATE_TEMP);
          fd = problem0_->getObjective()->value(*rndSolution_,tol);
        }
      }
      t = static_cast<Real>(0.5)*(a+b);
      round(*rndSolution_,*solution_,t);
    }
    else if (method_==3) { // Sum up rounding
      const Real zero(0), one(1);
      rndSolution_->set(*solution_);
      Teuchos::ArrayView<Real> data = getData(*rndSolution_);
      const int n = data.size();
      const int m = (std::log2(n)-1)/2;
      Real g1(0), g2(0);
      if (n == std::pow(2,2*m+1)) { // Use Hilbert curves if n = 2^{2m+1}
        const int nx = std::pow(2,m+1);
        const int ny = std::pow(2,m);
        int x(0), y(0), ind(0);
        // Domain [0,1]x[0,1]
        for (int j = 0; j < 2; ++j) {
          for (int i = 0; i < ny*ny; ++i) {
            hilbert::d2xy(m,i,x,y);
            ind = x + y*nx + j*ny;
            g1 += data[ind];
            g2 += one - data[ind];
            if (g1 >= g2) {
              data[ind] = one;
              g1 -= one;
            }
            else {
              data[ind] = zero;
              g2 -= one;
            }
          }
        }
      }
      else { // Otherwise use native ordering of cells
        for (int i = 0; i < n; ++i) {
          g1 += data[i];
          g2 += one - data[i];
          if (g1 >= g2) {
            data[i] = one;
            g1 -= one;
          }
          else {
            data[i] = zero;
            g2 -= one;
          }
        }
      }
    }
    else { // naive rounding
      t = static_cast<Real>(0.5);
      round(*rndSolution_,*solution_,t);
    }
    problem0_->getObjective()->update(*rndSolution_,ROL::UPDATE_TEMP);
    val = problem0_->getObjective()->value(*rndSolution_,tol);
    branching_->foundSolution(new ROL::ROL_PEBBL_Solution<Real>(*rndSolution_,val));
    if (verbosity_ > 0) {
      *outStream_ << "ADVDIFF_BranchSub::incumbentHeuristic: " << methodName_ << std::endl;
      *outStream_ << "  Incumbent Value:    " << val  << std::endl;
      *outStream_ << "  Rounding Threshold: " << t    << std::endl;
    }
  }

  pebbl::branchSub* makeChild(int whichChild = anyChild) override {
    if (whichChild == anyChild) {
      throw ROL::Exception::NotImplemented(">>> ROL_PEBBL_BranchSub::makeChild: whichChild is equal to anyChild!");
    }
    ADVDIFF_BranchSub<Real>* child
      = new ADVDIFF_BranchSub<Real>(*this);
    child->updateFixed(index_,
      (whichChild==0 ? static_cast<Real>(1) : static_cast<Real>(0)));
    return child;
  }

}; // class ADVDIFF_BranchSub

#endif
