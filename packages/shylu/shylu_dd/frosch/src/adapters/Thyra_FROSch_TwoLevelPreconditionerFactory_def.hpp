#ifndef THYRA_FROSCH_TWOLEVELPRECONDITIONER_FACTORY_DEF_HPP
#define THYRA_FROSCH_TWOLEVELPRECONDITIONER_FACTORY_DEF_HPP

#include "Thyra_FROSch_TwoLevelPreconditionerFactory_decl.hpp"

#include <Frosch_EpetraOp_def.hpp>
#include <Teuchos_XMLParameterListCoreHelpers.hpp>


#include <FROSch_Tools_def.hpp>
#include <FROSch_TwoLevelPreconditioner_def.hpp>


namespace Thyra {
    using Teuchos::RCP;
    using Teuchos::rcp;
    using Teuchos::ParameterList;
    
    //Constructor
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::FROSch_TwoLevelPreconditionerFactory()
    {
        paramList_ = rcp(new Teuchos::ParameterList());
    }
//-----------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    bool FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::isCompatible(const LinearOpSourceBase<Scalar>& fwdOpSrc) const
    {
        const RCP<const LinearOpBase<Scalar> > fwdOp = fwdOpSrc.getOp();
        //so far only Epetra is allowed
        if (Xpetra::ThyraUtils<Scalar,LocalOrdinal,GlobalOrdinal,Node>::isEpetra(fwdOp)) return true;
        
        return false;
    }
//--------------------------------------------------------------
    template<class Scalar, class LocalOrdinal, class GlobalOrdinal , class Node>
    RCP<PreconditionerBase<Scalar> >FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::createPrec() const{
        return Teuchos::rcp(new DefaultPreconditioner<Scalar>);
        
    }
//-------------------------------------------------------------
    template<class Scalar, class LocalOrdinal , class GlobalOrdinal, class Node>
    void FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::initializePrec
    (const Teuchos::RCP<const LinearOpSourceBase<Scalar> >& fwdOpSrc,
    PreconditionerBase<Scalar>* prec,
    const ESupportSolveUse supportSolveUse
    ) const{
        
        Teuchos::RCP<Teuchos::FancyOStream> fancy = fancyOStream(Teuchos::rcpFromRef(std::cout));
        
        
        using Teuchos::rcp_dynamic_cast;
        //Some Typedefs
        typedef Xpetra::Map<LocalOrdinal,GlobalOrdinal,Node>                     XpMap;
        typedef Xpetra::Operator<Scalar, LocalOrdinal, GlobalOrdinal, Node>      XpOp;
        typedef Xpetra::ThyraUtils<Scalar,LocalOrdinal,GlobalOrdinal,Node>       XpThyUtils;
        typedef Xpetra::CrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node>        XpCrsMat;
        typedef Xpetra::BlockedCrsMatrix<Scalar,LocalOrdinal,GlobalOrdinal,Node> XpBlockedCrsMat;
        typedef Xpetra::Matrix<Scalar,LocalOrdinal,GlobalOrdinal,Node>           XpMat;
        typedef Xpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node>      XpMultVec;
        typedef Xpetra::MultiVector<double,LocalOrdinal,GlobalOrdinal,Node>      XpMultVecDouble;
        typedef Thyra::LinearOpBase<Scalar>                                      ThyLinOpBase;
        typedef Thyra::EpetraLinearOp                                         ThyEpLinOp;

        
        
        
        
        
        //PreCheck
        TEUCHOS_ASSERT(Teuchos::nonnull(fwdOpSrc));
        //TEUCHOS_ASSERT(this->isCompatible(*fwdOpSrc));
        TEUCHOS_ASSERT(prec);
        
        // Create a copy, as we may remove some things from the list
        RCP<ParameterList> paramList(new ParameterList(*paramList_)); // AH: Muessen wir diese Kopie machen? Irgendwie wäre es doch besser, wenn man die nicht kopieren müsste, oder?
        
        // Retrieve wrapped concrete Xpetra matrix from FwdOp
        const RCP<const ThyLinOpBase> fwdOp = fwdOpSrc->getOp();
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(fwdOp));
        
        // Check whether it is Epetra/Tpetra
        bool bIsEpetra  = XpThyUtils::isEpetra(fwdOp);
        bool bIsTpetra  = XpThyUtils::isTpetra(fwdOp);
        bool bIsBlocked = XpThyUtils::isBlockedOperator(fwdOp);
        TEUCHOS_TEST_FOR_EXCEPT((bIsEpetra == true  && bIsTpetra == true));
        TEUCHOS_TEST_FOR_EXCEPT((bIsEpetra == bIsTpetra) && bIsBlocked == false);
        TEUCHOS_TEST_FOR_EXCEPT((bIsEpetra != bIsTpetra) && bIsBlocked == true);
        
        RCP<XpMat> A = Teuchos::null;
        RCP<const XpCrsMat > xpetraFwdCrsMat = XpThyUtils::toXpetra(fwdOp);
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(xpetraFwdCrsMat));
        
        // FROSCH needs a non-const object as input
        RCP<XpCrsMat> xpetraFwdCrsMatNonConst = Teuchos::rcp_const_cast<XpCrsMat>(xpetraFwdCrsMat);
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(xpetraFwdCrsMatNonConst));
        
        // wrap the forward operator as an Xpetra::Matrix that FROSch can work with
        A = rcp(new Xpetra::CrsMatrixWrap<Scalar,LocalOrdinal,GlobalOrdinal,Node>(xpetraFwdCrsMatNonConst));
        //A->describe(*fancy,Teuchos::VERB_EXTREME);
        
        
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(A));
        
        // Retrieve concrete preconditioner object--->Here Mem Leak?
        const Teuchos::Ptr<DefaultPreconditioner<Scalar> > defaultPrec = Teuchos::ptr(dynamic_cast<DefaultPreconditioner<Scalar> *>(prec));
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(defaultPrec));
        
        // extract preconditioner operator
        RCP<ThyLinOpBase> thyra_precOp = Teuchos::null;
        thyra_precOp = rcp_dynamic_cast<Thyra::LinearOpBase<Scalar> >(defaultPrec->getNonconstUnspecifiedPrecOp(), true);
        
        //Later needs to be a utility ExtractCoordinatesFromParameterList
        //not implemented yet
        // FROSCH::Tools<SC,LO,GO,Node>::ExtractCoordinatesFromParameterList(paramList);
        
        //-------Build New Two Level Prec--------------
       RCP<FROSch::TwoLevelPreconditioner<Scalar,LocalOrdinal,GlobalOrdinal,Node> > TwoLevelPrec (new FROSch::TwoLevelPreconditioner<Scalar,LocalOrdinal,GlobalOrdinal,Node>(A,paramList));
        
        
        RCP< const Teuchos::Comm< int > > Comm = A->getRowMap()->getComm();
        Comm->barrier();
        /*if(Comm->getRank() ==0){std::cout<<"Create Epetra Op new Constructor\n";
            
            cout << "--------------------------------------------------------------------------------\nPARAMETERS Two Level Precon Factory:" << endl;
            paramList.print(std::cout);
            cout << "--------------------------------------------------------------------------------\n\n";
            
        }*/
        Teuchos::RCP<Xpetra::MultiVector<Scalar,LocalOrdinal,GlobalOrdinal,Node> > coord = Teuchos::null;
        Teuchos::RCP<Xpetra::Map<LocalOrdinal,GlobalOrdinal,Node> > RepeatedMap =  Teuchos::null;

        if(paramList->isParameter("Coordinates")){
            coord = FROSch::ExtractCoordinatesFromParameterList<Scalar,LocalOrdinal,GlobalOrdinal,Node>(*paramList);
            coord->describe(*fancy,Teuchos::VERB_EXTREME);
            
        }
       
        if(paramList->isParameter("RepeatedMap")){
            
            RepeatedMap = FROSch::ExtractRepeatedMapFromParameterList<LocalOrdinal,GlobalOrdinal,Node>(*paramList);
            RepeatedMap->describe(*fancy,Teuchos::VERB_EXTREME);
            
        }
        
        if(coord.get()!=NULL && RepeatedMap.get()!=NULL){
            
            
            TwoLevelPrec->initialize(paramList->get("Dimension",1),paramList->get("Overlap",1),RepeatedMap,paramList->get("DofsPerNode",1),FROSch::NodeWise,coord);
            

        }else{
            TwoLevelPrec->initialize();
        }
       
        
        TwoLevelPrec->compute();
        //-----------------------------------------------
        //Prepare for FROSch Epetra Op-------------------
        RCP<FROSch::TwoLevelPreconditioner<double,int,int,Xpetra::EpetraNode> > epetraTwoLevel = rcp_dynamic_cast<FROSch::TwoLevelPreconditioner<double,int,int,Xpetra::EpetraNode> >(TwoLevelPrec);
        
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(epetraTwoLevel));
        
        
        RCP<FROSch::FROSch_EpetraOperator> frosch_epetraop = rcp(new FROSch::FROSch_EpetraOperator(epetraTwoLevel,paramList));
        
        Comm->barrier();
        /*if(Comm->getRank() ==0){std::cout<<"Create Epetra Op new Constructor\n";
            
                cout << "--------------------------------------------------------------------------------\nPARAMETERS Two Level Precon Factory:" << endl;
            paramList.print(std::cout);
                cout << "--------------------------------------------------------------------------------\n\n";
            
        }*/
        Comm->barrier();Comm->barrier();Comm->barrier();
        

       // RCP<FROSch::FROSch_EpetraOperator> frosch_epetraop(new FROSch::FROSch_EpetraOperator(A,rcpFromRef(paramList)))
        ;
        
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(frosch_epetraop));
        
        //attach to fwdOp
        set_extra_data(fwdOp,"IFPF::fwdOp", Teuchos::inOutArg(frosch_epetraop), Teuchos::POST_DESTROY,false);
        
        RCP<ThyEpLinOp> thyra_epetraOp = Thyra::nonconstEpetraLinearOp(frosch_epetraop, NOTRANS, EPETRA_OP_APPLY_APPLY_INVERSE, EPETRA_OP_ADJOINT_UNSUPPORTED);
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thyra_epetraOp));
        //thyra_epetraOp->describe(*fancy,Teuchos::VERB_EXTREME);
        //Wrap tp thyra
        RCP<ThyLinOpBase > thyraPrecOp = Teuchos::null;
        
        
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::nonnull(thyraPrecOp));
        
        thyraPrecOp = rcp_dynamic_cast<ThyLinOpBase>(thyra_epetraOp);
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(thyraPrecOp));

        defaultPrec->initializeUnspecified(thyraPrecOp);
       
        
    }
//-------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    void FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::
    uninitializePrec(PreconditionerBase<Scalar>* prec, RCP<const LinearOpSourceBase<Scalar> >* fwdOp, ESupportSolveUse* supportSolveUse) const {
        TEUCHOS_ASSERT(prec);
        
        // Retrieve concrete preconditioner object
        const Teuchos::Ptr<DefaultPreconditioner<Scalar> > defaultPrec = Teuchos::ptr(dynamic_cast<DefaultPreconditioner<Scalar> *>(prec));
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(defaultPrec));
        
        if (fwdOp) {
            // TODO: Implement properly instead of returning default value
            *fwdOp = Teuchos::null;
        }
        
        if (supportSolveUse) {
            // TODO: Implement properly instead of returning default value
            *supportSolveUse = Thyra::SUPPORT_SOLVE_UNSPECIFIED;
        }
        
        defaultPrec->uninitialize();
    }
//-----------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    void FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::setParameterList(RCP<ParameterList> const & paramList){
        TEUCHOS_TEST_FOR_EXCEPT(Teuchos::is_null(paramList));
        paramList_ = paramList;
    }
    
//------------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    RCP<ParameterList> FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::getNonconstParameterList(){
        return paramList_;
    }

//-----------------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
   RCP<const ParameterList>
    FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::getParameterList() const {
        return paramList_;
    }
//--------------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    RCP<const ParameterList> FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::getValidParameters() const {
        static RCP<const ParameterList> validPL;
        
        if (Teuchos::is_null(validPL))
            validPL = rcp(new ParameterList());
        
        return validPL;
    }
//-----------------------------------------------------------------------
    template <class Scalar, class LocalOrdinal, class GlobalOrdinal, class Node>
    std::string FROSch_TwoLevelPreconditionerFactory<Scalar,LocalOrdinal,GlobalOrdinal,Node>::description() const {
        return "Thyra::FROSch_TwoLevelPreconditionerFactory";
    }
//--------------------------------------------------------------------------
    template<class Scalar, class LocalOrdinal,class GlobalOrdinal, class Node>
    RCP<ParameterList> FROSch_TwoLevelPreconditionerFactory<Scalar, LocalOrdinal,GlobalOrdinal,Node>::unsetParameterList(){
        RCP<ParameterList> savedParamList = paramList_;
        paramList_ = Teuchos::null;
        return savedParamList;
        
    }
    
}
#endif

